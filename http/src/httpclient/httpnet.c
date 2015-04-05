#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#include "httpclient.h"
#include "httprequest.h"
#include "httpresponse.h"

int
httpnet_connect(HttpContext context, char *host, int port, request_err_t *err)
{
    struct hostent *he = gethostbyname(host);
    if (he == NULL) {
        *err = HOST_LOOKUP_FAILED;
        return -1;
    }

    if (context->debug) printf("\nHost %s resolved to:\n", host);
    struct in_addr **addr_list = (struct in_addr**)he->h_addr_list;
    char *ip = NULL;
    int i;
    for (i = 0; addr_list[i] != NULL; i++) {
        if (context->debug) printf("  %s\n", inet_ntoa(*addr_list[i]));
        if (i == 0) {
            ip = strdup(inet_ntoa(*addr_list[i]));
        }
    }
    if (context->debug) printf("Connecting to %s:%d...\n", ip, port);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) {
        *err = SOCK_CREATE_FAILED;
        return -1;
    }

    struct timeval tv;
    if (context->read_timeout_ms >= 1000) {
        tv.tv_sec = context->read_timeout_ms / 1000;
        tv.tv_usec = (context->read_timeout_ms % 1000) * 1000;
    } else {
        tv.tv_sec = 0;
        tv.tv_usec = context->read_timeout_ms * 1000;
    }
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_family = AF_INET; // ipv4
    server.sin_port = htons(port); // host to network byte order

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        *err = SOCK_CONNECT_FAILED;
        return -1;
    }
    if (context->debug) printf("Connected successfully.\n");

    return sock;
}

gboolean
httpnet_send(HttpContext context, int sock, HttpRequest request, request_err_t *err)
{
    GString *req = g_string_new("");
    g_string_append(req, "GET ");
    g_string_append(req, request->url->path);
    g_string_append(req, " HTTP/1.1");
    g_string_append(req, "\r\n");

    GList *header_keys = g_hash_table_get_keys(request->headers);
    GList *header = header_keys;
    while (header) {
        char *key = header->data;
        char *val = g_hash_table_lookup(request->headers, key);
        g_string_append_printf(req, "%s: %s", key, val);
        g_string_append(req, "\r\n");
        header = g_list_next(header);
    }
    g_list_free(header_keys);

    g_string_append(req, "\r\n");

    if (context->debug) printf("\n---REQUEST START---\n%s---REQUEST END---\n", req->str);

    int sent = 0;
    while (sent < req->len) {
        int res = send(sock, req->str+sent, req->len-sent, 0);
        if (res == -1) {
            *err = SOCK_SEND_FAILED;
            g_string_free(req, TRUE);
            return FALSE;
        }
        sent += res;
    }

    g_string_free(req, TRUE);
    return TRUE;
}

gboolean
httpnet_read_headers(HttpContext context, int sock, HttpResponse response, request_err_t *err)
{
    char header_buf[2];
    memset(header_buf, 0, sizeof(header_buf));
    int res;

    gboolean headers_read = FALSE;
    GString *header_stream = g_string_new("");
    while (!headers_read && ((res = recv(sock, header_buf, 1, 0)) > 0)) {
        g_string_append_len(header_stream, header_buf, res);
        if (g_str_has_suffix(header_stream->str, "\r\n\r\n")) headers_read = TRUE;
        memset(header_buf, 0, sizeof(header_buf));
    }

    if (res < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) *err = SOCK_TIMEOUT;
        else *err = SOCK_RECV_FAILED;
        return FALSE;
    }

    int status = 0;
    GHashTable *headers_ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    if (header_stream->len > 0) {
        gchar **lines = g_strsplit(header_stream->str, "\r\n", -1);

        // protocol line
        char *proto_line = lines[0];
        gchar **proto_chunks = g_strsplit(proto_line, " ", -1);
        status = (int) strtol(proto_chunks[1], NULL, 10);

        // headers
        int count = 1;
        while (count < g_strv_length(lines)) {
            gchar **header_chunks = g_strsplit(lines[count++], ":", 2);
            if (header_chunks[0]) {
                char *header_key = strdup(header_chunks[0]);
                char *header_val = strdup(header_chunks[1]);
                g_strstrip(header_key);
                g_strstrip(header_val);
                g_hash_table_replace(headers_ht, header_key, header_val);
            }
        }
    }
    g_string_free(header_stream, TRUE);

    response->status = status;
    response->headers = headers_ht;

    return TRUE;
}

gboolean
httpnet_read_body(HttpContext context, int sock, HttpResponse response, request_err_t *err)
{
    GString *body_stream = g_string_new("");

    if (g_hash_table_lookup(response->headers, "Content-Length")) {
        int content_length = (int) strtol(g_hash_table_lookup(response->headers, "Content-Length"), NULL, 10);

        if (content_length > 0) {
            int res = 0;
            int bufsize = BUFSIZ;
            char content_buf[bufsize+1];
            memset(content_buf, 0, sizeof(content_buf));

            int remaining = content_length;
            if (remaining < bufsize) bufsize = remaining;
            while (remaining > 0 && ((res = recv(sock, content_buf, bufsize, 0)) > 0)) {
                g_string_append_len(body_stream, content_buf, res);
                remaining = content_length - body_stream->len;
                if (bufsize > remaining) bufsize = remaining;
                memset(content_buf, 0, sizeof(content_buf));
            }

            if (res < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) *err = SOCK_TIMEOUT;
                else *err = SOCK_RECV_FAILED;
                return FALSE;
            }

            response->body = body_stream->str;
        } else {
            response->body = NULL;
        }
    } else if (g_strcmp0(g_hash_table_lookup(response->headers, "Transfer-Encoding"), "chunked") == 0) {
        // TODO handle chunked response
        response->body = NULL;
    } else {
        response->body = NULL;
    }
    g_string_free(body_stream, FALSE);

    return TRUE;
}