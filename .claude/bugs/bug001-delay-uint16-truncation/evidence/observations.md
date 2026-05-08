# Observations — bug001-delay-uint16-truncation

## Symptom
`delay_us(us)` returns far too early when `us > 65535`. Example: `delay_us(70000)` waits ~4.4 ms instead of 70 ms.

## Reproduction
Call `delay_us(70000)` and measure elapsed time with a GPIO toggle + oscilloscope or serial timestamp.

## Root Cause Location
- File: `Application/delay/delay.c`, line 19–20
- `uint16_t start = (uint16_t)tim_base_get_count(tim_handle);`
- `while ((uint16_t)(tim_base_get_count(tim_handle) - start) < (uint16_t)us)`

## Why it Truncates
`(uint16_t)70000` = 70000 % 65536 = **4464**. The loop exits after 4464 µs, not 70000 µs.

## Current Callers (all safe today)
- 1-Wire delays: max 480 µs (`TIME_DELAY_H`)
- `delay_ms`: loops `delay_us(1000)` — latent, never triggers bug

## Hardware Context
TIM14, ARR = 0xFFFF, 1 MHz → 16-bit counter, 1 tick = 1 µs, max span = 65535 µs.
