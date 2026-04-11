#include "delay.h"
#include "tim.h"
#include <stddef.h>

static Tim *tim_handle = NULL;

void delay_init(Tim *tim) { tim_handle = tim; }

void delay_us(uint32_t us) {
  if (tim_handle == NULL || us == 0) {
    return;
  }

  uint16_t start = (uint16_t)tim_base_get_count(tim_handle);
  while ((uint16_t)(tim_base_get_count(tim_handle) - start) < (uint16_t)us) {
    __NOP();
  }
}

void delay_ms(uint32_t ms) {
  while (ms--) {
    delay_us(1000);
  }
}
