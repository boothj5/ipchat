#ifndef _HTTPREQUEST_H
#define _HTTPREQUEST_H

typedef struct url_t {
    char *scheme;
    char *host;
    int port;
    char *path;
} HttpUrl;

typedef struct request_t {
    HttpUrl *url;
    char *method;
    GHashTable *headers;
} HttpRequest;

typedef enum {
    URL_NO_SCHEME,
    URL_INVALID_SCHEME,
    URL_INVALID_PORT,
    REQ_INVALID_METHOD
} request_err_t;

void httprequest_error(char *prefix, request_err_t err);

HttpRequest* httprequest_create(char *url_s, char *method, request_err_t *err);
char* httprequest_perform(HttpRequest *request);
void httprequest_addheader(HttpRequest *request, const char * const key, const char *const val);

#endif