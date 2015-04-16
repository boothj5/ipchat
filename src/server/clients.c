#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <glib.h>

#include "clients.h"

GSList *clients;

pthread_mutex_t clients_lock;
pthread_mutexattr_t lock_attr;

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
print_client_num(void)
{
    pthread_mutex_lock(&clients_lock);
    printf("Connected clients: %d\n", g_slist_length(clients));
    pthread_mutex_unlock(&clients_lock);
}

void
register_client(ChatClient *client, char *nickname)
{
    client->nickname = nickname;

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

    g_string_free(reg_msg, TRUE);
    g_string_free(reg_msg_term, TRUE);
}

void
broadcast(char *from, char *message)
{
    GString *relay_msg = g_string_new(from);
    g_string_append_printf(relay_msg, ": %s", message);

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

    g_string_free(relay_msg, TRUE);
    g_string_free(relay_msg_term, TRUE);
}

void
end_connection(ChatClient *client)
{
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

    curr = clients;
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

    print_client_num();

    g_string_free(quit_msg, TRUE);
    g_string_free(quit_msg_term, TRUE);
}
