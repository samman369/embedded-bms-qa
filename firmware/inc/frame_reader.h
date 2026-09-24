#ifndef FRAME_READER_H
#define FRAME_READER_H

/* Assembles UART bytes into '\n'-terminated lines and detects over-long frames. */

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

typedef enum {
    FR_NONE = 0,         /* keep feeding            */
    FR_FRAME_READY,      /* frame_reader_line() valid until the next feed */
    FR_FRAME_TOO_LONG    /* line exceeded PROTO_MAX_FRAME_LEN and was discarded */
} fr_result_t;

typedef struct {
    char     line[PROTO_MAX_FRAME_LEN + 1u];
    uint16_t len;
    bool     overflow;
} frame_reader_t;

void        frame_reader_init(frame_reader_t *fr);
fr_result_t frame_reader_feed(frame_reader_t *fr, uint8_t byte);
const char *frame_reader_line(const frame_reader_t *fr);

#endif /* FRAME_READER_H */
