#include "frame_reader.h"
#include <string.h>

void frame_reader_init(frame_reader_t *fr)
{
    memset(fr, 0, sizeof(*fr));
}

fr_result_t frame_reader_feed(frame_reader_t *fr, uint8_t byte)
{
    if (byte == (uint8_t)'\r') {
        return FR_NONE;
    }

    if (byte == (uint8_t)'\n') {
        if (fr->overflow) {
            fr->overflow = false;
            fr->len = 0u;
            return FR_FRAME_TOO_LONG;
        }
        if (fr->len == 0u) {
            return FR_NONE;             /* ignore empty lines */
        }
        fr->line[fr->len] = '\0';
        fr->len = 0u;
        return FR_FRAME_READY;
    }

    if (fr->overflow) {
        return FR_NONE;                 /* discard until end of line */
    }
    if (fr->len >= PROTO_MAX_FRAME_LEN) {
        fr->overflow = true;
        return FR_NONE;
    }
    fr->line[fr->len] = (char)byte;
    fr->len++;
    return FR_NONE;
}

const char *frame_reader_line(const frame_reader_t *fr)
{
    return fr->line;
}
