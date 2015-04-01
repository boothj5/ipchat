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

typedef struct httpurl_t {
    char *scheme;
    char *host;
    int port;
    char *path;
} HttpUrl;

struct httprequest_t {
    HttpUrl *url;
    char *method;
    GHashTable *headers;
};

HttpUrl*
_url_parse(char *url_s, request_err_t *err)
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
httprequest_error(char *prefix, request_err_t err)
{
    GString *full_msg = g_string_new("");
    if (prefix) {
        g_string_append_printf(full_msg, "%s: ", prefix);
    }
    switch (err) {
        case URL_NO_SCHEME:
            g_string_append(full_msg, "no scheme.");
            return;
        case URL_INVALID_SCHEME:
            g_string_append(full_msg, "invalid scheme.");
            return;
        case URL_INVALID_PORT:
            g_string_append(full_msg, "invalid port.");
            return;
        case REQ_INVALID_METHOD:
            g_string_append(full_msg, "unsupported method.");
            return;
        default:
            g_string_append(full_msg, "unknown.\n");
            return;
    }
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
httprequest_perform(HttpRequest request)
{
    // create socket ip4, tcp, ip
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) {
        perror("Failed to create socket");
        return NULL;
    }

    struct hostent *he = gethostbyname(request->url->host);
    // failed to resolve hostname
    if (he == NULL) {
        herror("Could lookup host");
        return NULL;
    }

    printf("\nHost %s resolved to:\n", request->url->host);
    struct in_addr **addr_list = (struct in_addr**)he->h_addr_list;
    char *ip = NULL;
    int i;
    for (i = 0; addr_list[i] != NULL; i++) {
        printf("  %s\n", inet_ntoa(*addr_list[i]));
        if (i == 0) {
            ip = strdup(inet_ntoa(*addr_list[i]));
        }
    }

    printf("Connecting to %s:%d...\n", ip, request->url->port);

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_family = AF_INET; // ipv4
    server.sin_port = htons(request->url->port); // host to network byte order

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Failed to connect");
        return NULL;
    } else {
        printf("Connected successfully.\n");
    }

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

    printf("\n---REQUEST START---\n%s---REQUEST END---\n", req->str);

    int sent = 0;
    while (sent < strlen(req->str)) {
        int tmpres = send(sock, req->str+sent, strlen(req->str)-sent, 0);
        if (tmpres == -1){
            perror("Could not send request");
            return NULL;
        }
        sent += tmpres;
    }

    char buf[BUFSIZ+1];
    memset(buf, 0, sizeof(buf));
    int tmpres;
    GString *res_str = g_string_new("");
    char *result = NULL;

    while ((tmpres = recv(sock, buf, BUFSIZ, 0)) > 0) {
        g_string_append(res_str, buf);
    }

    if(tmpres < 0) {
        perror("Error receiving data");
    }

    g_string_free(req, TRUE);
    close(sock);

    result = res_str->str;
    g_string_free(res_str, FALSE);

    printf("\n---RESPONSE START---\n");
    printf("%s", result);
    printf("---RESPONSE END---\n");

    return result;
}