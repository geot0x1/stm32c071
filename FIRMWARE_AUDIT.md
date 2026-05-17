# Firmware Audit — STM32C071 Thermal Controller

Adversarial static analysis audit. Findings #1 and #2 have been applied and
closed. Findings #10, #11, #12, and #18 were determined to be false positives
after verification and are excluded. Findings #5, #15, and #20 were partially
revised after verification (see notes inline). All remaining findings are open
for implementation.

---

## Open Findings

---

### Finding #3

**Status:** Open
**Severity:** High
**Category:** Logic Error / State Bug
**Location:** `commands.c` — `parse_pwm_throttle_temp()`, `settings.c` — `settings_set_temp_throttle_on()`

**Trigger Conditions:**
Send `PWMTHRTEMP=32` with defaults `fan_off=30`, `fan_on=35`, `critical=60`. The setter checks: `32 != 255` ✓, `32 > fan_off(30)` ✓, `32 < critical(60)` ✓ — **accepted**. The invariant `fan_on < throttle_on` is now violated (`35 > 32`).

**Why Lab Testing Misses It:** Normal QA sends `PWMTHRTEMP` to values visibly above `fan_on`. A value in the dead zone between `fan_off` and `fan_on` is never tested.

**Failure Behavior:** With `throttle_on=32` and `fan_on=35`: at 33°C the system is in `ThermalHigh` (fans on), and the check `t_deg >= throttle_on(32)` is true, so it transitions to `ThermalThrottling`. PWM is capped below `fan_on` temperature, permanently throttling display or fan PWM in a zone where no throttling should occur. Exit from `ThermalThrottling` requires `t_deg < throttle_off(30)`, which coincides with `fan_off` — creating a state where throttle engages below `fan_on` and cannot exit until fans should be off entirely.

**Root Cause:** `settings_set_temp_throttle_on()` validates against `fan_off` and `critical` but not `fan_on`. The bulk setter `settings_set_all()` has the correct full ordering check; individual setters are inconsistent.

The same class of defect exists symmetrically:
- `settings_set_temp_fan_on()` should also check `temp_deg < current.temp_throttle_on`
- `settings_set_temp_critical()` should check `temp_deg > current.temp_throttle_on`

**Fix:**
```c
bool settings_set_temp_throttle_on(uint8_t temp_deg)
{
    if (temp_deg == SETTINGS_TEMP_INVALID
        || temp_deg <= current.temp_fan_on      // ADD: enforce fan_on < throttle_on
        || temp_deg >= current.temp_critical)
    {
        return false;
    }
    current.temp_throttle_on = temp_deg;
    return settings_save();
}
```
Apply the same pattern to the other individual setters to close all ordering gaps.

**Confidence:** High

---

### Finding #4

**Status:** Open
**Severity:** High
**Category:** Logic Error
**Location:** `commands.c` — `parse_pwm_throttle_temp()`

**Trigger Conditions:**
`PWMTHRTEMP=<any_value>` bypasses all range validation in the command handler. The only gate is inside `settings_set_temp_throttle_on()`. Compare to `parse_fan_temp_on()` which validates `val >= CMD_TEMPON_MIN && val <= CMD_TEMPON_MAX` before calling the setter.

**Why Lab Testing Misses It:** Normal QA uses valid values. The response difference (`ERR SAVE_FAILED` vs `ERR OUT_OF_RANGE`) is only visible with deliberate edge-case input.

**Failure Behavior:** Invalid input returns `ERR SAVE_FAILED` instead of a meaningful `ERR OUT_OF_RANGE`. Field diagnostics are misleading — an operator reading the log cannot distinguish a flash failure from an out-of-range command.

**Root Cause:** Missing bounds check in `parse_pwm_throttle_temp()` before calling the setter. Inconsistent pattern compared to the other command parsers.

**Fix:** Add explicit range validation in `parse_pwm_throttle_temp()` mirroring what `parse_fan_temp_on()` does. Define `CMD_PWMTHRTEMP_MIN` and `CMD_PWMTHRTEMP_MAX` constants and validate before the setter call.

**Confidence:** High

---

### Finding #5

**Status:** Open (revised after verification)
**Severity:** Medium (downgraded from High)
**Category:** Architecture / Code Quality
**Location:** `sys_time.c` — `millis()`, all modules using `HAL_GetTick()`

**Verification Note:** The original finding claimed the `sys_time_handler()` wiring "cannot be verified." It **can** be verified: `Core/Src/stm32c0xx_it.c:134` does call `sys_time_handler()` from `SysTick_Handler`, so `millis()` is correctly incremented. The described failure (PWM timeout never fires) is **not active** in current code. What remains is a code-quality / architectural concern.

**Trigger Conditions:**
The firmware maintains two parallel tick sources: `millis()` (backed by `systemTick` in `sys_time.c`) and `HAL_GetTick()` (backed by HAL's `uwTick`). Both are incremented from the same `SysTick_Handler`, so they stay in sync — but they are independent counters maintained side-by-side. Module usage is split:
- `millis()`: `main.c`, `pwm_repeater.c`, `hdc2010.c`, `telemetry.c`, `push_button.c`
- `HAL_GetTick()`: `temperature_sensor.c`, `program_led.c`, `fan_tacho.c`

**Why Lab Testing Misses It:** Both counters work correctly today, so behavior is indistinguishable from a single-source design.

**Failure Behavior:** None today. The latent risk is that a future change to `stm32c0xx_it.c` (e.g., regenerating from CubeMX without re-applying the USER CODE section) could drop the `sys_time_handler()` call. Then `millis()` would freeze at 0 and any module that uses it for timeouts would silently misbehave — most dangerously `pwm_repeater_task()`, where the 50 ms capture-timeout that fails the PWM output to a safe state would never fire.

**Root Cause:** Two redundant tick sources with a brittle dependency on a CubeMX-regenerated file.

**Fix:** Consolidate to one tick source. Replace all `HAL_GetTick()` calls in application code with `millis()` (or vice versa) and remove the redundant module. Eliminates the regen-time risk.

**Confidence:** High (the duplication exists and is verified); risk is latent.

---

### Finding #6

**Status:** Open
**Severity:** High
**Category:** Race / Re-entrancy
**Location:** `usb.c` — `usb_write()`, `commands.c` — `handle_reset()`

**Trigger Conditions:**
`usb_write()` calls `usb_task()` internally when the TX FIFO has less than 64 bytes free. `usb_task()` calls `tud_task()`, which processes all pending USB events including incoming CDC RX data. `handle_reset()` is called from within `process_line()`, which is already inside the command processing call stack. `handle_reset()` calls `usb_task()` 10 times in a 100 ms drain loop.

If USB RX data arrives during the drain loop, `tud_task()` processes it and may invoke CDC RX callbacks. Depending on tinyUSB configuration, this can re-enter the command assembly path while `lineBuf` is in an intermediate state.

**Why Lab Testing Misses It:** RESET is tested as a one-shot with no immediate follow-up data. The 100 ms window is short enough that no additional data arrives in a human-operated lab session.

**Failure Behavior:** An automated host sends RESET then immediately floods the port. During the 100 ms drain, the flood data is processed by `tud_task()` inside the drain loop, potentially re-entering the command dispatcher and corrupting `lineBuf` or the USB FIFO state.

**Root Cause:** `usb_write()` calls `usb_task()` internally, creating a re-entrant path when called from within the task loop context.

**Fix:** Remove the `usb_task()` call from inside `usb_write()`. Accept the possibility of a TX stall for oversized writes and let the main loop's `usb_task()` drain the FIFO. In `handle_reset()`, use `tud_cdc_write_flush()` directly rather than calling `usb_task()` in a loop.

**Confidence:** Medium

---

### Finding #7

**Status:** Open
**Severity:** High
**Category:** Field Failure
**Location:** `settings.c` — `settings_save()`

**Trigger Conditions:**
`settings_save()` erases and rewrites the settings flash page on every call — one full page erase cycle per settings change. STM32C0 flash endurance is ~10,000 erase cycles per page.

**Why Lab Testing Misses It:** A new device in a lab never reaches 10,000 writes.

**Failure Behavior:** An automated host (HVAC controller, server rack management) that adjusts settings on every boot (once per day) exhausts flash endurance in 27 years — acceptable. At once per hour: 416 days. At once per minute: 7 days. Silent failure mode: `settings_save()` returns `false`, commands respond with `ERR SAVE_FAILED`, and settings revert to defaults on every reboot.

Secondary risk: if `nvs.c` is ever activated without adjusting its base address, it will target the same `FLASH_STORAGE_START_ADDR` as `settings.c`, immediately corrupting the settings page.

**Root Cause:** Single-page settings storage with erase-on-every-write; no wear leveling; NVS dead code sharing the same address region.

**Fix (minimum):** Deduplicate writes — only call `settings_save()` if the stored value actually differs from the new value (already partially done; audit all setters for consistency). For a full fix: implement round-robin across the four reserved pages (60–63), storing a write counter so the most recent page can be identified on boot. This extends endurance 4×. Document the NVS address collision risk and separate the address ranges.

**Confidence:** High

---

### Finding #8

**Status:** Open
**Severity:** High
**Category:** Logic Error / State Bug
**Location:** `main.c` — `handle_fault()`, `app_state.c` — `app_state_enter_running()`

**Trigger Conditions:**
`SystemFault` exits to `SystemRunning` on the first valid sensor reading. `handle_fault()` calls `app_state_enter_running()` immediately when `t != INT16_MIN`. If the sensor is intermittent (e.g., loose 1-Wire connection), the system alternates: valid reading → `SystemRunning` → next poll: `INT16_MIN` → `SystemFault` → valid → `SystemRunning`, cycling every 2–3 seconds.

**Why Lab Testing Misses It:** In a lab, sensor recovery is permanent. Intermittent connections only occur in field installations.

**Failure Behavior:** Fans cycle on/off at 2–3 second intervals. PWM throttle engages/disengages rapidly. Program LED epileptically alternates between the error pattern and thermal-state pattern. Fans experience stress from rapid start/stop cycles.

**Root Cause:** No hysteresis on the `SystemFault → SystemRunning` recovery transition. One valid reading is sufficient to exit fault.

**Fix:**
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
Require 3 consecutive valid readings (~6–9 seconds) before exiting `SystemFault`.

**Confidence:** High

---

### Finding #9

**Status:** Open
**Severity:** Critical
**Category:** Logic Error / Recovery Failure
**Location:** `main.c` — `apply_fans()` (or equivalent fan drive logic in `app_task()`)

**Trigger Conditions:**
`SystemFault` disables automatic fan drive. `auto_on = (state == SystemRunning) && (thermal == ...)`. In `SystemFault`, `state != SystemRunning`, so `auto_on = false`. `fans_on = button_pressed || auto_on`. If no button is pressed, all fans stop.

**Why Lab Testing Misses It:** Sensor loss is never tested simultaneously with an elevated temperature. Lab tests either: (a) raise temperature with sensors healthy, or (b) disconnect sensor at ambient temperature. The combination — thermal emergency + sensor fault — is never exercised.

**Failure Behavior:** Component is at 65°C (above `temp_critical`). An ESD event or vibration disrupts the 1-Wire bus. System enters `SystemFault`. All fans stop. Component continues to heat with no active cooling. Potential hardware damage or fire hazard. This is the most dangerous defect in the audit.

**Root Cause:** `SystemFault` unconditionally kills fan auto-drive regardless of the last-known thermal state. The firmware treats sensor loss as a signal to reduce output rather than to fail safe with maximum cooling.

**Fix — Fail-Safe Fan Behavior:**
```c
static void apply_fans(SystemState state, ThermalState thermal, bool button_pressed)
{
    if (state == SystemFault)
    {
        fan_control_all_on();   // fail-safe: keep fans running on sensor loss
        return;
    }
    // ... existing logic ...
}
```
Alternatively, persist the last valid `ThermalState` when entering fault and continue driving fans based on that state until recovery.

**Confidence:** High

---

### Finding #13

**Status:** Open
**Severity:** High
**Category:** Logic Error
**Location:** `commands.c` — `usb_read_line()` / `linebuffer_append_byte()`

**Trigger Conditions:**
A command longer than `CMD_LINE_BUF_SIZE - 1 = 127` bytes arrives. `linebuffer_append_byte()` detects overflow, clears the buffer (`len = 0`), and sends `ERR OVERFLOW`. Subsequent bytes of the same overlong command then begin filling the now-empty buffer from position 0. When the newline arrives, the accumulated tail of the original overlong command is passed to `process_line()` as if it were a new command.

**Why Lab Testing Misses It:** Test commands are always short. No one sends 200-byte commands manually in a lab.

**Failure Behavior:** An automated host or a USB framing glitch sends an oversized payload. The tail fragment of the invalid command is interpreted as a new command. If the tail happens to match a valid command prefix (e.g., the last 9 bytes are `SETTINGS?`), it executes. Low probability but nonzero for automated control systems.

**Root Cause:** No mechanism to discard all bytes belonging to the overlong command until the next newline.

**Fix:** Add a discard flag to the line buffer state:
```c
typedef struct {
    char buf[CMD_LINE_BUF_SIZE];
    uint16_t len;
    bool overflow_discard;   // ADD
} CommandLineBuffer;
```
On overflow: set `overflow_discard = true` and clear the buffer. In `linebuffer_append_byte()`: if `overflow_discard` is set, discard the byte; clear the flag only when `\n` is received.

**Confidence:** High

---

### Finding #14

**Status:** Open
**Severity:** Medium
**Category:** Logic Error
**Location:** `thermal_control.c` — `compute_thresholds()`, `thermal_control_step()`

**Trigger Conditions:**
Always active. `t_cdeg / 100` truncates toward zero. With `fan_on = 35`, the system requires `t_cdeg >= 3500` (exactly 35.00°C) to enter `ThermalHigh`. At 34.99°C (`t_cdeg = 3499`), `t_deg = 34`, so `34 >= 35` is false — fans stay off. This creates an implicit 0.99°C dead band on all upward transitions that is not configurable and not documented.

**Why Lab Testing Misses It:** Lab temperature measurements are coarse. The 0.99°C dead band is invisible at the resolution a bench observer uses.

**Failure Behavior:** User observes temperature reading 35°C on a host display (which shows `t_cdeg / 100` rounded) but fans have not started. User files a field defect. The system is working as coded but not as expected. Repeated `fan_on` threshold adjustment attempts fail to resolve the perceived issue.

**Root Cause:** Thermal thresholds stored in whole degrees are compared against a truncated (not rounded) centidegree reading.

**Fix:** Compare `t_cdeg` directly against the thresholds in centidegrees:
```c
// Store thresholds in centidegrees in ThresholdSet
typedef struct {
    int16_t fan_on_cdeg;      // settings.temp_fan_on * 100
    int16_t fan_off_cdeg;
    int16_t throttle_on_cdeg;
    int16_t crit_on_cdeg;
    int16_t crit_off_cdeg;
    int16_t throttle_off_cdeg;
} ThresholdSet;
```
Eliminate the `t_deg` truncation step. All comparisons use `t_cdeg` directly against `threshold * 100`. This removes the dead band entirely.

**Confidence:** High

---

### Finding #15

**Status:** Open (revised after verification)
**Severity:** Medium
**Category:** Fragility / Architecture
**Location:** `fan_tacho.c` — `HAL_GPIO_EXTI_Falling_Callback()`

**Verification Note:** The original finding included a "torn-read" sub-claim about the `last_pulse_ms` / `prev_pulse_ms` read in `fan_tacho_get_rpm()`. That sub-claim is **incorrect**. The reader at `fan_tacho.c:103-110` wraps both reads in `__disable_irq()` / `__enable_irq()`, so the ISR cannot fire between the two reads on Cortex-M0+. The torn-read scenario described is impossible. Only the port-match fragility remains.

**Trigger Conditions:** The EXTI callback matches by `GPIO_Pin` number only, not by port. If two fans were ever assigned GPIO pins with the same number on different ports (e.g., PA7 and PC7), the callback would misattribute the pulse. The current board configuration has no conflicts (`PC7`, `PB14`, `PA5`, `PC15` — all distinct pin numbers, verified in `board_config.h:142-149`), but the code structure would silently break on a board revision that reassigns pins.

**Why Lab Testing Misses It:** Current pins are non-conflicting. A future board respin with overlapping pin numbers would break silently — no compile-time or runtime indicator.

**Failure Behavior (if pins ever collide):** A tach pulse from one fan would be attributed to a different fan, producing incorrect RPM readings and incorrect presence detection.

**Fix:** Change the EXTI callback to match on both `GPIO_Pin` and source port:
```c
if (tacho[i].gpio.pin == GPIO_Pin && tacho[i].gpio.port == source_port)
```
This requires the HAL callback to receive or infer the source port. Since HAL's `HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)` only passes the pin, the port must be inferred from EXTI pending bits, or each pin's port must be looked up from a table at callback time.

**Confidence:** High (fragility is real); current board is safe.

---

### Finding #16

**Status:** Open (Low Priority)
**Severity:** Medium
**Category:** Code Quality / False Security
**Location:** `onewire.c` — `ow_sem_lock()` / `ow_sem_unlock()`

**Trigger Conditions:**
`ow_sem_lock()` calls `critical_enter()` + increments `lock_count` + calls `critical_exit()`. IRQs are disabled only for the duration of the `lock_count++` increment, then immediately re-enabled. The `lock_count` counter provides no mutual exclusion — it is a dead abstraction. The actual 1-Wire bit-bang timing is correctly protected by inner `ENTER_CRITICAL()` / `EXIT_CRITICAL()` calls in `ow_write_bit()` and `ow_read_bit()`. The outer semaphore does nothing useful.

Additionally, `ds18b20_begin()` calls `ow_init()` on every `StatePoll`, reconfiguring the GPIO pin unnecessarily on each 1-second cycle.

**Why Lab Testing Misses It:** The inner critical sections correctly protect timing. The outer semaphore failure is invisible because no concurrent access is possible in a cooperative scheduler without the semaphore.

**Failure Behavior:** No runtime defect. The risk is that a future developer, trusting the `ow_sem_lock()` abstraction, assumes mutual exclusion is provided and removes the inner `ENTER_CRITICAL()` calls, leaving the 1-Wire timing unprotected.

**Fix:** Remove `ow_sem_lock()` / `ow_sem_unlock()` entirely, or replace with a meaningful assertion that verifies no 1-Wire operation is in progress. Document that the inner `ENTER_CRITICAL()` calls are the authoritative timing guards. Move `ow_init()` out of `ds18b20_begin()` — call it once during initialization only.

**Confidence:** High (code smell; no current runtime impact)

---

### Finding #17

**Status:** Open (Low Priority — NVS currently unused)
**Severity:** Medium
**Category:** Logic Error
**Location:** `nvs.c` — `nvs_delete()`, `nvs_read()`

**Trigger Conditions:**
`nvs_delete(key)` calls `nvs_write(key, &dummy, 0)`, writing a tombstone entry with `data_len = 0`. On subsequent `nvs_read()` for the same key, the tombstone is found: `match_dl = 0`. The function copies 0 bytes and returns `NVS_OK` with `*out_len = 0`.

A caller checking only `rc == NVS_OK` will believe the key exists and proceed with 0 bytes of data — which is indistinguishable from a zero-length write rather than a deletion.

**Why Lab Testing Misses It:** NVS is currently dead code (never called from `main.c`). This defect is latent.

**Failure Behavior:** If NVS is activated: any caller that deletes a key and then calls `nvs_read()` expecting `NVS_ERR_NOT_FOUND` will instead get `NVS_OK` with empty data. Code that uses `if (nvs_read(...) == NVS_OK)` as a key-existence check will silently process deleted entries as present.

**Fix:** Define a special tombstone marker distinct from a zero-length value. In `nvs_read()`, detect the tombstone and return `NVS_ERR_NOT_FOUND`. Alternatively document explicitly that callers must check `*out_len > 0` to confirm a non-deleted entry and update all call sites accordingly.

**Confidence:** High (logic is clear; impact zero until NVS is activated)

---

### Finding #19

**Status:** Open (Low Priority)
**Severity:** Medium
**Category:** Code Quality
**Location:** `pwm_repeater.c` — `pwm_repeater_init()`, `main.c` — post-init overrides

**Trigger Conditions:**
`pwm_repeater_init()` calls `pwm_set_throttle_a(50)` and `pwm_set_throttle_b(50)` internally. Immediately after, `main.c` calls `pwm_set_throttle_a(100U)` and `pwm_set_throttle_b(100U)`, overriding the init values. The 50% default set inside `pwm_repeater_init()` is dead — it is never the actual operational value.

**Why Lab Testing Misses It:** The final throttle is 100% from day one. No test exercises the `pwm_repeater_init()` default in isolation.

**Failure Behavior:** If someone removes the `main.c` override lines, assuming "the init already sets a sane default," the operational throttle becomes 50% permanently with no explanation. Silent functionality regression.

**Fix:** Remove the `pwm_set_throttle_a/b()` calls from inside `pwm_repeater_init()`. The init function should set the hardware to a known-off state only. Throttle values are application policy and belong in the caller (`main.c`), where they already are. Document that throttle must be set by the caller after `pwm_repeater_init()`.

**Confidence:** High

---

### Finding #20

**Status:** Open (Low Priority, revised after verification)
**Severity:** Low (downgraded from Medium)
**Category:** Field Failure / Margin
**Location:** `watchdog.c` — `watchdog_init()`

**Verification Note:** The original finding cited LSI tolerance of "±15–30%". Per the STM32C0 datasheet (DS13560), the LSI is specified at 32 kHz with ~±10% tolerance over the full V/T range (typical roughly ±5%). The ±15-30% figure is exaggerated. Numbers below are recomputed with the datasheet figure.

**Trigger Conditions:**
`WATCHDOG_PRESCALER = IWDG_PRESCALER_32`, `WATCHDOG_RELOAD = 1999`. Nominal IWDG timeout = `2000 / (32000 / 32) = 2.0 seconds`. At LSI = 35.2 kHz (+10%): timeout ≈ `2000 / (35200/32) = 1.82 seconds`. At LSI = 28.8 kHz (−10%): timeout ≈ `2000 / (28800/32) = 2.22 seconds`.

**Why Lab Testing Misses It:** Lab devices have typical LSI. The tolerance tail only manifests across a production population.

**Failure Behavior:** On devices at the fast end of tolerance: the ~1.82-second IWDG budget remains comfortable for the current cooperative loop. Worst-case loop iteration is on the order of ~1.1 seconds (DS18B20 conversion dominant). Margin is ~700 ms today, which is adequate but shrinks with each feature added.

**Fix (optional):** Widen the IWDG timeout to ~3 seconds by setting `WATCHDOG_RELOAD = 2999` to maintain a 2× margin guarantee. Also document the worst-case loop iteration time so future additions can be checked against the budget.

**Confidence:** Low (no current defect; small theoretical headroom concern).

---

## Summary Table

| # | Severity | Category | Location | Status |
|---|----------|----------|----------|--------|
| 3 | High | Logic / State | `settings_set_temp_throttle_on()` missing fan_on check | Open |
| 4 | High | Logic | `parse_pwm_throttle_temp()` missing range validation | Open |
| 5 | Medium | Architecture | Dual tick sources (`millis` vs `HAL_GetTick`) | Open (revised) |
| 6 | High | Race | `usb_write()` re-entrant via `usb_task()` | Open |
| 7 | High | Field Failure | Flash single-page wear, no wear-leveling | Open |
| 8 | High | Logic / State | No hysteresis on `SystemFault → SystemRunning` | Open |
| 9 | Critical | Safety | `SystemFault` kills fans — fail-unsafe | Open |
| 13 | High | Logic | Command buffer overflow tail-accumulation | Open |
| 14 | Medium | Logic | Implicit 0.99°C dead band in thermal comparisons | Open |
| 15 | Medium | Fragility | Fan tacho EXTI port-match (torn-read claim retracted) | Open (revised) |
| 16 | Medium | Code Quality | `ow_sem_lock()` provides no real mutual exclusion | Open |
| 17 | Medium | Logic | NVS delete returns OK not NOT_FOUND | Open |
| 19 | Medium | Code Quality | Dead throttle init inside `pwm_repeater_init()` | Open |
| 20 | Low | Margin | IWDG headroom (LSI numbers corrected, margin OK) | Open |

**Closed:**
- Finding #1 — torn read in `pwm_repeater.c` (fixed).
- Finding #2 — fixed-point conversion + tightened raw bounds in `temperature_sensor.c`; residual all-`0xFF`-scratchpad-with-valid-CRC scenario is bounded by Dallas CRC8 collision probability on `ds18b20_is_connected()`, which is the standard mitigation for this sensor. No further work.

**False positives:**
- Finding #10 — claimed soft-float in DS18B20 path; the conversion is already fixed-point at `temperature_sensor.c:97`. `ds18b20_raw_to_celsius()` exists but has no callers.
- Finding #11 — ARR/CCR1 preload makes write order safe.
- Finding #12 — claimed EWMA stale-pulse / stale-period mismatch during frequency transitions; verification shows `out->period_ticks` and `out->pulse_ticks` are updated atomically together only when `new_data_ready` is true, so they remain a consistent pair.
- Finding #18 — telemetry buffer adequately sized.

---

## Recommended Implementation Order

1. **Finding #9** — Fail-unsafe fan behavior. Safety defect. Fix immediately.
2. **Finding #8** — Fault recovery hysteresis. Prevents fan cycling on intermittent sensors.
3. **Finding #3** — Settings invariant gap. Corrupts thermal state machine from a single command.
4. **Finding #13** — Command buffer tail-accumulation. Prevents garbage command execution.
5. **Finding #4** — Range validation in PWMTHRTEMP parser. Consistency fix.
6. **Finding #14** — Centidegree comparison dead band. Eliminates user-visible threshold confusion.
7. **Finding #7** — Flash wear mitigation. Long-term reliability.
8. **Finding #5** — Tick source consolidation. Architectural correctness.
9. **Finding #6** — USB re-entrancy. Low probability but deterministic when triggered.
