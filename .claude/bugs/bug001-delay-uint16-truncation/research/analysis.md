# Analysis — bug001-delay-uint16-truncation

## Root Cause
The function signature accepts `uint32_t us` but the implementation narrows it to `uint16_t` in two places:
1. `uint16_t start` — benign for TIM14 (16-bit hardware counter, return value is always ≤ 65535)
2. `(uint16_t)us` in the loop condition — **the bug**: silently truncates `us` modulo 65536

## Why the Modular Arithmetic Is Otherwise Correct
The expression `(uint16_t)(current - start)` correctly handles a single timer wrap-around due to unsigned subtraction rules. This is the standard "free-running timer" pattern — valid as long as the delay duration ≤ 65535 µs.

## Why the Bug Is Latent
All current callers pass values ≤ 1000 µs, so the truncation of `(uint16_t)us` has no visible effect today. It will only manifest if someone calls `delay_us` with a value > 65535 µs directly.

## Fix Strategy
Loop in chunks of ≤ 65535 µs using the existing modular arithmetic. This extends the function to the full `uint32_t` range without changing the hardware approach.

Chunk = 65535 is mathematically safe: starting at `start`, the inner loop exits when elapsed = 65535, which occurs exactly 65535 µs later regardless of whether the counter wrapped once during that span.
