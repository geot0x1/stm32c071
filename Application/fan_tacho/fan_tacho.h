#ifndef FAN_TACHO_H
#define FAN_TACHO_H

#include <stdint.h>
#include <stdbool.h>

#define FAN_TACHO_COUNT 4U

void fan_tacho_init(uint8_t fan_idx);
void fan_tacho_enable(uint8_t fan_idx);
void fan_tacho_disable(uint8_t fan_idx);
uint32_t fan_tacho_get_rpm(uint8_t fan_idx);

#endif /* FAN_TACHO_H */
