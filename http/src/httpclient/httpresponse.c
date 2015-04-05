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
httpresponse_status_message(HttpResponse response)
{
    return response->status_msg;
}

GByteArray*
httpresponse_body(HttpResponse response)
{
    return response->body;
}

char*
httpresponse_body_as_string(HttpResponse response)
{
    if (response->body) {
        char *body = strndup((char *)response->body->data, response->body->len);
        return body;
    } else {
        return NULL;
    }
}

GHashTable*
httpresponse_headers(HttpResponse response)
{
    return response->headers;
}