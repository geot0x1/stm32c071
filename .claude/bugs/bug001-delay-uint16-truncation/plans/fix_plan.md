# Fix Plan — bug001-delay-uint16-truncation

## File to Modify
`Application/delay/delay.c` — lines 19–23 (body of `delay_us`)

## Change
Replace the single spin-loop with a chunked outer loop:

```c
void delay_us(uint32_t us)
{
    if (tim_handle == NULL || us == 0)
    {
        return;
    }

    while (us > 0)
    {
        uint16_t chunk = (us >= 65535U) ? 65535U : (uint16_t)us;
        uint16_t start = (uint16_t)tim_base_get_count(tim_handle);
        while ((uint16_t)(tim_base_get_count(tim_handle) - start) < chunk)
        {
            __NOP();
        }
        us -= chunk;
    }
}
```

## No Other Files Need Changing
- `delay.h` — function signature `uint32_t us` is already correct
- `delay_ms` — already loops 1 ms at a time, unaffected
- All callers — pass ≤ 480 µs, unaffected

## Constraints
- Must not use `TIM_EGR_UG` or reset the counter (per CLAUDE.md critical rules)
- Must preserve 16-bit modular arithmetic (hardware is 16-bit)
