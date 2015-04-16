#include <glib.h>

#include "proto/proto.h"

proto_action_t
proto_get_action(char *stream)
{
    if (g_str_has_suffix(stream, STR_REGISTER_END)) {
        return PROTO_REGISTER;
    } else if (g_str_has_suffix(stream, STR_MESSAGE_END)) {
        return PROTO_MESSAGE;
    } else if (g_str_has_suffix(stream, STR_SESSION_END)) {
        return PROTO_CLOSE;
    } else {
        return PROTO_UNDEFINED;
    }
}