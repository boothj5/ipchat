#ifndef __H_CLIENTS
#define __H_CLIENTS

#include <netinet/in.h>

typedef struct thread_client_t {
    char *ip;
    int port;
    int sock;
    char *nickname;
} ChatClient;

void clients_init(void);

ChatClient* clients_new(struct sockaddr_in client_addr, int socket);
void clients_add(ChatClient *client);

void print_client_num(void);
void register_client(ChatClient *client, char *nickname);
void broadcast(char *from, char *message);
void end_connection(ChatClient *client);

#endif