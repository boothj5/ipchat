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
} HttpClientUrl;

typedef enum {
    NO_SCHEME,
    INVALID_SCHEME,
    INVALID_PORT
} url_parse_err_t;

HttpClientUrl*
url_parse(char *url_s, url_parse_err_t *err)
{
    char *scheme = g_uri_parse_scheme(url_s);
    if (!scheme) {
        *err = NO_SCHEME;
        return NULL;
    }
    if (strcmp(scheme, "http") != 0) {
        *err = INVALID_SCHEME;
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
        *err = INVALID_PORT;
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

    HttpClientUrl *url = malloc(sizeof(HttpClientUrl));
    url->scheme = scheme;
    url->host = host;
    url->port = port;
    url->path = path;

    return url;
}

int
main(int argc, char *argv[])
{
    char *arg_url = NULL;

    GOptionEntry entries[] =
    {
        { "url", 'u', 0, G_OPTION_ARG_STRING, &arg_url, "URL", NULL },
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

    if (!arg_url) {
        printf("You must specify a URL\n");
        return 1;
    }

    url_parse_err_t err;
    HttpClientUrl *url = url_parse(arg_url, &err);
    if (!url) {
        switch (err) {
            case NO_SCHEME:
                printf("Error parsing URL, no scheme.\n");
                return 1;
            case INVALID_SCHEME:
                printf("Error parsing URL, invalid scheme.\n");
                return 1;
            case INVALID_PORT:
                printf("Error parsing URL, invalid port.\n");
                return 1;
            default:
                printf("Error parsing URL, unknown.\n");
                return 1;
        }
    }

    printf("Scheme : %s\n", url->scheme);
    printf("Host   : %s\n", url->host);
    printf("Port   : %d\n", url->port);
    printf("Path   : %s\n", url->path);
    return 0;
}
