# Outcome — bug001-delay-uint16-truncation

## Status
fixed

## Changes Made
`Application/delay/delay.c` lines 19–23: replaced single spin-loop with a chunked outer loop. Each iteration waits at most 65535 µs using correct 16-bit modular arithmetic, then subtracts from `us` until done.

## Verification Checklist
- [ ] `python build.py` compiles without errors
- [ ] 1-Wire communication still functional (DS18B20 reads temperature correctly)
- [ ] `delay_ms` behavior unchanged
- [ ] `delay_us(70000)` observed to delay ~70 ms (oscilloscope or serial timestamp)
