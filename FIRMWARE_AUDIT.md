# Firmware Audit — STM32C071 Thermal Controller

Nine open findings from the adversarial static analysis. Sorted by priority.
Closed and false-positive entries have been removed; consult git history for
the investigation record.

## Summary

| Priority | # | Area | One-line issue |
|---|---|---|---|
| Critical | 9 | Safety | `SystemFault` kills fans — fail-unsafe cooling on sensor loss |
| High | 8 | State | No hysteresis on `SystemFault → SystemRunning` recovery |
| High | 13 | Logic | Command-buffer overflow tail bytes execute as a new command |
| Medium | 14 | Logic | Implicit 0.99°C dead band in thermal threshold comparisons |
| Medium | 5 | Architecture | Dual tick sources (`millis()` vs `HAL_GetTick()`) |
| Medium | 15 | Fragility | Fan tacho EXTI matches on pin number only, not port |
| Medium | 16 | Code Quality | `ow_sem_lock()` provides no real mutual exclusion |
| Medium | 19 | Code Quality | Dead 50% throttle default inside `pwm_repeater_init()` |
| Low | 20 | Margin | IWDG headroom adequate today, shrinks with future features |

## Recommended order

1. **#9** — fail-unsafe fans on sensor loss. Safety defect.
2. **#8** — fault-recovery hysteresis. Prevents fan on/off oscillation.
3. **#13** — command-buffer tail-accumulation. Blocks garbage-command execution.
4. **#14** — centidegree dead band. Eliminates user-visible threshold mismatch.
5. **#5** — tick-source consolidation. Architectural correctness.
6. **#15, #16, #19, #20** — code quality / margin items, low urgency.

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

## #14 — Implicit 0.99°C dead band in thermal comparisons

**Severity:** Medium · **Category:** Logic · **Location:** [`thermal_control.c compute_thresholds()` / `thermal_control_step()`](Application/thermal_control/thermal_control.c) · **Confidence:** High

`t_cdeg / 100` truncates toward zero. With `fan_on = 35`, the state machine needs `t_cdeg >= 3500` to enter `ThermalHigh`. At `t_cdeg = 3499` (34.99°C), `t_deg = 34`, so `34 >= 35` is false — fans stay off. The implicit 0.99°C dead band on every upward transition is invisible at lab precision.

The user-facing inconsistency: [`telemetry.c cdeg_to_deg_rounded()`](Application/telemetry/telemetry.c#L29) **rounds** the displayed value, while `thermal_control` **truncates** internally. An operator sees telemetry reporting 35°C with fans still off, files a defect, and reproduces "the threshold doesn't work."

**Fix — compare in centidegrees, no truncation:**
```c
typedef struct {
    int16_t fan_on_cdeg;      // settings.temp_fan_on * 100
    int16_t fan_off_cdeg;
    int16_t throttle_on_cdeg;
    int16_t crit_on_cdeg;
    int16_t crit_off_cdeg;
    int16_t throttle_off_cdeg;
} ThresholdSet;
```
All comparisons against `t_cdeg` directly; the truncation step disappears.

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

## #16 — `ow_sem_lock()` is a dead abstraction

**Severity:** Medium · **Category:** Code Quality · **Location:** [`onewire.c ow_sem_lock()` / `ow_sem_unlock()`](Application/onewire/onewire.c) · **Confidence:** High

`ow_sem_lock()` enters a critical section, increments `lock_count`, exits the critical section. IRQs are disabled for just the increment, then immediately re-enabled. The counter provides **no** mutual exclusion. The real timing protection comes from inner `ENTER_CRITICAL()` / `EXIT_CRITICAL()` calls inside `ow_write_bit()` and `ow_read_bit()`.

No current runtime defect — the cooperative scheduler doesn't preempt 1-Wire operations anyway. The risk is that a future developer reads `ow_sem_lock()`, trusts the abstraction, and removes the inner critical sections, leaving timing unprotected.

Adjacent observation: [`ds18b20.c ds18b20_begin()`](Application/ds18b20/ds18b20.c) calls `ow_init()` on every `StatePoll` cycle, reconfiguring the GPIO pin once per second for no reason.

**Fix:** Remove `ow_sem_lock()` / `ow_sem_unlock()` (or replace with a real assertion that no 1-Wire operation is in progress). Document that the inner critical sections are the authoritative timing guards. Move `ow_init()` out of `ds18b20_begin()` so it runs once at boot.

---

## #19 — Dead throttle init in `pwm_repeater_init()`

**Severity:** Medium · **Category:** Code Quality · **Location:** [`pwm_repeater.c pwm_repeater_init()`](Application/pwm_repeater/pwm_repeater.c), [`main.c`](Application/main.c) · **Confidence:** High

`pwm_repeater_init()` calls `pwm_set_throttle_a(50)` / `pwm_set_throttle_b(50)` at the end. `main.c` then immediately overrides with `pwm_set_throttle_a(100U)` / `pwm_set_throttle_b(100U)`. The 50% default is never the operational value.

If someone removes the `main.c` lines thinking "init already sets a sane default," operational throttle silently becomes 50%.

**Fix:** Drop the throttle calls from `pwm_repeater_init()`. Init should set the hardware to a known-off state only; throttle is application policy and belongs in the caller, where it already is.

---

## #20 — IWDG margin

**Severity:** Low (revised) · **Category:** Margin · **Location:** [`watchdog.c watchdog_init()`](Application/watchdog/watchdog.c) · **Confidence:** Low

`WATCHDOG_PRESCALER = IWDG_PRESCALER_32`, `WATCHDOG_RELOAD = 1999` → nominal 2.0 s timeout. STM32C0 LSI tolerance per datasheet (DS13560) is ~±10% over V/T range:
- LSI = 35.2 kHz (+10%): timeout ≈ 1.82 s.
- LSI = 28.8 kHz (−10%): timeout ≈ 2.22 s.

Worst-case main loop is ≈ 1.1 s (DS18B20 conversion dominates), giving ≈ 700 ms margin at the fast end of LSI tolerance. Adequate today; shrinks with each feature added to the loop.

**Fix (optional):** Widen to ~3 s with `WATCHDOG_RELOAD = 2999` for a clean 2× margin even at +10% LSI. Document worst-case loop iteration alongside so future additions can be checked against the budget.
