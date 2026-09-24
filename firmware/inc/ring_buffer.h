#ifndef RING_BUFFER_H
#define RING_BUFFER_H

/*
 * Single-producer / single-consumer byte ring buffer (UART ISR -> main loop).
 * One slot is always kept empty, so a buffer of `size` bytes holds size - 1.
 * head is only written by the producer and tail only by the consumer, which
 * makes it safe without disabling interrupts.
 */

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t          *buf;
    uint16_t          size;
    volatile uint16_t head;
    volatile uint16_t tail;
    uint16_t          overflow_count;
} ring_buffer_t;

bool     rb_init(ring_buffer_t *rb, uint8_t *storage, uint16_t size);
void     rb_reset(ring_buffer_t *rb);
bool     rb_push(ring_buffer_t *rb, uint8_t byte);
bool     rb_pop(ring_buffer_t *rb, uint8_t *byte);
uint16_t rb_count(const ring_buffer_t *rb);
uint16_t rb_capacity(const ring_buffer_t *rb);
bool     rb_is_empty(const ring_buffer_t *rb);
bool     rb_is_full(const ring_buffer_t *rb);

#endif /* RING_BUFFER_H */
