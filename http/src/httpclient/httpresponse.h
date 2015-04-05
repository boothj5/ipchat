#ifndef _HTTPRESPONSE_H
#define _HTTPRESPONSE_H

struct httpresponse_t {
    char *proto;
    int status;
    char *status_msg;
    GHashTable *headers;
    char *body;
};

#endif