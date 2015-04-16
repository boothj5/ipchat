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
    char *nickname;
} ChatClient;

GSList *clients;

pthread_mutex_t clients_lock;

ChatClient* client_new(struct sockaddr_in client_addr, int socket)
{
    ChatClient *client = malloc(sizeof(ChatClient));
    client->ip = strdup(inet_ntoa(client_addr.sin_addr));
    client->port = ntohs(client_addr.sin_port);
    client->sock = socket;
    client->nickname = NULL;

    return client;
}

void end_connection(ChatClient *client)
{
    pthread_mutex_lock(&clients_lock);
    GSList *curr = clients;
    while (curr != NULL) {
        if (curr->data == client) {
            clients = g_slist_remove_link(clients, curr);
            free(client->ip);
            free(client->nickname);
            close(client->sock);
            free(client);

            break;
        }
        curr = g_slist_next(curr);
    }
    printf("Connected clients: %d\n", g_slist_length(clients));
    pthread_mutex_unlock(&clients_lock);
}

void* connection_handler(void *data)
{
    ChatClient *client = (ChatClient *)data;
    int read_size;

    // client loop
    while(1) {
        char buf[2];
        memset(buf, 0, sizeof(buf));

        // listen for from client
        gboolean term = FALSE;
        gboolean sessionend = FALSE;
        GString *stream = g_string_new("");
        errno = 0;
        while (!term && !sessionend && ((read_size = recv(client->sock, buf, 1, 0)) > 0)) {
            g_string_append_len(stream, buf, read_size);
            if (g_str_has_suffix(stream->str, "MSGEND")) term = TRUE;
            if (g_str_has_suffix(stream->str, "SESSIONEND")) sessionend = TRUE;
            memset(buf, 0, sizeof(buf));
        }

        // error
        if (read_size == -1) {
            perror("Error receiving on connection");
            g_string_free(stream, TRUE);
            end_connection(client);
            break;

        // client closed
        } else if (read_size == 0) {
            printf("%s:%d - Client disconnected.\n", client->ip, client->port);
            g_string_free(stream, TRUE);
            end_connection(client);
            break;

        // end session
        } else if (sessionend) {
            printf("%s:%d - QUIT: %s\n", client->ip, client->port, client->nickname);
            fflush(stdout);

            // send to all clients
            GString *quit_msg = g_string_new("<-- ");
            g_string_append(quit_msg, client->nickname);
            g_string_append(quit_msg, " has left the conversation.");

            GString *quit_msg_term = g_string_new(quit_msg->str);
            g_string_append(quit_msg_term, "MSGEND");

            pthread_mutex_lock(&clients_lock);
            GSList *curr = clients;
            while (curr != NULL) {
                ChatClient *participant = curr->data;
                printf("%s:%d - SEND: %s\n", participant->ip, participant->port, quit_msg->str);

                int sent = 0;
                int to_send = strlen(quit_msg_term->str);
                char *marker = quit_msg_term->str;
                while (to_send > 0 && ((sent = write(participant->sock, marker, to_send)) > 0)) {
                    to_send -= sent;
                    marker += sent;
                }

                curr = g_slist_next(curr);
            }
            pthread_mutex_unlock(&clients_lock);
            g_string_free(stream, TRUE);
            g_string_free(quit_msg, TRUE);
            g_string_free(quit_msg_term, TRUE);
            end_connection(client);
            break;

        // registration
        } else if (!client->nickname) {
            char *nickname = malloc(stream->len -  5);
            strncpy(nickname, stream->str, stream->len - 6);
            nickname[stream->len - 6] = '\0';
            client->nickname = nickname;

            printf("%s:%d - NICK: %s\n", client->ip, client->port, nickname);
            fflush(stdout);

            // send to all clients
            GString *reg_msg = g_string_new("--> ");
            g_string_append(reg_msg, nickname);
            g_string_append(reg_msg, " has joined the conversation.");

            GString *reg_msg_term = g_string_new(reg_msg->str);
            g_string_append(reg_msg_term, "MSGEND");

            pthread_mutex_lock(&clients_lock);
            GSList *curr = clients;
            while (curr != NULL) {
                ChatClient *participant = curr->data;
                printf("%s:%d - SEND: %s\n", participant->ip, participant->port, reg_msg->str);

                int sent = 0;
                int to_send = strlen(reg_msg_term->str);
                char *marker = reg_msg_term->str;
                while (to_send > 0 && ((sent = write(participant->sock, marker, to_send)) > 0)) {
                    to_send -= sent;
                    marker += sent;
                }

                curr = g_slist_next(curr);
            }
            pthread_mutex_unlock(&clients_lock);
            g_string_free(stream, TRUE);
            g_string_free(reg_msg, TRUE);
            g_string_free(reg_msg_term, TRUE);

        // message
        } else {
            char *incoming = malloc(stream->len -  5);
            strncpy(incoming, stream->str, stream->len - 6);
            incoming[stream->len - 6] = '\0';

            printf("%s:%d - RECV: %s\n", client->ip, client->port, incoming);
            fflush(stdout);

            GString *relay_msg = g_string_new(client->nickname);
            g_string_append_printf(relay_msg, ": %s", incoming);

            GString *relay_msg_term = g_string_new(relay_msg->str);
            g_string_append(relay_msg_term, "MSGEND");

            // send to all clients
            pthread_mutex_lock(&clients_lock);
            GSList *curr = clients;
            while (curr != NULL) {
                ChatClient *participant = curr->data;
                printf("%s:%d - SEND: %s\n", participant->ip, participant->port, relay_msg->str);

                int sent = 0;
                int to_send = strlen(relay_msg_term->str);
                char *marker = relay_msg_term->str;
                while (to_send > 0 && ((sent = write(participant->sock, marker, to_send)) > 0)) {
                    to_send -= sent;
                    marker += sent;
                }

                curr = g_slist_next(curr);
            }
            pthread_mutex_unlock(&clients_lock);
            free(incoming);
            g_string_free(stream, TRUE);
            g_string_free(relay_msg, TRUE);
            g_string_free(relay_msg_term, TRUE);
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

    int listen_socket, client_socket;
    int c, ret;
    struct sockaddr_in server_addr, client_addr;

    if (argc == 2) {
        port = atoi(argv[1]);
    }

    printf("Starting on port: %d...\n", port);

    // create socket
    errno = 0;
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // ipv4, tcp, ip
    if (listen_socket == -1) {
        perror("Could not create socket");
        return 0;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // bind socket to port
    errno = 0;
    ret = bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret == -1) {
        perror("Bind failed");
        return 0;
    }

    // set socket to listen mode
    errno = 0;
    ret = listen(listen_socket, 5);
    if (ret == -1) {
        perror("Listen failed");
        return 0;
    }
    puts("Waiting for incoming connections...");

    pthread_mutex_lock(&clients_lock);
    printf("Connected clients: %d\n", g_slist_length(clients));
    pthread_mutex_unlock(&clients_lock);

    // accept incoming client connections
    while (1) {

        // listen for new connections
        c = sizeof(struct sockaddr_in);
        errno = 0;
        client_socket = accept(listen_socket, (struct sockaddr *)&client_addr, (socklen_t*)&c);
        if (client_socket == -1) {
            perror("Accept failed");
        }

        // create thread for each new client
        ChatClient *client = client_new(client_addr, client_socket);
        pthread_mutex_lock(&clients_lock);
        clients = g_slist_append(clients, client);
        pthread_mutex_unlock(&clients_lock);

        pthread_t client_thread;
        ret = pthread_create(&client_thread, NULL, connection_handler, (void *)client);
        if (ret == 0) {
            printf("%s:%d - Connection handler assigned.\n", client->ip, client->port);
        } else {
            puts("Could not create thread.");
        }

        pthread_mutex_lock(&clients_lock);
        printf("Connected clients: %d\n", g_slist_length(clients));
        pthread_mutex_unlock(&clients_lock);
    }

    return 0;
}
