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