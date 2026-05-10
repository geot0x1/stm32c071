#ifndef FIFO_H
#define FIFO_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint8_t  *buf;
    uint16_t  capacity;
    uint16_t  head;
    uint16_t  tail;
    uint16_t  count;
} Fifo;

void     fifo_init(Fifo *f, uint8_t *buf, uint16_t capacity);
bool     fifo_push(Fifo *f, uint8_t byte);
bool     fifo_push_array(Fifo *f, const uint8_t *data, uint16_t len);
bool     fifo_pop(Fifo *f, uint8_t *out);
uint16_t fifo_pop_array(Fifo *f, uint8_t *out, uint16_t len);
bool     fifo_peek(const Fifo *f, uint8_t *out);
bool     fifo_is_empty(const Fifo *f);
bool     fifo_is_full(const Fifo *f);
uint16_t fifo_count(const Fifo *f);

#endif /* FIFO_H */
