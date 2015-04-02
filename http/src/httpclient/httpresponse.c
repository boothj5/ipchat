#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "httpclient.h"
#include "httpresponse.h"

int
httpresponse_status(HttpResponse response)
{
    return response->status;
}

char*
httpresponse_body(HttpResponse response)
{
    return response->body;
}

GHashTable*
httpresponse_headers(HttpResponse response)
{
    return response->headers;
}

HttpResponse
httpresponse_parse(char *response_str)
{
    HttpResponse response = malloc(sizeof(HttpResponse));
    response->headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    gchar **chunks = g_strsplit(response_str, "\r\n", -1);

    // set status
    char *proto_line = chunks[0];
    gchar **proto_chunks = g_strsplit(proto_line, " ", -1);
    response->status = (int) strtol(proto_chunks[1], NULL, 10);

    // set headers
    int count = 1;
    while (g_strcmp0(chunks[count], "") != 0) {
        gchar **header_chunks = g_strsplit(chunks[count++], ":", -1);
        char *header_key = strdup(header_chunks[0]);
        char *header_val = strdup(header_chunks[1]);
        g_strstrip(header_key);
        g_strstrip(header_val);
        g_hash_table_replace(response->headers, header_key, header_val);
    }

    // set body
    count++;
    if (chunks[count]) {
        response->body = strdup(chunks[count]);
    } else {
        response->body = NULL;
    }

    return response;
}
