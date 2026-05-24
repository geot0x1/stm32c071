# Firmware Audit — STM32C071 Thermal Controller

Five open findings from the adversarial static analysis. Sorted by priority.

## Summary

| Priority | # | Area | One-line issue |
|---|---|---|---|
| High | 8 | State | No hysteresis on `SystemFault → SystemRunning` recovery |
| High | 13 | Logic | Command-buffer overflow tail bytes execute as a new command |
| Medium | 5 | Architecture | Dual tick sources (`millis()` vs `HAL_GetTick()`) |
| Medium | 15 | Fragility | Fan tacho EXTI matches on pin number only, not port |
| Low | 20 | Margin | IWDG headroom adequate today, shrinks with future features |

## Recommended order

1. **#8** — fault-recovery hysteresis. Prevents fan on/off oscillation. **[HIGH]**
2. **#13** — command-buffer tail-accumulation. Blocks garbage-command execution. **[HIGH]**
3. **#5** — tick-source consolidation. Architectural correctness. **[MEDIUM]**
4. **#15, #20** — code quality / margin items, low urgency. **[MEDIUM, LOW]**

---

## #8 — No hysteresis on fault recovery

**Severity:** High · **Category:** Logic / State · **Location:** [`main.c handle_fault()`](Application/main.c), [`app_state.c app_state_enter_running()`](Application/app_state/app_state.c) · **Confidence:** High

`SystemFault` exits to `SystemRunning` on the *first* valid sensor reading. An intermittent sensor (loose 1-Wire wiring, noisy bus) toggles the state every 2-3 s: valid reading → `SystemRunning` → next poll fails → `SystemFault` → next poll valid → back to `SystemRunning`. Fans cycle on/off, PWM throttle engages and disengages, the program LED alternates between error and thermal patterns.

Permanent recovery only happens in a lab. Field installations with marginal cabling see this immediately.

**Fix — require N consecutive valid readings:**
```c
static uint8_t s_recovery_count = 0;

// In handle_fault():
if (t != INT16_MIN)
{
    s_recovery_count++;
    if (s_recovery_count >= 3)
    {
        s_recovery_count = 0;
        app_state_enter_running(thermal_control_initial(t, settings_get()));
    }
}
else
{
    s_recovery_count = 0;
}
```
Three consecutive valid readings (≈ 6-9 s) before exiting fault.

---

## #13 — Command-buffer overflow tail-accumulation

**Severity:** High · **Category:** Logic · **Location:** [`commands.c usb_read_line()` / `linebuffer_append_byte()`](Application/commands/commands.c) · **Confidence:** High

A line longer than `CMD_LINE_BUF_SIZE - 1 = 127` bytes arrives. `linebuffer_append_byte()` detects overflow, clears `len`, sends `ERR OVERFLOW`. The next byte then *starts a new line* at position 0. When the eventual `\n` arrives, the accumulated tail of the original overlong payload is dispatched to `process_line()` as if it were a fresh command.

If the tail happens to match a valid command (e.g., the last 9 bytes are `SETTINGS?`), it executes. Low probability for human typists; nonzero for automated hosts or USB framing glitches.

**Fix — discard everything up to the next `\n` after overflow:**
```c
typedef struct {
    char     buf[CMD_LINE_BUF_SIZE];
    uint16_t len;
    bool     overflow_discard;   // ADD
} CommandLineBuffer;
```
On overflow: set `overflow_discard = true` and clear the buffer. In `linebuffer_append_byte()`: if `overflow_discard` is set, drop the byte. Clear the flag only when `\n` arrives.

---

## #5 — Dual tick sources

**Severity:** Medium (revised) · **Category:** Architecture · **Location:** [`sys_time.c millis()`](Application/sys_time/sys_time.c), all modules using `HAL_GetTick()` · **Confidence:** High

Two redundant tick counters: `millis()` (incremented by `sys_time_handler()` in `sys_time.c`) and `HAL_GetTick()` (HAL's `uwTick`). Both are bumped from the same `SysTick_Handler`, so they stay in sync — but they're independent counters maintained side by side.

| Counter | Users |
|---|---|
| `millis()` | `main.c`, `pwm_repeater.c`, `hdc2010.c`, `telemetry.c`, `push_button.c` |
| `HAL_GetTick()` | `temperature_sensor.c`, `program_led.c`, `fan_tacho.c` |

**Verified:** `Core/Src/stm32c0xx_it.c:134` does call `sys_time_handler()` from `SysTick_Handler` — both counters work today.

**Latent risk:** A CubeMX regeneration that drops the USER CODE block in `stm32c0xx_it.c` freezes `millis()` at 0. Worst case: `pwm_repeater_task()`'s 50 ms capture-timeout (which fails the PWM output to a safe state on signal loss) never fires, leaving stale output.

**Fix:** Consolidate to one tick source — replace all `HAL_GetTick()` calls in application code with `millis()`, or vice versa, and delete the redundant module. Removes the regen-time dependency.

---

## #15 — Fan tacho EXTI matches pin number only

**Severity:** Medium (revised) · **Category:** Fragility · **Location:** [`fan_tacho.c HAL_GPIO_EXTI_Falling_Callback()`](Application/fan_tacho/fan_tacho.c) · **Confidence:** High

The EXTI callback matches `tacho[i].gpio.pin == GPIO_Pin` only. If two fans were assigned the same pin number on different ports (e.g., PA7 and PC7), the callback would misattribute the pulse to whichever fan was listed first in the table.

The current board (`PC7`, `PB14`, `PA5`, `PC15` — all distinct pin numbers, verified in [`board_config.h:142-149`](Application/board/board_config.h)) is safe. A future pin reassignment with overlap would silently break: no compile-time or runtime indicator.

**Note on what was retracted:** the original finding also claimed a torn read on `last_pulse_ms` / `prev_pulse_ms`. Verified false — `fan_tacho.c:103-110` wraps both reads in `__disable_irq()` / `__enable_irq()`, so the ISR cannot fire between them on Cortex-M0+. Only the port-match fragility remains.

**Fix:** Match on both pin and port:
```c
if (tacho[i].gpio.pin == GPIO_Pin && tacho[i].gpio.port == source_port)
```
HAL's callback signature only delivers the pin, so the port has to be inferred from EXTI pending bits or looked up from a table at callback time.

---

## #20 — IWDG margin

**Severity:** Low (revised) · **Category:** Margin · **Location:** [`watchdog.c watchdog_init()`](Application/watchdog/watchdog.c) · **Confidence:** Low

`WATCHDOG_PRESCALER = IWDG_PRESCALER_32`, `WATCHDOG_RELOAD = 1999` → nominal 2.0 s timeout. STM32C0 LSI tolerance per datasheet (DS13560) is ~±10% over V/T range:
- LSI = 35.2 kHz (+10%): timeout ≈ 1.82 s.
- LSI = 28.8 kHz (−10%): timeout ≈ 2.22 s.

Worst-case main loop is ≈ 1.1 s (DS18B20 conversion dominates), giving ≈ 700 ms margin at the fast end of LSI tolerance. Adequate today; shrinks with each feature added to the loop.

**Fix (optional):** Widen to ~3 s with `WATCHDOG_RELOAD = 2999` for a clean 2× margin even at +10% LSI. Document worst-case loop iteration alongside so future additions can be checked against the budget.
