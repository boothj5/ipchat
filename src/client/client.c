#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#endif
#include <fcntl.h>
#include <glib.h>

#include "proto/proto.h"

int
main(int argc, char *argv[])
{
    char *hostname = NULL;
    char *nickname = NULL;
    int port = 0;
    char ip[100];
    struct hostent *he = NULL;
    struct in_addr **addr_list;
    gboolean ip_resolved = FALSE;

    GOptionEntry entries[] =
    {
        { "host", 'h', 0, G_OPTION_ARG_STRING, &hostname, "Server hostname or IP address", NULL },
        { "port", 'p', 0, G_OPTION_ARG_INT, &port, "Listen port", NULL },
        { "nick", 'n', 0, G_OPTION_ARG_STRING, &nickname, "Nickname", NULL },
        { NULL }
    };

    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new(NULL);
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("%s\n", error->message);
        g_option_context_free(context);
        g_error_free(error);
        return 1;
    }

    g_option_context_free(context);

    printf("\n");

    if (hostname == NULL) {
        printf("Use -h to provide a hostname or ip address.\n");
        return 1;
    }

    if (nickname == NULL) {
        printf("Use -n to provide a nickname.\n");
        return 1;
    }

    if (port == 0) {
        port = 6660;
    }

    // parse ip address
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, hostname, &(sa.sin_addr));

    // valid ip address
    if (result != 0) {
        strncpy(ip, hostname, strlen(hostname));
        ip[strlen(hostname)] = '\0';
        ip_resolved = TRUE;

    // invalid ip address, attempt to resolve hostname
    } else {
        he = gethostbyname(hostname);

        // failed to resolve hostname
        if (he == NULL) {
            switch(h_errno)
            {
            case HOST_NOT_FOUND:
                printf("%s: Unknown host\n", hostname);
                break;
            case NO_ADDRESS:
                printf("%s: No ip address found\n", hostname);
                break;
            case NO_RECOVERY:
                printf("%s: Non recoverable name server request\n", hostname);
                break;
            case TRY_AGAIN:
                printf("%s: Temporary error occurred at name server\n", hostname);
                break;
            default:
                printf("%s: Unknown error occurred getting ip address\n", hostname);
                break;
            }
            return 1;
        }
    }

    initscr();
    cbreak();
    keypad(stdscr, TRUE);

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    WINDOW *outw = newwin(rows - 2, cols, 0, 0);
    scrollok(outw, TRUE);
    WINDOW *inpw = newwin(2, cols, rows - 2, 0);
    scrollok(inpw, TRUE);
    wtimeout(inpw, 20);

    if (ip_resolved == FALSE) {
        // get ip address
        wprintw(outw, "Host %s resolved to:\n", hostname);
        wrefresh(outw);
        addr_list = (struct in_addr**)he->h_addr_list;
        int i;
        for (i = 0; addr_list[i] != NULL; i++) {
            wprintw(outw, "  %s\n", inet_ntoa(*addr_list[i]));
            wrefresh(outw);
            if (i == 0) {
                strcpy(ip, inet_ntoa(*addr_list[i]));
            }
        }
    }

    wprintw(outw, "Connecting to %s:%d...\n", ip, port);

    int srv_socket;
    srv_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // ipv4, tcp, ip
    if (srv_socket == -1) {
        endwin();
        printf("Failed to create socket.\n");
        return 1;
    }

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_family = AF_INET; // ipv4
    server.sin_port = htons(port); // host to network byte order

    if (connect(srv_socket, (struct sockaddr*)&server, sizeof(server)) < 0) {
        endwin();
        printf("Failed to connect.\n");
        return 1;
    }

    wprintw(outw, "Connected successfully.\n");
    wrefresh(outw);

    // register
    GString *reg_msg = g_string_new(nickname);
    g_string_append(reg_msg, STR_MESSAGE_END);
    int sent = 0;
    int to_send = reg_msg->len;
    char *marker = reg_msg->str;
    while (to_send > 0 && ((sent = write(srv_socket, marker, to_send)) > 0)) {
        to_send -= sent;
        marker += sent;
    }
    g_string_free(reg_msg, TRUE);

    // main loop
    char input[1000];
    int len = 0;
    while (1) {
        int ch = wgetch(inpw);
        if (ch != ERR) {

            // handle input
            if (ch == '\n') {
                input[len] = '\0';

                // quit
                if (strcmp(input, "/quit") == 0) {
                    char *end_session_msg = STR_SESSION_END;
                    int sent = 0;
                    int to_send = strlen(end_session_msg);
                    char *marker = end_session_msg;
                    while (to_send > 0 && ((sent = write(srv_socket, marker, to_send)) > 0)) {
                        to_send -= sent;
                        marker += sent;
                    }
                    break;

                // send message
                } else if (strlen(input) > 0) {
                    GString *terminated_msg = g_string_new("");
                    g_string_append_printf(terminated_msg, "%s%s", input, STR_MESSAGE_END);
                    wrefresh(outw);
                    int sent = 0;
                    int to_send = terminated_msg->len;
                    char *marker = terminated_msg->str;
                    while (to_send > 0 && ((sent = write(srv_socket, marker, to_send)) > 0)) {
                        to_send -= sent;
                        marker += sent;
                    }
                    g_string_free(terminated_msg, TRUE);
                }
                wclear(inpw);
                len = 0;
            } else {
                input[len++] = ch;
            }
        }

        // check for incoming messages from server
        int read_size = 0;
        char buf[2];
        memset(buf, 0, sizeof(buf));
        gboolean term = FALSE;
        GString *stream = g_string_new("");
        while (!term && ((read_size = recv(srv_socket, buf, 1, MSG_DONTWAIT)) > 0)) {
            g_string_append_len(stream, buf, read_size);
            if (g_str_has_suffix(stream->str, STR_MESSAGE_END)) term = TRUE;
            memset(buf, 0, sizeof(buf));
        }
        if (term) {
            char *incoming = malloc(stream->len -  5);
            strncpy(incoming, stream->str, stream->len - 6);
            incoming[stream->len - 6] = '\0';

            wprintw(outw, "%s\n", incoming);
            wrefresh(outw);
            free(incoming);
        }

        g_string_free(stream, TRUE);

        wrefresh(inpw);
    }

    close(srv_socket);
    endwin();

    return 0;
}
