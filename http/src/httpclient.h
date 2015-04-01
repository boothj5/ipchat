#ifndef _HTTPCLIENT_H
#define _HTTPCLIENT_H

typedef enum {
    URL_NO_SCHEME,
    URL_INVALID_SCHEME,
    URL_INVALID_PORT,
    REQ_INVALID_METHOD
} request_err_t;

typedef struct httprequest_t *HttpRequest;
typedef struct httpresponse_t *HttpResponse;

void httprequest_error(char *prefix, request_err_t err);

HttpRequest httprequest_create(char *url_s, char *method, request_err_t *err);
HttpResponse httprequest_perform(HttpRequest request);
void httprequest_addheader(HttpRequest request, const char * const key, const char *const val);

int httpresponse_status(HttpResponse response);
char* httpresponse_body(HttpResponse response);
GHashTable* httpresponse_headers(HttpResponse response);

#endif