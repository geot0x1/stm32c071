# Firmware Audit — STM32C071 Thermal Controller

Six open findings from the adversarial static analysis. Sorted by priority.
Closed findings are marked as **CLOSED** with resolution details below the summary table.

## Summary

| Priority | # | Area | One-line issue |
|---|---|---|---|
| Critical | 9 | Safety | `SystemFault` kills fans — fail-unsafe cooling on sensor loss |
| High | 8 | State | No hysteresis on `SystemFault → SystemRunning` recovery |
| High | 13 | Logic | Command-buffer overflow tail bytes execute as a new command |
| Medium | 5 | Architecture | Dual tick sources (`millis()` vs `HAL_GetTick()`) |
| Medium | 15 | Fragility | Fan tacho EXTI matches on pin number only, not port |
| Low | 20 | Margin | IWDG headroom adequate today, shrinks with future features |

## Recommended order

1. **#9** — fail-unsafe fans on sensor loss. Safety defect.
2. **#8** — fault-recovery hysteresis. Prevents fan on/off oscillation.
3. **#13** — command-buffer tail-accumulation. Blocks garbage-command execution.
4. **#5** — tick-source consolidation. Architectural correctness.
5. **#15, #20** — code quality / margin items, low urgency.

---

## Closed Findings

- **#14** — Implicit 0.99°C dead band in thermal comparisons — [see resolution below](#14--implicit-099c-dead-band-in-thermal-comparisons--closed)
- **#16** — `ow_sem_lock()` dead abstraction — [see resolution below](#16--ow_sem_lock-is-a-dead-abstraction--closed)
- **#19** — Dead 50% throttle default in `pwm_repeater_init()` — [see resolution below](#19--dead-throttle-init-in-pwm_repeater_init--closed)

---

## #9 — `SystemFault` kills fans (fail-unsafe)

**Severity:** Critical · **Category:** Safety · **Location:** [`main.c apply_fans()`](Application/main.c) · **Confidence:** High

In `SystemFault` (entered on temperature-sensor loss), `auto_on = (state == SystemRunning) && (thermal == …)` evaluates to false, so the auto-drive path stops every fan. Without the manual button held, every fan goes OFF — exactly when a hot component most needs cooling.

The combination of *thermal emergency* + *sensor fault* is never tested. Lab QA either raises temperature with sensors healthy, or pulls the sensor at ambient. Real-world trigger: component at 65°C (above `temp_critical`), an ESD event or loose 1-Wire connector disrupts the sensor, system enters SystemFault, fans stop, component keeps heating.

**Fix — fail-safe to full cooling:**
```c
static void apply_fans(SystemState state, ThermalState thermal, bool button_pressed)
{
    if (state == SystemFault)
    {
        fan_control_all_on();   // fail-safe on sensor loss
        return;
    }
    // ... existing logic ...
}
```
Alternative: persist the last valid `ThermalState` on fault entry and continue driving fans by that state until recovery.

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

## #14 — Implicit 0.99°C dead band in thermal comparisons — **CLOSED**

**Severity:** Medium · **Category:** Logic · **Status:** Fixed (commit TBD)

**Original Issue:**
`t_cdeg / 100` truncated toward zero, creating an implicit 0–0.99°C dead band on upward transitions. With `fan_on = 35`, at `t_cdeg = 3499` (34.99°C), the truncated value was 34, so fans stayed off even near threshold. Compounded by telemetry rounding (not truncating), users saw "35°C reported but fans still off."

**Resolution:**
Replaced truncation with rounding via `cdeg_to_deg_rounded()` static inline function in [`thermal_control.c`](Application/thermal_control/thermal_control.c). Sensor readings are now rounded to nearest whole degree before threshold comparison, introducing a symmetric ±0.5°C dead band (acceptable for thermal control). Added design documentation noting whole-degree resolution and acceptable dead-band margin.

**Implementation:**
- Line 10–13: Static inline `cdeg_to_deg_rounded(cdeg)` — rounds centidegrees to nearest degree (matches telemetry logic)
- Line 113: `compute_thresholds()` uses rounding function for sensor value
- Lines 3–5: Design note documenting whole-degree resolution and ±0.5°C dead band justification

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

## #16 — `ow_sem_lock()` is a dead abstraction — **CLOSED**

**Severity:** Medium · **Category:** Code Quality · **Status:** Not Applicable

**Original Issue:**
`ow_sem_lock()` / `ow_sem_unlock()` provided no real mutual exclusion — it only disabled IRQs to increment a counter, then re-enabled them. The actual timing protection came from inner `ENTER_CRITICAL()` / `EXIT_CRITICAL()` calls in `ow_write_bit()` / `ow_read_bit()`. Risk was that a future developer would trust the abstraction and remove the inner critical sections.

**Resolution:**
Not applicable. This architecture uses a bare-metal cooperative scheduler with no RTOS tasks and no IRSs calling 1-Wire code. The 1-Wire operations are never preempted, so the lock serves no purpose and poses no risk. The inner critical sections are the sole synchronization mechanism and are sufficient. The lock functions remain but are understood to be non-functional in this context by design.

**Note:** Adjacent observation about `ds18b20_begin()` calling `ow_init()` on every poll cycle (reconfiguring GPIO unnecessarily) remains valid but is a separate optimization concern, not a correctness defect.

---

## #19 — Dead throttle init in `pwm_repeater_init()` — **CLOSED**

**Severity:** Medium · **Category:** Code Quality · **Status:** Fixed

**Original Issue:**
`pwm_repeater_init()` set throttle to 50%, which was immediately overridden to 100% in `main.c` (lines 222–223). Dead code risk: if someone removed the `main.c` overrides believing init had set a sane default, operational throttle would silently become 50%.

**Resolution:**
Changed `pwm_repeater_init()` to initialize throttle to 100% directly:
- **pwm_repeater.c line 48–49:** Global `PwmOutput` instances now initialize `.throttle_val = 100` instead of 50
- **pwm_repeater.c line 120–121:** `pwm_repeater_init()` calls `pwm_set_throttle_a(100)` / `pwm_set_throttle_b(100)` instead of 50
- **main.c line 222–223:** Removed redundant overrides to 100 — init now sets the correct value directly

**Fix:** Drop the throttle calls from `pwm_repeater_init()`. Init should set the hardware to a known-off state only; throttle is application policy and belongs in the caller, where it already is.

---

## #20 — IWDG margin

**Severity:** Low (revised) · **Category:** Margin · **Location:** [`watchdog.c watchdog_init()`](Application/watchdog/watchdog.c) · **Confidence:** Low

`WATCHDOG_PRESCALER = IWDG_PRESCALER_32`, `WATCHDOG_RELOAD = 1999` → nominal 2.0 s timeout. STM32C0 LSI tolerance per datasheet (DS13560) is ~±10% over V/T range:
- LSI = 35.2 kHz (+10%): timeout ≈ 1.82 s.
- LSI = 28.8 kHz (−10%): timeout ≈ 2.22 s.

Worst-case main loop is ≈ 1.1 s (DS18B20 conversion dominates), giving ≈ 700 ms margin at the fast end of LSI tolerance. Adequate today; shrinks with each feature added to the loop.

**Fix (optional):** Widen to ~3 s with `WATCHDOG_RELOAD = 2999` for a clean 2× margin even at +10% LSI. Document worst-case loop iteration alongside so future additions can be checked against the budget.
