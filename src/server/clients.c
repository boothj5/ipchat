#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <glib.h>

#include "server/clients.h"
#include "proto/proto.h"

GSList *clients;

pthread_mutex_t clients_lock;
pthread_mutexattr_t lock_attr;

void _broadcast_message(char *message)
{
    GString *msg_term = g_string_new(message);
    g_string_append(msg_term, STR_MESSAGE_END);

    pthread_mutex_lock(&clients_lock);
    GSList *curr = clients;
    while (curr != NULL) {
        ChatClient *participant = curr->data;
        int sent = 0;
        int to_send = strlen(msg_term->str);
        char *marker = msg_term->str;
        while (to_send > 0 && ((sent = write(participant->sock, marker, to_send)) > 0)) {
            to_send -= sent;
            marker += sent;
        }
        curr = g_slist_next(curr);
    }
    pthread_mutex_unlock(&clients_lock);

    g_string_free(msg_term, TRUE);
}

void
clients_init(void)
{
    // initialise recursive mutex
    pthread_mutexattr_init(&lock_attr);
    pthread_mutexattr_settype(&lock_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&clients_lock, &lock_attr);
}

ChatClient*
clients_new(struct sockaddr_in client_addr, int socket)
{
    ChatClient *client = malloc(sizeof(ChatClient));
    client->ip = strdup(inet_ntoa(client_addr.sin_addr));
    client->port = ntohs(client_addr.sin_port);
    client->sock = socket;
    client->nickname = NULL;

    return client;
}

void
clients_add(ChatClient *client)
{
    pthread_mutex_lock(&clients_lock);
    clients = g_slist_append(clients, client);
    pthread_mutex_unlock(&clients_lock);
}

void
clients_print_total(void)
{
    pthread_mutex_lock(&clients_lock);
    printf("Connected clients: %d\n", g_slist_length(clients));
    pthread_mutex_unlock(&clients_lock);
}

void
clients_register(ChatClient *client, char *nickname)
{
    client->nickname = nickname;

    GString *msg = g_string_new("--> ");
    g_string_append(msg, nickname);
    g_string_append(msg, " has joined the conversation.");

    _broadcast_message(msg->str);

    g_string_free(msg, TRUE);
}

void
clients_broadcast_message(char *from, char *message)
{
    GString *msg = g_string_new(from);
    g_string_append_printf(msg, ": %s", message);

    _broadcast_message(msg->str);

    g_string_free(msg, TRUE);
}

void
clients_end_session(ChatClient *client)
{
    // send to all clients
    GString *msg = g_string_new("<-- ");
    g_string_append(msg, client->nickname);
    g_string_append(msg, " has left the conversation.");

    _broadcast_message(msg->str);

    g_string_free(msg, TRUE);

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
    pthread_mutex_unlock(&clients_lock);

    clients_print_total();
}
