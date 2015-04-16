#include <stdlib.h>
#include <string.h>
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

char*
proto_get_message(char *stream)
{
    char *message = malloc(strlen(stream) - (strlen(STR_MESSAGE_END) -1));
    strncpy(message, stream, strlen(stream) - strlen(STR_MESSAGE_END));
    message[strlen(stream) - strlen(STR_MESSAGE_END)] = '\0';

    return message;
}

char*
proto_get_nickname(char *stream)
{
    char *nickname = malloc(strlen(stream) - (strlen(STR_REGISTER_END) -1));
    strncpy(nickname, stream, strlen(stream) - strlen(STR_REGISTER_END));
    nickname[strlen(stream) - strlen(STR_REGISTER_END)] = '\0';

    return nickname;
}