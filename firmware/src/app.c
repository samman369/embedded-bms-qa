#include "app.h"
#include "bms.h"
#include "cmd_handler.h"
#include "frame_reader.h"
#include "hal.h"
#include "protocol.h"
#include "ring_buffer.h"

static uint8_t        s_rx_storage[APP_RX_BUFFER_SIZE];
static ring_buffer_t  s_rx;
static frame_reader_t s_reader;

static void send_payload(const char *payload)
{
    char frame[APP_RESP_MAX + 8u];
    if (protocol_encode(payload, frame, sizeof(frame)) > 0u) {
        hal_uart_write(frame);
    }
}

static void handle_frame(const char *frame)
{
    char payload[PROTO_MAX_FRAME_LEN + 1u];
    char resp[APP_RESP_MAX];

    switch (protocol_decode(frame, payload, sizeof(payload))) {
    case PROTO_OK:
        cmd_handle(payload, resp, sizeof(resp));
        send_payload(resp);
        break;
    case PROTO_ERR_CRC:
        send_payload("ERR,CRC");            /* REQ-COM-002 */
        break;
    case PROTO_ERR_LENGTH:
        send_payload("ERR,LENGTH");         /* REQ-COM-004 */
        break;
    case PROTO_ERR_FORMAT:
    default:
        send_payload("ERR,FORMAT");         /* REQ-COM-003 */
        break;
    }
}

void app_init(void)
{
    (void)rb_init(&s_rx, s_rx_storage, (uint16_t)sizeof(s_rx_storage));
    frame_reader_init(&s_reader);
    bms_init();
}

void app_uart_rx_isr(uint8_t byte)
{
    (void)rb_push(&s_rx, byte);
}

void app_poll(void)
{
    uint8_t byte;
    while (rb_pop(&s_rx, &byte)) {
        fr_result_t r = frame_reader_feed(&s_reader, byte);
        if (r == FR_FRAME_READY) {
            handle_frame(frame_reader_line(&s_reader));
        } else if (r == FR_FRAME_TOO_LONG) {
            send_payload("ERR,LENGTH");
        } else {
            /* keep collecting */
        }
    }
}

void app_tick(void)
{
    bms_tick();
}

uint16_t app_rx_overflow_count(void)
{
    return s_rx.overflow_count;
}
