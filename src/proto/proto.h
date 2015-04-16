#ifndef __H_PROTO
#define __H_PROTO

#define STR_MESSAGE_END "MSGEND"
#define STR_SESSION_END "SESSEND"

typedef enum {
    PROTO_MESSAGE_END,
    PROTO_SESSION_END
} proto_term_t;

#endif