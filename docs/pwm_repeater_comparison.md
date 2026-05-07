# PWM Repeater: Commit 222fd93d vs panos_dev

Comparison of the PWM repeater implementation at commit `222fd93d`
("Implemented Robust Input Capture using BOTHEDGE polarity") against the current `panos_dev` branch.

---

## Hardware Context

| Item | Detail |
|---|---|
| MCU | STM32C071, 48 MHz |
| TIM2 (32-bit) | Input capture, PSC=47 → **1 MHz tick resolution** |
| TIM16 / TIM17 (16-bit) | PWM output, PSC=47 → 1 MHz resolution |
| Input BJT (PB10/PB11) | Inverts signal: DIM_PWM HIGH → pin LOW; DIM_PWM LOW → pin HIGH |
| Output BJT (PA0/PA1) | Inverts signal: PA0 HIGH → LCD_PWM LOW; PA0 LOW → LCD_PWM HIGH |
| PWM Mode 1 | PA0 HIGH while counter < CCR1; LOW while counter ≥ CCR1 |

---

## Architecture Side-by-Side

| Dimension | Commit 222fd93d (Version A) | panos_dev (Version B) |
|---|---|---|
| **Output frequency** | Variable — tracks input. `ARR = period_ticks − 1`. At 160 Hz: ARR=6249; at 120 Hz: ARR=8332. | Fixed 160 Hz. `OUTPUT_ARR = 6249` always. Only CCR1 changes. |
| **Output scaling** | `ARR = period_ticks − 1`, `CCR1 = active_pulse` — direct 1:1 mapping at same frequency. | Proportional: `active_ccr = (active_pulse × 6250) / period_ticks`. Frequency-independent duty conversion. |
| **BJT output inversion** | Not corrected. CCR1 = active duty ticks directly. | Corrected: `target_ccr = OUTPUT_PERIOD_TICKS − active_ccr`. PA0 HIGH time = desired LCD_PWM LOW time. |
| **BJT input inversion** | Not corrected. `pulse_ticks` = PB10 HIGH phase = DIM_PWM LOW phase = display-dark time. | Corrected. `out->pulse_ticks = ch->low_level_ticks` = PB10 LOW phase = DIM_PWM HIGH phase = display-on time. |
| **Throttle modes** | Two: `ThrottleMode_Scale` (scale PB10 HIGH phase) and `ThrottleMode_Fixed` (cap at N% of period). Plus independent `cap_factor_pct` safety ceiling. | Single: `min(DIM_PWM_HIGH_ticks, period × throttle% / 100)`. No Scale mode, no `cap_factor_pct`. |
| **Throttle API** | `pwm_set_throttle_a(uint32_t val, ThrottleMode mode)` | `pwm_set_throttle_a(uint32_t limit_pct)` |
| **Default throttle** | Ch A = 50% Fixed, Ch B = 100% Scale (pass-through) | Both channels = 50% |
| **`get_duty_a/b()`** | `pulse_ticks / period` — reports darkness fraction (inverse of brightness). | `low_level_ticks / period` — reports true DIM_PWM HIGH fraction = display brightness %. |
| **`calculate_frequency()`** | `48,000,000 / period_ticks` — wrong; TIM2 runs at 1 MHz, returns ~48× actual Hz. | `1,000,000 / period_ticks` — correct for 1 MHz timebase. |
| **64-bit output math** | None. `active_pulse × throttle_val / 100` — safe at validated bounds (max ~150 M < UINT32_MAX). | Yes. `(uint64_t)active_pulse × 6250 / period_ticks` — necessary; max product ~156 M fits uint32 at validated bounds, but the cast guards against future bound changes. |
| **`last_capture_ms` type** | `uint32_t` — wraps at ~49 days with `HAL_GetTick()`. | `uint64_t` — no wrap risk with `millis()`. |
| **Hardware abstraction** | Direct `extern TIM_HandleTypeDef htim2/htim16/htim17`. `HAL_GPIO_ReadPin()`, `HAL_GetTick()`. | BSP `Tim *` and `Gpio` structs injected at `pwm_repeater_init(Tim*, Tim*, Tim*)`. `gpio_read()`, `millis()`. |
| **IRQ routing** | Direct HAL weak override `HAL_TIM_IC_CaptureCallback`. | Same callback, plus `pwm_repeater_tim2_irq_handler()` indirection for `stm32c0xx_it.c`. |
| **`pwm_get_output_duty_a/b()`** | Not present. | Added — reports throttled output duty cycle. |
| **`get_ticks()` debug export** | Present. | Removed. |

---

## Input Capture State Machine (same in both versions)

- BOTHEDGE polarity on TIM2 CH3 (PB10) and CH4 (PB11).
- At each edge, the ISR reads the GPIO pin to determine direction (rising vs falling).
- FALLING edge (PB10 goes LOW = DIM_PWM rising): delta stored into `pulse_ticks` via IIR `(new + 3×old)/4`.
- RISING edge (PB10 goes HIGH = DIM_PWM falling): delta stored into `low_level_ticks` via IIR; period assembled as `pulse_ticks + low_level_ticks`.
- Stability gate: both `period_stable_counter` and `pulse_stable_counter` must reach 3 consecutive cycles within ±100 ticks before `new_data_ready` is set.
- Edge rejection: any level delta outside `[100, 1,200,000]` ticks or assembled period outside `[1,000, 1,500,000]` ticks resets the channel.
- Timer overflow handled in `calculate_delta`: `(arr − previous) + current + 1`.

---

## Watchdog / Signal Loss

Both versions use a 50 ms no-edge timeout in `process_channel_update` (main loop).

### Version A watchdog

```
if (pin == GPIO_PIN_SET)   ← WRONG: SET = DIM_PWM LOW = display off
    drive 100% output
else
    CCR1 = 0               ← WRONG: PA0 always LOW → LCD_PWM HIGH = 100% brightness
    // init_channel_struct NOT called in the 100% DC known-period path
    // → last_capture_ms never reset → watchdog re-fires every loop iteration
```

| Version A path | Register write | Result via BJTs | Intended |
|---|---|---|---|
| 100% DC — cold | `ARR=0xFFFE, CCR1=0xFFFF` | CCR1 > ARR → PA0 always HIGH → LCD_PWM LOW = **0% brightness** | 100% ✗ |
| 100% DC — known period | `CCR1 = active_pulse` | Passes through (double BJT inversion cancels near input DC) | ~correct below cap ✓ |
| 0% DC / signal lost | `CCR1 = 0` | CCR1=0 → counter always ≥ 0 → PA0 LOW → LCD_PWM HIGH = **100% brightness** | 0% ✗ |

### Version B watchdog (introduced in commit 560e11c)

```
if (pin == GPIO_PIN_RESET)   ← correct: RESET = DIM_PWM HIGH = display on
    drive 100% output (throttled)
else
    CCR1 = OUTPUT_ARR + 1    ← correct: counter always < CCR1 → PA0 always HIGH → LCD_PWM LOW = 0%
init_channel_struct(ch)      ← called unconditionally after both branches
```

| Version B path | Register write | Result via BJTs | Intended |
|---|---|---|---|
| 100% DC — cold | `CCR1 = 0` | PA0 always LOW → LCD_PWM HIGH = **100% brightness** | 100% ✓ |
| 100% DC — known period | `CCR1 = OUTPUT_PERIOD_TICKS − active_ccr` | Correct throttled brightness | ✓ |
| 0% DC / signal lost | `CCR1 = OUTPUT_ARR + 1 = 6250` | Counter [0…6249] always < 6250 → PA0 HIGH → LCD_PWM LOW = **0% brightness** | 0% ✓ |

---

## ThrottleMode_Scale — Why It Is Broken for This Hardware

`ThrottleMode_Scale` operates on `pulse_ticks`, which is the PB10 HIGH phase = DIM_PWM LOW phase (display dark time). Scaling it down shortens the dark interval, which *increases* perceived brightness rather than reducing it. The mode is semantically inverted on this hardware and was removed in Version B.

`ThrottleMode_Fixed` (Version A default for Ch A) happened to work approximately correctly for the pass-through case because the double BJT inversion (input + output) largely cancelled. However, it produces incorrect output when DIM_PWM exceeds the throttle cap, and the `cap_factor_pct` safety ceiling was always 100% (no-op in practice).

---

## What Was Lost Going from Version A to Version B

- `ThrottleMode_Scale` and `ThrottleMode_Fixed` as distinct modes.
- `cap_factor_pct` independent safety ceiling on the output.
- `get_ticks()` debug export.
- Variable output frequency (may be relevant if the downstream LCD controller is not fixed at 160 Hz).

---

## Confirmed Bugs Remaining in panos_dev

| Bug | Location | Impact |
|---|---|---|
| `TIMEOUT_TICKS = 48000000U` comment says "1 second at 48MHz" | `pwm_repeater.c` top | Dead code; comment is stale — at 1 MHz, 1 second = 1,000,000 ticks. No functional impact. |
