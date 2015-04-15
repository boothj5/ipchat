#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <ncurses.h>
#include <fcntl.h>
#include <glib.h>

int
main(int argc, char *argv[])
{
    char *hostname = NULL;
    int port = 0;
    char ip[100];
    struct hostent *he = NULL;
    struct in_addr **addr_list;
    int connected = 0;
    gboolean ip_resolved = FALSE;

    GOptionEntry entries[] =
    {
        { "host", 'h', 0, G_OPTION_ARG_STRING, &hostname, "Server hostname or IP address", NULL },
        { "port", 'p', 0, G_OPTION_ARG_INT, &port, "Listen port", NULL },
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
    WINDOW *outw = newwin(rows/2, cols, 0, 0);
    scrollok(outw, TRUE);
    WINDOW *inpw = newwin(rows/2-1, cols, rows/2, 0);
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

    int socket_desc;
    socket_desc = socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // ipv4, tcp, ip
    if (socket_desc == -1) {
        wprintw(outw, "Failed to create socket.\n");
        wrefresh(outw);
    } else {
        struct sockaddr_in server;
        server.sin_addr.s_addr = inet_addr(ip);
        server.sin_family = AF_INET; // ipv4
        server.sin_port = htons(port); // host to network byte order

        if (connect(socket_desc, (struct sockaddr*)&server, sizeof(server)) < 0) {
            wprintw(outw, "Failed to connect.\n");
            wrefresh(outw);
        } else {
            wprintw(outw, "Connected successfully.\n");
            wrefresh(outw);
            connected = 1;
        }

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
                        break;

                    // send message
                    } else if (strlen(input) > 0) {
                        if (connected == 1) {
                            GString *terminated_msg = g_string_new("");
                            g_string_append_printf(terminated_msg, "%sMSGEND", input);
                            wrefresh(outw);
                            int sent = 0;
                            int to_send = terminated_msg->len;
                            char *marker = terminated_msg->str;
                            while (to_send > 0 && ((sent = write(socket_desc, marker, to_send)) > 0)) {
                                to_send -= sent;
                                marker += sent;
                            }
                            g_string_free(terminated_msg, TRUE);
                        }
                    }
                    wclear(inpw);
                    len = 0;
                } else {
                    input[len++] = ch;
                }
            }

            // check for incoming messages from server
            int read_size;
            char *server_reply = malloc(sizeof(char) * 1000);
            read_size = recv(socket_desc, server_reply, 1000, MSG_DONTWAIT);
            if (read_size > 0) {
                server_reply[read_size] = '\0';
                wprintw(outw, "%s\n", server_reply);
                wrefresh(outw);
            }
            free(server_reply);
            wrefresh(inpw);
        }
    }

    close(socket_desc);

    endwin();

    return 0;
}
