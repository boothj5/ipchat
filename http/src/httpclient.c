#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include "httprequest.h"

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

    request_err_t r_err;
    HttpRequest *request = httprequest_create(arg_url, arg_method, &r_err);
    if (!request) {
        switch (r_err) {
            case URL_NO_SCHEME:
                printf("Error creating request, no scheme.\n");
                return 1;
            case URL_INVALID_SCHEME:
                printf("Error creating request, invalid scheme.\n");
                return 1;
            case URL_INVALID_PORT:
                printf("Error creating request, invalid port.\n");
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
