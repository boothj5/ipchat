#include <string.h>
#include <glib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "clients.h"
#include "proto/proto.h"

void* connection_handler(void *data)
{
    ChatClient *client = (ChatClient *)data;
    int read_size;

    // client loop
    while(1) {
        char buf[2];
        memset(buf, 0, sizeof(buf));

        // listen to client stream
        proto_term_t action = PROTO_UNDEFINED;
        gboolean sessionend = FALSE;
        GString *stream = g_string_new("");
        errno = 0;
        while ((action == PROTO_UNDEFINED) && !sessionend && ((read_size = recv(client->sock, buf, 1, 0)) > 0)) {
            g_string_append_len(stream, buf, read_size);
            if (g_str_has_suffix(stream->str, STR_REGISTER_END)) action = PROTO_REGISTER;
            if (g_str_has_suffix(stream->str, STR_MESSAGE_END)) action = PROTO_MESSAGE;
            if (g_str_has_suffix(stream->str, STR_SESSION_END)) action = PROTO_CLOSE;
            memset(buf, 0, sizeof(buf));
        }

        // error
        if (read_size == -1) {
            perror("Error receiving on connection");
            clients_end_session(client);
            g_string_free(stream, TRUE);
            break;

        // client closed
        } else if (read_size == 0) {
            printf("%s:%d - Client disconnected.\n", client->ip, client->port);
            clients_end_session(client);
            g_string_free(stream, TRUE);
            break;

        // end session
        } else if (action == PROTO_CLOSE) {
            printf("%s:%d - EXIT: %s\n", client->ip, client->port, client->nickname);
            clients_end_session(client);
            g_string_free(stream, TRUE);
            break;

        // registration
        } else if (action == PROTO_REGISTER) {
            char *nickname = malloc(stream->len - (strlen(STR_REGISTER_END) -1));
            strncpy(nickname, stream->str, stream->len - strlen(STR_REGISTER_END));
            nickname[stream->len - strlen(STR_REGISTER_END)] = '\0';

            printf("%s:%d - JOIN: %s\n", client->ip, client->port, nickname);
            clients_register(client, nickname);
            g_string_free(stream, TRUE);

        // message
        } else if (action == PROTO_MESSAGE) {
            char *incoming = malloc(stream->len -  (strlen(STR_MESSAGE_END) -1));
            strncpy(incoming, stream->str, stream->len - strlen(STR_MESSAGE_END));
            incoming[stream->len - strlen(STR_MESSAGE_END)] = '\0';

            printf("%s:%d - RECV: %s\n", client->ip, client->port, incoming);
            clients_broadcast_message(client->nickname, incoming);
            g_string_free(stream, TRUE);
            free(incoming);

        } else {
            printf("Unknown protocol action.");
            clients_end_session(client);
            g_string_free(stream, TRUE);
            break;
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

    clients_init();

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

    clients_print_total();

    // connection accept loop
    while (1) {
        c = sizeof(struct sockaddr_in);
        errno = 0;
        client_socket = accept(listen_socket, (struct sockaddr *)&client_addr, (socklen_t*)&c);
        if (client_socket == -1) {
            perror("Accept failed");
        }

        // create thread for each new client
        ChatClient *client = clients_new(client_addr, client_socket);
        clients_add(client);

        pthread_t client_thread;
        ret = pthread_create(&client_thread, NULL, connection_handler, (void *)client);
        if (ret == 0) {
            printf("%s:%d - Connection handler assigned.\n", client->ip, client->port);
        } else {
            puts("Could not create thread.");
        }

        clients_print_total();
    }

    return 0;
}
