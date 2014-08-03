#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

typedef struct thread_data_t {
    char *ip;
    int port;
    int sock;
} ThreadData;

void *connection_handler(void *data)
{
    ThreadData *client = (ThreadData *)data;
    int read_size;
     
    printf("Connection handler assigned for ip: %s, port: %d.\n", client->ip, client->port);

    char message[100];
    sprintf(message, "Echo server, enter something...\n");
    write(client->sock, message, strlen(message));

    char client_message[2000];
    while((read_size = recv(client->sock, client_message, 2000 , 0)) > 0 ) {
        write(client->sock, client_message, read_size);
    }
     
    if(read_size == 0) {
        printf("Client disconnected, ip: %s, port %d.\n", client->ip, client->port);
        fflush(stdout);
    }

    else if(read_size == -1) {
        perror("recv failed.");
    }

    free(client);
         
    return 0;
}

int main(int argc , char *argv[])
{
    int socket_desc, new_socket, c;
    struct sockaddr_in server, client;

    if (argc != 2) {
        printf("Please enter a port.\n");
        return 0;
    }

    int port = atoi(argv[1]);
     
    socket_desc = socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // ipv4, tcp, ip
    if (socket_desc == -1) {
        printf("Could not create socket.");
    }
     
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
     
    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        puts("Bind failed.");
    }
    puts("Bind successfull.");
     
    listen(socket_desc, 20);
     
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);

    while ((new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ) {
        ThreadData *data = malloc(sizeof(ThreadData));
        data->ip = inet_ntoa(client.sin_addr);
        data->port = ntohs(client.sin_port);
        data->sock = new_socket;

        pthread_t sniffer_thread;
        if (pthread_create(&sniffer_thread, NULL, connection_handler, (void *)data) < 0) {
            perror("Could not create thread.");
            return 1;
        }
    }

    if (new_socket < 0) {
        perror("accept failed.");
    }
 
    return 0;
}
