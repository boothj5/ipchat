#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

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
httpresponse_parse(HttpContext context, char *response_str, request_err_t *err)
{
    if (context->debug) printf("\n---RESPONSE START---\n%s---RESPONSE END---\n", response_str);

    HttpResponse response = malloc(sizeof(HttpResponse));
    response->headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    gchar *end_head = g_strstr_len(response_str, -1, "\r\n\r\n");
    gchar *head = g_strndup(response_str, end_head - response_str);
    gchar *body = end_head + 4;

    gchar **headers = g_strsplit(head, "\r\n", -1);

    // protocol line
    char *proto_line = headers[0];
    gchar **proto_chunks = g_strsplit(proto_line, " ", -1);
    response->status = (int) strtol(proto_chunks[1], NULL, 10);

    // headers
    int count = 1;
    while (count < g_strv_length(headers)) {
        gchar **header_chunks = g_strsplit(headers[count++], ":", 2);
        char *header_key = strdup(header_chunks[0]);
        char *header_val = strdup(header_chunks[1]);
        g_strstrip(header_key);
        g_strstrip(header_val);
        g_hash_table_replace(response->headers, header_key, header_val);
    }

    // body
    char *transfer_encoding = g_hash_table_lookup(response->headers, "Transfer-Encoding");
    if (transfer_encoding) {
        if (g_strcmp0(transfer_encoding, "chunked") == 0) {
            if (body) {
                GString *body_str = g_string_new("");

                int start = 0;
                int pos = 0;

                while (start < strlen(body)) {
                    // get chunk size
                    while (!g_str_has_prefix(&body[start + pos], "\r\n")) pos++;

                    char *hexlen = strndup(&body[start], pos);
                    if (g_strcmp0(hexlen, "0") == 0) break;

                    char *end;
                    errno = 0;
                    int len = (int) strtol(hexlen, &end, 16);
                    if ((!(errno == 0 && hexlen && !*end)) || (len < 1)) {
                        *err = RESP_ERROR_PARSING_CHUNK;
                        free(hexlen);
                        return NULL;
                    }
                    free(hexlen);

                    // read chunk
                    pos += 2;
                    g_string_append_len(body_str, &body[pos], len);

                    start = pos + len + 2;
                    pos = 0;
                }

                response->body = strdup(body_str->str);
                g_string_free(body_str, TRUE);

                return response;
            } else {
                response->body = NULL;

                return response;
            }
        } else {
            *err = RESP_UPSUPPORTED_TRANSFER_ENCODING;
            return NULL;
        }
    } else {
        if (body) {
            response->body = strdup(body);
        } else {
            response->body = NULL;
        }

        return response;
    }
}
