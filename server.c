#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <glib.h>

typedef struct thread_client_t {
    char *ip;
    int port;
    int sock;
} ThreadData;

GSList *clients;

void* connection_handler(void *data)
{
    ThreadData *client = (ThreadData *)data;
    int read_size;
    char *client_message;

    while(1) {

        // listen for from client
        client_message = malloc(sizeof(char) * 2000);
        errno = 0;
        read_size = recv(client->sock, client_message, 2000 , 0);

        // error
        if (read_size == -1) {
            perror("Error receiving on connection");
            free(client->ip);
            free(client);
            free(client_message);
            break;

        // client closed
        } else if (read_size == 0) {
            printf("%s:%d - Client disconnected.\n", client->ip, client->port);
            free(client->ip);
            free(client);
            free(client_message);
            break;

        // successful recv
        } else {
            char incoming[2000];
            strncpy(incoming, client_message, 2000);
            incoming[read_size] = '\0';
            
            printf("%s:%d - Received: %s\n", client->ip, client->port, incoming);
            fflush(stdout);

            // send to all clients
            GSList *curr = clients;
            while (curr != NULL) {
                ThreadData *participant = curr->data;
                write(participant->sock, client_message, read_size);
                curr = g_slist_next(curr);
            }
            free(client_message);
        }
    }

    return 0;
}

int main(int argc , char *argv[])
{
    int port = 0;

    GOptionEntry entries[] =
    {
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

    if (port == 0) {
        port = 6660;
    }

    int socket_desc, client_socket, c, ret;
    struct sockaddr_in server_addr, client_addr;

    if (argc == 2) {
        port = atoi(argv[1]);
    }

    printf("Starting on port: %d...\n", port);
     
    errno = 0;
    socket_desc = socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // ipv4, tcp, ip
    if (socket_desc == -1) {
        perror("Could not create socket");
	return 0;
    }
     
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
     
    errno = 0;
    ret = bind(socket_desc, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret == -1) {
        perror("Bind failed");
	return 0;
    }

    errno = 0;
    ret = listen(socket_desc, 20);
    if (ret == -1) {
        perror("Listen failed");
	return 0;
    }
    puts("Waiting for incoming connections...");

    while (1) {

        // listen for new connections
        c = sizeof(struct sockaddr_in);
        errno = 0;
        client_socket = accept(socket_desc, (struct sockaddr *)&client_addr, (socklen_t*)&c);
        if (client_socket == -1) {
            perror("Accept failed");
        }

        // add new client
        ThreadData *client = malloc(sizeof(ThreadData));
        client->ip = strdup(inet_ntoa(client_addr.sin_addr));
        client->port = ntohs(client_addr.sin_port);
        client->sock = client_socket;
        clients = g_slist_append(clients, client);

        pthread_t sniffer_thread;
        ret = pthread_create(&sniffer_thread, NULL, connection_handler, (void *)client);
        if (ret == 0) {
            printf("%s:%d - Connection handler assigned.\n", client->ip, client->port);
        } else {
            puts("Could not create thread.");
        }
    }
 
    return 0;
}
