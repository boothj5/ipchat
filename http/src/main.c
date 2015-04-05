#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include "httpclient/httpclient.h"

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

    HttpContext ctx = httpcontext_create();
    httpcontext_debug(ctx, FALSE);
    httpcontext_read_timeout(ctx, 1000);

    request_err_t r_err;
    HttpRequest request = httprequest_create(arg_url, arg_method, &r_err);
    if (!request) {
        http_error("Error creating request", r_err);
        return 1;
    }

    httprequest_addheader(request, "User-Agent", "HTTPCLIENT 1.0");
    httprequest_addheader(request, "Accept-Encoding", "gzip");

    HttpResponse response = httprequest_perform(ctx, request, &r_err);
    if (!response) {
        http_error("Error performing request", r_err);
        return 1;
    }

    printf("\n");

    int status = httpresponse_status(response);

    printf("Status: %d\n\n", status);

    GHashTable *headers = httpresponse_headers(response);
    GList *keys = g_hash_table_get_keys(headers);
    GList *curr = keys;
    if (curr) {
        printf("Headers:\n");
        while (curr) {
            printf("  %s: %s\n", (char *)curr->data, (char *)g_hash_table_lookup(headers, curr->data));
            curr = g_list_next(curr);
        }
        printf("\n");
    }

    char *body = httpresponse_body(response);
    if (body) {
        printf("Body:\n%s\n", body);
    }

    return 0;
}
