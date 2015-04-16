#ifndef __H_PROTO
#define __H_PROTO

#define STR_REGISTER_END "REGEND"
#define STR_MESSAGE_END "MSGEND"
#define STR_SESSION_END "SESSEND"

typedef enum {
    PROTO_REGISTER,
    PROTO_MESSAGE,
    PROTO_CLOSE,
    PROTO_UNDEFINED
} proto_action_t;

proto_action_t proto_get_action(char *stream);

#endif