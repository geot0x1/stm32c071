#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "app_state.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define TELEMETRY_DEFAULT_INTERVAL_MS 1000U
#define TELEMETRY_MIN_INTERVAL_MS 100U
#define TELEMETRY_MAX_INTERVAL_MS 3600000U
#define TELEMETRY_BUF_SIZE 96U

void telemetry_init(void);
void telemetry_task(void);
void telemetry_create(char *buf, size_t buf_size);
void telemetry_send(void);
void telemetry_reset(void);
void telemetry_enable(bool enable);
bool telemetry_is_enabled(void);
void telemetry_set_interval_ms(uint32_t ms);
uint32_t telemetry_get_interval_ms(void);

#endif /* TELEMETRY_H */
