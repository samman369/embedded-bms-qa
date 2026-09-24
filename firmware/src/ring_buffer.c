#include "ring_buffer.h"
#include <stddef.h>

static uint16_t next_index(const ring_buffer_t *rb, uint16_t index)
{
    return (uint16_t)((index + 1u) % rb->size);
}

bool rb_init(ring_buffer_t *rb, uint8_t *storage, uint16_t size)
{
    if ((rb == NULL) || (storage == NULL) || (size < 2u)) {
        return false;
    }
    rb->buf = storage;
    rb->size = size;
    rb_reset(rb);
    return true;
}

void rb_reset(ring_buffer_t *rb)
{
    if (rb == NULL) {
        return;
    }
    rb->head = 0u;
    rb->tail = 0u;
    rb->overflow_count = 0u;
}

bool rb_push(ring_buffer_t *rb, uint8_t byte)
{
    if (rb == NULL) {
        return false;
    }
    uint16_t next = next_index(rb, rb->head);
    if (next == rb->tail) {
        if (rb->overflow_count < UINT16_MAX) {
            rb->overflow_count++;
        }
        return false;
    }
    rb->buf[rb->head] = byte;
    rb->head = next;
    return true;
}

bool rb_pop(ring_buffer_t *rb, uint8_t *byte)
{
    if ((rb == NULL) || (byte == NULL) || (rb->head == rb->tail)) {
        return false;
    }
    *byte = rb->buf[rb->tail];
    rb->tail = next_index(rb, rb->tail);
    return true;
}

uint16_t rb_count(const ring_buffer_t *rb)
{
    if (rb == NULL) {
        return 0u;
    }
    return (uint16_t)((rb->head + rb->size - rb->tail) % rb->size);
}

uint16_t rb_capacity(const ring_buffer_t *rb)
{
    return (rb == NULL) ? 0u : (uint16_t)(rb->size - 1u);
}

bool rb_is_empty(const ring_buffer_t *rb)
{
    return rb_count(rb) == 0u;
}

bool rb_is_full(const ring_buffer_t *rb)
{
    return (rb != NULL) && (rb_count(rb) == rb_capacity(rb));
}
