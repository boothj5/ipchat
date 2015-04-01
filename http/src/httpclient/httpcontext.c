#include <stdlib.h>

#include <glib.h>

#include "httpclient.h"
#include "httpcontext.h"

HttpContext
httpcontext_create(gboolean debug, int read_timeout_ms)
{
    HttpContext context = malloc(sizeof(HttpContext));
    context->debug = debug;
    context->read_timeout_ms = read_timeout_ms;

    return context;
}