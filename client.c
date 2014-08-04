#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

int
main(int argc, char *argv[])
{
    char *hostname;
    int port;
    char ip[100];
    struct hostent *he;
    struct in_addr **addr_list;

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

    printf("Host %s resolved to:\n", hostname);
    addr_list = (struct in_addr**)he->h_addr_list;
    int i;
    for (i = 0; addr_list[i] != NULL; i++) {
        printf("  %s\n", inet_ntoa(*addr_list[i]));
        if (i == 0) {
            strcpy(ip, inet_ntoa(*addr_list[i]));
        }
    }

    int socket_desc;
    socket_desc = socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // ipv4, tcp, ip
    if (socket_desc == -1) {
        puts("Failed to create socket.");
    }

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_family = AF_INET; // ipv4
    server.sin_port = htons(port); // host to network byte order

    if (connect(socket_desc, (struct sockaddr*)&server, sizeof(server)) < 0) {
        puts("Failed to connect.");
        return 1;
    }

    puts("Connected successfully.");

    while (1) {
        char *input = malloc(sizeof(char) * 1000);
        scanf("%s", input);

        write(socket_desc, input, strlen(input));
        free(input);

        char *server_reply = malloc(sizeof(char) * 1000);
        if (recv(socket_desc, server_reply, 1000, 0) < 0) {
            puts("Receive failed.");
            return 1;
        }

        printf("%s\n", server_reply);
        free(server_reply);
    }

    close(socket_desc);

    return 0;
}
