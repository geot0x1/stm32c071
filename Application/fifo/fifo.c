#include "fifo.h"
#include <stddef.h>

void fifo_init(Fifo *f, uint8_t *buf, uint16_t capacity)
{
    if (f == NULL)
    {
        return;
    }

    f->buf = buf;
    f->capacity = capacity;
    f->head = 0;
    f->tail = 0;
    f->count = 0;
}

bool fifo_push(Fifo *f, uint8_t byte)
{
    if (f == NULL || fifo_is_full(f))
    {
        return false;
    }

    f->buf[f->head] = byte;
    f->head = (f->head + 1) % f->capacity;
    f->count++;

    return true;
}

bool fifo_push_array(Fifo *f, const uint8_t *data, uint16_t len)
{
    if (f == NULL || data == NULL || len == 0)
    {
        return false;
    }

    if (f->count + len > f->capacity)
    {
        return false;
    }

    for (uint16_t i = 0; i < len; i++)
    {
        f->buf[f->head] = data[i];
        f->head = (f->head + 1) % f->capacity;
    }

    f->count += len;
    return true;
}

bool fifo_pop(Fifo *f, uint8_t *out)
{
    if (f == NULL || out == NULL || fifo_is_empty(f))
    {
        return false;
    }

    *out = f->buf[f->tail];
    f->tail = (f->tail + 1) % f->capacity;
    f->count--;

    return true;
}

uint16_t fifo_pop_array(Fifo *f, uint8_t *out, uint16_t len)
{
    if (f == NULL || out == NULL || len == 0)
    {
        return 0;
    }

    uint16_t bytes_to_pop = (f->count < len) ? f->count : len;

    for (uint16_t i = 0; i < bytes_to_pop; i++)
    {
        out[i] = f->buf[f->tail];
        f->tail = (f->tail + 1) % f->capacity;
    }

    f->count -= bytes_to_pop;
    return bytes_to_pop;
}

bool fifo_peek(const Fifo *f, uint8_t *out)
{
    if (f == NULL || out == NULL || fifo_is_empty(f))
    {
        return false;
    }

    *out = f->buf[f->tail];
    return true;
}

bool fifo_is_empty(const Fifo *f)
{
    if (f == NULL)
    {
        return true;
    }

    return f->count == 0;
}

bool fifo_is_full(const Fifo *f)
{
    if (f == NULL)
    {
        return false;
    }

    return f->count >= f->capacity;
}

uint16_t fifo_count(const Fifo *f)
{
    if (f == NULL)
    {
        return 0;
    }

    return f->count;
}
