#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <ncurses.h>

int
main(int argc, char *argv[])
{
    char *hostname;
    int port;
    char ip[100];
    struct hostent *he;
    struct in_addr **addr_list;
    int connected = 0;

    if (argc != 3) {
        printf("Please enter a hostname and port.\n");
        return 0;
    }

    hostname = argv[1];
    port = atoi(argv[2]);

    if ((he = gethostbyname(hostname)) == NULL) {
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

    initscr();
    cbreak();
    keypad(stdscr, TRUE);

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    WINDOW *outw = newwin(rows/2, cols, 0, 0);
    scrollok(outw, TRUE);
    WINDOW *inpw = newwin(rows/2-1, cols, rows/2, 0);
    scrollok(inpw, TRUE);

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

        while (1) {
            char *input = malloc(sizeof(char) * 1000);
            wgetstr(inpw, input);

            if ((strlen(input) > 0) && (input[strlen(input) - 1] == '\n'))
                input[strlen (input) - 1] = '\0';

            if (strncmp(input, "/quit", 5) == 0) {
                break;
            } else if (strlen(input) > 0) {
                if (connected == 1) {
                    write(socket_desc, input, strlen(input));
                    free(input);

                    int read_size;
                    char *server_reply = malloc(sizeof(char) * 1000);
                    if ((read_size = recv(socket_desc, server_reply, 1000, 0)) < 0) {
                        wprintw(outw, "Receive failed.\n");
                        wrefresh(outw);
                        return 1;
                    }

                    server_reply[read_size] = '\0';
                    wprintw(outw, "%s\n", server_reply);
                    wrefresh(outw);
                    free(server_reply);
                }
            }
        }
    }

    close(socket_desc);

    endwin();

    return 0;
}
