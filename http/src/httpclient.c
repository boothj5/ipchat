#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include "httprequest.h"

int
main(int argc, char *argv[])
{
    char *arg_url = NULL;
    char *arg_method = NULL;
    request_err_t r_err;

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

    HttpRequest *request = httprequest_create(arg_url, arg_method, &r_err);
    if (!request) {
        httprequest_error("Error creating request", r_err);
        return 1;
    }

    httprequest_addheader(request, "User-Agent", "HTTPCLIENT 1.0");

    char *response = httprequest_perform(request);

    printf("\n---RESPONSE START---\n");
    printf("%s", response);
    printf("---RESPONSE END---\n");

    return 0;
}
