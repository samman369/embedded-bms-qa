#ifndef PROTOCOL_H
#define PROTOCOL_H

/* Frame format: $<payload>*<CC>\n  (CC = CRC-8 of payload, 2 hex digits) */

#include <stddef.h>

#define PROTO_SOF            '$'
#define PROTO_CRC_SEP        '*'
#define PROTO_MAX_FRAME_LEN  64u   /* characters, excluding the '\n' */

typedef enum {
    PROTO_OK = 0,
    PROTO_ERR_FORMAT,
    PROTO_ERR_CRC,
    PROTO_ERR_LENGTH
} proto_status_t;

/* frame: NUL-terminated, without trailing '\n'. payload receives the text between '$' and '*'. */
proto_status_t protocol_decode(const char *frame, char *payload, size_t payload_size);

/* Builds "$payload*CC\n". Returns the frame length, or 0 on invalid input / small buffer. */
size_t protocol_encode(const char *payload, char *out, size_t out_size);

#endif /* PROTOCOL_H */
