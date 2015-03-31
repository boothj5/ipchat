#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib.h>

typedef struct url_t {
    char *scheme;
    char *host;
    int port;
    char *path;
} HttpUrl;

typedef struct request_t {
    HttpUrl *url;
    char *method;
} HttpRequest;

typedef enum {
    URL_NO_SCHEME,
    URL_INVALID_SCHEME,
    URL_INVALID_PORT
} url_parse_err_t;

typedef enum {
    REQ_NO_URL_SPECIFIED,
    REQ_INVALID_METHOD
} request_err_t;

HttpUrl*
url_parse(char *url_s, url_parse_err_t *err)
{
    char *scheme = g_uri_parse_scheme(url_s);
    if (!scheme) {
        *err = URL_NO_SCHEME;
        return NULL;
    }
    if (strcmp(scheme, "http") != 0) {
        *err = URL_INVALID_SCHEME;
        g_free(scheme);
        return NULL;
    }

    int pos = strlen("http://");
    int start = pos;
    while (url_s[pos] != '/' && url_s[pos] != ':' && pos < strlen(url_s)) pos++;
    char *host = strndup(&url_s[start], pos - start);

    char *port_s = NULL;
    if (url_s[pos] == ':') {
        pos++;
        start = pos;
        while (url_s[pos] != '/' && pos < strlen(url_s)) pos++;
        port_s = strndup(&url_s[start], pos - start);
    } else {
        port_s = strdup("80");
    }

    char *end;
    errno = 0;
    int port = (int) strtol(port_s, &end, 10);
    if ((!(errno == 0 && port_s && !*end)) || (port < 1)) {
        *err = URL_INVALID_PORT;
        g_free(scheme);
        free(host);
        free(port_s);
        return NULL;
    }

    char *path = NULL;
    if (url_s[pos] != '\0') {
        path = strdup(&url_s[pos]);
    } else {
        path = strdup("/");
    }

    HttpUrl *url = malloc(sizeof(HttpUrl));
    url->scheme = scheme;
    url->host = host;
    url->port = port;
    url->path = path;

    return url;
}

HttpRequest*
httprequest_create(HttpUrl *url, char *method, request_err_t *err)
{
    if (!url) {
        *err = REQ_NO_URL_SPECIFIED;
        return NULL;
    }

    if (g_strcmp0(method, "GET") != 0) {
        *err = REQ_INVALID_METHOD;
        return NULL;
    }

    HttpRequest *request = malloc(sizeof(HttpRequest));
    request->url = url;
    request->method = strdup(method);

    return request;
}

int
main(int argc, char *argv[])
{
    char *arg_url = NULL;
    char *arg_method = NULL;

    GOptionEntry entries[] =
    {
        { "url", 'u', 0, G_OPTION_ARG_STRING, &arg_url, "URL", NULL },
        { "method", 'm', 0, G_OPTION_ARG_STRING, &arg_method, "URL", NULL },
        { NULL }
    };

    GError *error = NULL;
    GOptionContext *context = g_option_context_new(NULL);
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("%s\n", error->message);
        g_option_context_free(context);
        g_error_free(error);
        return 1;
    }
    g_option_context_free(context);

    if (!arg_url) {
        printf("You must specify a URL\n");
        return 1;
    }

    if (!arg_method) {
        printf("You must specify a method\n");
        return 1;
    }

    url_parse_err_t u_err;
    HttpUrl *url = url_parse(arg_url, &u_err);
    if (!url) {
        switch (u_err) {
            case URL_NO_SCHEME:
                printf("Error parsing URL, no scheme.\n");
                return 1;
            case URL_INVALID_SCHEME:
                printf("Error parsing URL, invalid scheme.\n");
                return 1;
            case URL_INVALID_PORT:
                printf("Error parsing URL, invalid port.\n");
                return 1;
            default:
                printf("Error parsing URL, unknown.\n");
                return 1;
        }
    }

    request_err_t r_err;
    HttpRequest *request = httprequest_create(url, arg_method, &r_err);
    if (!request) {
        switch (r_err) {
            case REQ_NO_URL_SPECIFIED:
                printf("Error creating request, no URL provided.\n");
                return 1;
            case REQ_INVALID_METHOD:
                printf("Error creating request, unsupported method.\n");
                return 1;
            default:
                printf("Error creating request, unknown.\n");
                return 1;
        }
    }

    printf("Scheme : %s\n", request->url->scheme);
    printf("Host   : %s\n", request->url->host);
    printf("Port   : %d\n", request->url->port);
    printf("Path   : %s\n", request->url->path);
    printf("Method : %s\n", request->method);
    return 0;
}
