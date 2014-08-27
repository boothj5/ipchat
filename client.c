#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <ncursesw/ncurses.h>
#include <fcntl.h>

int
main(int argc, char *argv[])
{
    char *hostname;
    int port = 6660;
    char ip[100];
    struct hostent *he;
    struct in_addr **addr_list;
    int connected = 0;

    if (argc != 2) {
        printf("Please enter at least a hostname or ip address.\n");
        return 0;
    }

    hostname = argv[1];
    if (argc == 3) {
        port = atoi(argv[2]);
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
   
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, hostname, &(sa.sin_addr));

    if (result != 0) {
        strncpy(ip, hostname, strlen(hostname));
	ip[strlen(hostname)] = '\0';
	wprintw(outw, "Connecting to %s\n", ip);
    } else {
        if ((he = gethostbyname(hostname)) == NULL) {
            switch(h_errno)
            {
	        case HOST_NOT_FOUND:
	            wprintw(outw, "%s: Unknown host\n", hostname);
	            break;
	        case NO_ADDRESS:
                    wprintw(outw, "%s: No ip address found\n", hostname);
                    break;
                case NO_RECOVERY:
                    wprintw(outw, "%s: Non recoverable name server request\n", hostname);
                    break;
                case TRY_AGAIN:
                    wprintw(outw, "%s: Temporary error occurred at name server\n", hostname);
                    break;
                default:
                    wprintw(outw, "%s: Unknown error occurred getting ip address\n", hostname);
                    break;
            }
            return 1;
        }

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

        char input[1000];
        char len = 0;
        while (1) {
            int ch = wgetch(inpw);
            if (ch != ERR) {
                if (ch == '\n') {
                    input[len] = '\0';
                    if (strncmp(input, "/quit", 5) == 0) {
                        break;
                    } else if (strlen(input) > 0) {
                        if (connected == 1) {
                            write(socket_desc, input, strlen(input));
                        }
                    }
                    wclear(inpw);
                    len = 0;
                } else {
                    input[len++] = ch;
                }
            }

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
