#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include "httprequest.h"

gboolean
_validate_args(int argc, char *argv[], char **arg_url, char **arg_method)
{
    GOptionEntry entries[] =
    {
        { "url", 'u', 0, G_OPTION_ARG_STRING, arg_url, "URL", NULL },
        { "method", 'm', 0, G_OPTION_ARG_STRING, arg_method, "method", NULL },
        { NULL }
    };

    GError *error = NULL;
    GOptionContext *context = g_option_context_new(NULL);
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("%s\n", error->message);
        g_option_context_free(context);
        g_error_free(error);
        return FALSE;
    }
    g_option_context_free(context);

    if (!*arg_url) {
        printf("You must specify a URL\n");
        return FALSE;
    }

    if (!*arg_method) {
        printf("You must specify a method\n");
        return FALSE;
    }

    return TRUE;
}

int
main(int argc, char *argv[])
{
    char *arg_url = NULL;
    char *arg_method = NULL;

    if (!_validate_args(argc, argv, &arg_url, &arg_method)) return 1;

    request_err_t r_err;
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
