#ifndef _HTTPCLIENT_H
#define _HTTPCLIENT_H

#include <glib.h>

#include "httpcontext.h"

typedef enum {
    URL_NO_SCHEME,
    URL_INVALID_SCHEME,
    URL_INVALID_PORT,
    REQ_INVALID_METHOD,
    SOCK_CREATE_FAILED,
    SOCK_CONNECT_FAILED,
    SOCK_SEND_FAILED,
    SOCK_TIMEOUT,
    SOCK_RECV_FAILED,
    HOST_LOOKUP_FAILED
} request_err_t;

typedef struct httprequest_t *HttpRequest;
typedef struct httpresponse_t *HttpResponse;
typedef struct httpcontext_t *HttpContext;

void http_error(char *prefix, request_err_t err);

HttpContext httpcontext_create(void);
void httpcontext_debug(HttpContext ctx, gboolean debug);
void httpcontext_read_timeout(HttpContext ctx, int read_timeout_ms);

HttpRequest httprequest_create(char *url_s, char *method, request_err_t *err);
void httprequest_addheader(HttpRequest request, const char * const key, const char *const val);
HttpResponse httprequest_perform(HttpContext ctx, HttpRequest request, request_err_t *err);

int httpresponse_status(HttpResponse response);
char* httpresponse_body(HttpResponse response);
GHashTable* httpresponse_headers(HttpResponse response);

#endif