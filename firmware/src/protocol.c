#include "protocol.h"
#include "crc8.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define FRAME_OVERHEAD  4u   /* '$' + '*' + 2 CRC digits */

static bool is_payload_char(char c)
{
    return (c >= 0x20) && (c <= 0x7E) && (c != PROTO_SOF) && (c != PROTO_CRC_SEP);
}

static int hex_value(char c)
{
    if ((c >= '0') && (c <= '9')) { return c - '0'; }
    if ((c >= 'A') && (c <= 'F')) { return c - 'A' + 10; }
    if ((c >= 'a') && (c <= 'f')) { return c - 'a' + 10; }
    return -1;
}

/* strlen that stops early, so a missing terminator can't run away. */
static size_t bounded_len(const char *s, size_t max)
{
    size_t n = 0u;
    while ((n <= max) && (s[n] != '\0')) {
        n++;
    }
    return n;
}

proto_status_t protocol_decode(const char *frame, char *payload, size_t payload_size)
{
    if ((frame == NULL) || (payload == NULL)) {
        return PROTO_ERR_FORMAT;
    }

    size_t len = bounded_len(frame, PROTO_MAX_FRAME_LEN);
    if (len > PROTO_MAX_FRAME_LEN) {
        return PROTO_ERR_LENGTH;
    }
    if ((len < FRAME_OVERHEAD + 1u) || (frame[0] != PROTO_SOF) || (frame[len - 3u] != PROTO_CRC_SEP)) {
        return PROTO_ERR_FORMAT;
    }

    int hi = hex_value(frame[len - 2u]);
    int lo = hex_value(frame[len - 1u]);
    if ((hi < 0) || (lo < 0)) {
        return PROTO_ERR_FORMAT;
    }

    size_t payload_len = len - FRAME_OVERHEAD;
    for (size_t i = 0u; i < payload_len; i++) {
        if (!is_payload_char(frame[1u + i])) {
            return PROTO_ERR_FORMAT;
        }
    }
    if (payload_len + 1u > payload_size) {
        return PROTO_ERR_LENGTH;
    }

    uint8_t expected = (uint8_t)((hi << 4) | lo);
    if (crc8((const uint8_t *)&frame[1], payload_len) != expected) {
        return PROTO_ERR_CRC;
    }

    memcpy(payload, &frame[1], payload_len);
    payload[payload_len] = '\0';
    return PROTO_OK;
}

size_t protocol_encode(const char *payload, char *out, size_t out_size)
{
    static const char hex[] = "0123456789ABCDEF";

    if ((payload == NULL) || (out == NULL)) {
        return 0u;
    }

    size_t len = strlen(payload);
    if (len == 0u) {
        return 0u;
    }
    for (size_t i = 0u; i < len; i++) {
        if (!is_payload_char(payload[i])) {
            return 0u;
        }
    }

    size_t total = len + FRAME_OVERHEAD + 1u;   /* + '\n' */
    if (total + 1u > out_size) {                 /* + NUL  */
        return 0u;
    }

    uint8_t crc = crc8((const uint8_t *)payload, len);
    out[0] = PROTO_SOF;
    memcpy(&out[1], payload, len);
    out[len + 1u] = PROTO_CRC_SEP;
    out[len + 2u] = hex[(crc >> 4) & 0x0Fu];
    out[len + 3u] = hex[crc & 0x0Fu];
    out[len + 4u] = '\n';
    out[len + 5u] = '\0';
    return total;
}
