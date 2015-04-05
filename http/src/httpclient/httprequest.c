#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <glib.h>

#include "httprequest.h"
#include "httpclient.h"
#include "httpresponse.h"

HttpUrl*
_url_parse(char *url_s, request_err_t *err)
{
    char *scheme = g_uri_parse_scheme(url_s);
    if (!scheme) {
        *err = URL_NO_SCHEME;
        return NULL;
    }
    if ((strcmp(scheme, "http") != 0) && (strcmp(scheme, "https") != 0)) {
        *err = URL_INVALID_SCHEME;
        g_free(scheme);
        return NULL;
    }

    int pos = 0;
    if (strcmp(scheme, "http") == 0) {
        pos = strlen("http://");
    } else {
        pos = strlen("https://");
    }
    int start = pos;
    while (url_s[pos] != '/' && url_s[pos] != ':' && pos < (int)strlen(url_s)) pos++;
    char *host = strndup(&url_s[start], pos - start);

    char *port_s = NULL;
    if (url_s[pos] == ':') {
        pos++;
        start = pos;
        while (url_s[pos] != '/' && pos < (int)strlen(url_s)) pos++;
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

void
httprequest_addheader(HttpRequest request, const char * const key, const char *const val)
{
    g_hash_table_replace(request->headers, strdup(key), strdup(val));
}

HttpRequest
httprequest_create(char *url_s, char *method, request_err_t *err)
{
    if (g_strcmp0(method, "GET") != 0) {
        *err = REQ_INVALID_METHOD;
        return NULL;
    }

    HttpUrl *url = _url_parse(url_s, err);
    if (!url) {
        return NULL;
    }

    HttpRequest request = malloc(sizeof(HttpRequest));
    request->url = url;
    request->method = strdup(method);
    request->headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    GString *host_val = g_string_new("");
    g_string_append_printf(host_val, "%s:%d", request->url->host, request->url->port);
    httprequest_addheader(request, "Host", host_val->str);
    g_string_free(host_val, FALSE);

    return request;
}

char*
_build_request(HttpContext context, HttpRequest request)
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

    char *result = req->str;
    g_string_free(req, FALSE);

    if (context->debug) printf("\n---REQUEST START---\n%s---REQUEST END---\n", result);

    return result;
}

HttpResponse
httprequest_perform(HttpContext context, HttpRequest request, request_err_t *err)
{
    struct hostent *he = gethostbyname(request->url->host);
    if (he == NULL) {
        *err = HOST_LOOKUP_FAILED;
        return NULL;
    }

    if (context->debug) printf("\nHost %s resolved to:\n", request->url->host);
    struct in_addr **addr_list = (struct in_addr**)he->h_addr_list;
    char *ip = NULL;
    int i;
    for (i = 0; addr_list[i] != NULL; i++) {
        if (context->debug) printf("  %s\n", inet_ntoa(*addr_list[i]));
        if (i == 0) {
            ip = strdup(inet_ntoa(*addr_list[i]));
        }
    }
    if (context->debug) printf("Connecting to %s:%d...\n", ip, request->url->port);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) {
        *err = SOCK_CREATE_FAILED;
        return NULL;
    }

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_family = AF_INET; // ipv4
    server.sin_port = htons(request->url->port); // host to network byte order

    struct timeval tv;
    if (context->read_timeout_ms >= 1000) {
        tv.tv_sec = context->read_timeout_ms / 1000;
        tv.tv_usec = (context->read_timeout_ms % 1000) * 1000;
    } else {
        tv.tv_sec = 0;
        tv.tv_usec = context->read_timeout_ms * 1000;
    }

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        *err = SOCK_CONNECT_FAILED;
        return NULL;
    }
    if (context->debug) printf("Connected successfully.\n");

    char *req = _build_request(context, request);

    int sent = 0;
    while (sent < strlen(req)) {
        int res = send(sock, req+sent, strlen(req)-sent, 0);
        if (res == -1) {
            *err = SOCK_SEND_FAILED;
            return NULL;
        }
        sent += res;
    }

    free(req);

    HttpResponse response = malloc(sizeof(HttpResponse));

    int bufsize = 1;

    char buf[bufsize+1];
    memset(buf, 0, sizeof(buf));
    int res;
    GString *header_stream = g_string_new("");

    gboolean headers_read = FALSE;
    while (!headers_read && ((res = recv(sock, buf, bufsize, 0)) > 0)) {
        if (context->debug) {
            char *packet = strndup((char *)buf, res);
            packet[res] = '\0';
            printf("\n---PACKET START---\n%s---PAKCET END---\n", packet);
            free(packet);
        }

        g_string_append_len(header_stream, buf, res);
        if (g_strstr_len(header_stream->str, -1, "\r\n\r\n")) headers_read = TRUE;

        memset(buf, 0, sizeof(buf));
    }

    if (res < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *err = SOCK_TIMEOUT;
        } else {
            *err = SOCK_RECV_FAILED;
        }
        return NULL;
    }

    int status = 0;
    GHashTable *headers_ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    char *headers_end = g_strstr_len(header_stream->str, -1, "\r\n\r\n");
    if (headers_end) {
        gchar *head = g_strndup(header_stream->str, headers_end - header_stream->str);
        gchar **headers = g_strsplit(head, "\r\n", -1);

        // protocol line
        char *proto_line = headers[0];
        gchar **proto_chunks = g_strsplit(proto_line, " ", -1);
        status = (int) strtol(proto_chunks[1], NULL, 10);

        // headers
        int count = 1;
        while (count < g_strv_length(headers)) {
            gchar **header_chunks = g_strsplit(headers[count++], ":", 2);
            char *header_key = strdup(header_chunks[0]);
            char *header_val = strdup(header_chunks[1]);
            g_strstrip(header_key);
            g_strstrip(header_val);
            g_hash_table_replace(headers_ht, header_key, header_val);
        }
    }

    response->status = status;
    response->headers = headers_ht;

    GString *body_stream = g_string_new(headers_end + 4);

    g_string_free(header_stream, TRUE);

    if (g_hash_table_lookup(headers_ht, "Content-Length")) {
        int content_length = (int) strtol(g_hash_table_lookup(headers_ht, "Content-Length"), NULL, 10);
        if (content_length > 0) {
            int remaining = content_length - body_stream->len;
            if (bufsize > remaining) bufsize = remaining;

            while (remaining > 0 && ((res = recv(sock, buf, bufsize, 0)) > 0)) {
                if (context->debug) {
                    char *packet = strndup((char *)buf, res);
                    packet[res] = '\0';
                    printf("\n---PACKET START---\n%s---PAKCET END---\n", packet);
                    free(packet);
                }

                g_string_append_len(body_stream, buf, res);
                remaining = content_length - body_stream->len;
                if (bufsize > remaining) bufsize = remaining;
                memset(buf, 0, sizeof(buf));
            }

            if (res < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    *err = SOCK_TIMEOUT;
                } else {
                    *err = SOCK_RECV_FAILED;
                }
                return NULL;
            }
            response->body = body_stream->str;
        } else {
            response->body = NULL;
        }
    } else if (g_strcmp0(g_hash_table_lookup(headers_ht, "Transfer-Encoding"), "chunked") == 0) {
        // TODO handle chunked response
        response->body = NULL;
    } else {
        response->body = NULL;
    }
    g_string_free(body_stream, FALSE);

    close(sock);

    return response;
}