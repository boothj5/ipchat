#include <stdio.h>
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
    char ip[100];
    struct hostent *he;
    struct in_addr **addr_list;

    if (argc != 2) {
        printf("Please enter an hostname.\n");
        return 0;
    }

    hostname = argv[1];

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
    server.sin_port = htons(80); // host to network byte order

    if (connect(socket_desc, (struct sockaddr*)&server, sizeof(server)) < 0) {
        puts("Failed to connect.");
        return 1;
    }

    puts("Connected successfully.");

    char message[100];
    sprintf(message,"GET / HTTP/1.1\r\nHost: %s\r\n\r\n", hostname);
    if (send(socket_desc, message, strlen(message), 0) < 0) {
        printf("Failed to send request: %s\n", message);
        return 1;
    }

    printf("Request sent: \n%s\n", message);

    char server_reply[1000];
    if (recv(socket_desc, server_reply, 1000, 0) < 0) {
        puts("Receive failed.");
        return 1;
    }

    puts("Reply:");
    puts(server_reply);

    close(socket_desc);

    return 0;
}
