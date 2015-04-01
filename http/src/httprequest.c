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

HttpRequest*
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

    HttpRequest *request = malloc(sizeof(HttpRequest));
    request->url = url;
    request->method = strdup(method);

    return request;
}

char*
httprequest_perform(HttpRequest *request)
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
    g_string_append(req, "GET /");
    g_string_append(req, request->url->path);
    g_string_append(req, " HTTP/1.0");
    g_string_append(req, "\r\n");
    g_string_append(req, "Host: ");
    g_string_append(req, request->url->host);
    g_string_append(req, "\r\n");
    g_string_append(req, "User-Agent: HTTPCLIENT 1.0");
    g_string_append(req, "\r\n");
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
    int htmlstart = 0;
    char *htmlcontent;
    int tmpres;
    GString *res_str = g_string_new("");
    char *result = NULL;

    while ((tmpres = recv(sock, buf, BUFSIZ, 0)) > 0) {
        if(htmlstart == 0) {
            htmlcontent = strstr(buf, "\r\n\r\n");
            if (htmlcontent != NULL) {
                htmlstart = 1;
                htmlcontent += 4;
            }
        } else {
            htmlcontent = buf;
        }

        if (htmlstart) {
            g_string_append(res_str, htmlcontent);
        }
        memset(buf, 0, tmpres);
    }

    if(tmpres < 0) {
        perror("Error receiving data");
    }

    g_string_free(req, TRUE);
    close(sock);

    result = res_str->str;
    g_string_free(res_str, FALSE);

    return result;
}