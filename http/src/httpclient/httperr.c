#include <stdio.h>

#include <glib.h>

#include "httpclient.h"

void
http_error(char *prefix, request_err_t err)
{
    GString *full_msg = g_string_new("");
    if (prefix) {
        g_string_append_printf(full_msg, "%s: ", prefix);
    }
    switch (err) {
        case URL_NO_SCHEME:
            g_string_append(full_msg, "no scheme.");
            break;
        case URL_INVALID_SCHEME:
            g_string_append(full_msg, "invalid scheme.");
            break;
        case URL_INVALID_PORT:
            g_string_append(full_msg, "invalid port.");
            break;
        case REQ_INVALID_METHOD:
            g_string_append(full_msg, "unsupported method.");
            break;
        case SOCK_CREATE_FAILED:
            g_string_append(full_msg, "failed to create socket.");
            break;
        case SOCK_CONNECT_FAILED:
            g_string_append(full_msg, "socket connect failed.");
            break;
        case SOCK_SEND_FAILED:
            g_string_append(full_msg, "socket send failed.");
            break;
        case SOCK_TIMEOUT:
            g_string_append(full_msg, "socket read timeout.");
            break;
        case SOCK_RECV_FAILED:
            g_string_append(full_msg, "socket read failed.");
            break;
        case HOST_LOOKUP_FAILED:
            g_string_append(full_msg, "host lookup failed.");
            break;
        default:
            g_string_append(full_msg, "unknown.\n");
            break;
    }

    printf("%s\n", full_msg->str);
    g_string_free(full_msg, TRUE);
}