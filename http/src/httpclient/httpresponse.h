#ifndef _HTTPRESPONSE_H
#define _HTTPRESPONSE_H

struct httpresponse_t {
    char *proto;
    int status;
    GHashTable *headers;
    char *body;
};

HttpResponse httpresponse_parse(char *response_str);

#endif