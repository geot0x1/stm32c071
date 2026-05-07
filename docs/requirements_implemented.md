# Requirements Implementation Status

> Last updated: 2026-05-07  
> Based on post-review audit of the full codebase.

---

## Legend

| Symbol | Meaning |
| :--- | :--- |
| ✅ | Fully implemented and verified in code |
| ⚠️ | Partially implemented — gap noted |
| 🔧 | Fixed in the current development session |
| ℹ️ | Requirement clarified / no code change needed |

---

## §1 PWM Management

| Requirement | Status | Evidence |
| :--- | :---: | :--- |
| 2 independent PWM input channels | ✅ | `pwm_repeater.c` — `pwmChannelA/B` via TIM2 CH3/CH4 |
| Frequency range 120–200 Hz | ℹ️ | Hardware window is 40–500 Hz; 120–200 Hz is the intended operational range, not a hard filter. Accepted by design per `CLAUDE.md`. |
| Individual duty cycle measurement per channel | ✅ | `pwm_get_duty_a()`, `pwm_get_duty_b()` |
| Fixed output frequency 160 Hz | ✅ | `OUTPUT_FREQ_HZ 160U` → TIM16 CH1 (PA0), TIM17 CH1 (PA1) |
| Throttle: output DC = min(Input DC, Limit), independent per channel | ✅ | `calculate_output_pulse()` in `pwm_repeater.c` |

---

## §2 Thermal Monitoring

| Requirement | Status | Evidence |
| :--- | :---: | :--- |
| DS18B20 1-Wire sensor (PB4) | ✅ | `temperature_sensor.c:18` — `GPIO_PIN_4`. *Note: requirement doc says PB3; PB4 is the correct hardware pin.* |
| HDC2010 I2C sensor (on-board) | ✅ | `hdc2010` module — init via `board_get_i2c()`, `HDC2010_ADDR_LOW` |
| Continuous reading of both sensors | ✅ | `temperature_sensor_task()` and `hdc2010_task()` called every main loop iteration |
| System temperature = max(DS18B20, HDC2010) | 🔧 | `system_temp_get()` in new `Application/system_temp/` module. Used by the thermal state machine (`app_task()` → `thermal_step()`). Telemetry still pending — see §8.2. |

---

## §3 Fan Control

| Requirement | Status | Evidence |
| :--- | :---: | :--- |
| 2-wire fan — power ON/OFF | ✅ | `fan_control.c` — `FanType2Wire` branch in `fan_control_set_unit_duty()` |
| 3/4-wire fan — PWM at 0% or 100% | ✅ | `fan_control.c` — `FanType34Wire` branch; `on_off = (duty > 0) ? 100 : 0` |
| Global / unified fan control (all fans together) | ✅ | `apply_fans()` in `main.c` iterates all `APP_FAN_COUNT` fans with one duty value |

---

## §4 Control Logic

| Requirement | Status | Evidence |
| :--- | :---: | :--- |
| T_critical: backlight output → 0% | ✅ | `apply_throttle()` sets both PWM channels to 0% at `ThermalCritical`. Zero output pulse confirmed sufficient. |
| T_critical: fans remain ON | ✅ | `app_task()` — `fans_auto_on` is true for both `ThermalHigh` and `ThermalCritical` |
| T_critical: ±2°C hysteresis | ✅ | `APP_CRITICAL_HYSTERESIS_CDEG 200` (main.c:26); applied in `thermal_step()` |
| T_high: fans turn ON, PWM throttling begins | ✅ | `thermal_step()` transitions to `ThermalHigh`; `apply_throttle()` applies `settings.pwm_throttle_a/b` |
| T_low: fans turn OFF, full PWM restored | ✅ | `thermal_step()` transitions to `ThermalLow`; `apply_throttle()` restores 100% throttle |
| Fan hysteresis — fans ON at T_high, OFF at T_low | ✅ | `thermal_step()` uses separate `fan_on` / `fan_off` thresholds; no oscillation possible |

---

## §5 Status Indicators (Program LED)

| LED Pattern | System State | Status | Evidence |
| :--- | :--- | :---: | :--- |
| ON 100ms / OFF 2000ms | Fans OFF | ✅ | `program_led.c` index 0 (`ProgramLedFansOff`) |
| ON 100ms / OFF 1000ms | Fans ON | ✅ | `program_led.c` index 1 (`ProgramLedFansOn`) |
| ON 200ms / OFF 200ms | ERROR (Overheat / Sensor Lost) | ✅ | `program_led.c` index 2 (`ProgramLedError`); triggered from `update_led()` for `ThermalSensorLost` and `ThermalCritical` |

---

## §6 Configuration (DIP Switches)

| Requirement | Status | Evidence |
| :--- | :---: | :--- |
| Fan type selection via DIP switch | ℹ️ | Hardware uses **1 bit per fan** (not 2-bit as spec states). LOW = 3/4-wire, HIGH = 2-wire. Only 2-wire fans are deployed in production. Confirmed correct by hardware team. |
| Software fan type override (USB command `fantype`) | ⚠️ | `Settings.fan_type_override[]` is persisted and writable via USB, but `fan_control.c` always reads the DIP GPIO pin — the override is stored but not applied at runtime. Not a priority given 2-wire-only deployment. |

---

## §7 Settings Storage

| Requirement | Status | Evidence |
| :--- | :---: | :--- |
| PWM throttle limits persisted | ✅ | `Settings.pwm_throttle_a/b` — saved by `settings_save()`, restored by `settings_init()` |
| Temperature thresholds persisted | ✅ | `Settings.temp_fan_on/off/critical` — same CRC-protected flash record |
| Fan type selection persisted | ⚠️ | `Settings.fan_type_override[]` is persisted; not consumed at runtime (see §6) |
| Defaults on first boot / corrupted flash | ✅ | `settings_init()` detects magic/CRC failure, applies compile-time defaults, writes to flash |

---

## §8 USB Interface

### §8.1 Settings (Command / Response)

| Requirement | Status | Evidence |
| :--- | :---: | :--- |
| Write settings via USB commands | ✅ | `commands.c` — `SETTINGSCHANGE pwma/pwmb/fantype/tempon/tempoff/tempcrit/default` |
| Read settings via USB command | ⚠️ | `GETSETTINGS` command not yet implemented. In progress. |

### §8.2 Telemetry (Periodic Output)

| Requirement | Status | Evidence |
| :--- | :---: | :--- |
| Periodic output, default 1 s | ✅ | `telemetry_task()` — `TELEMETRY_DEFAULT_INTERVAL_MS` |
| Reports **system temperature** (max of both sensors) | 🔧 | `telemetry.c` includes `system_temp.h` and calls `system_temp_get()` at lines 35 and 85. |
| Reports fan state, input/output duty cycles, thermal state | ✅ | `telemetry_create()` — format `$01,<boot_s>,<temp>,<fans>,<in_a>,<out_a>,<in_b>,<out_b>,<state>` |
| Telemetry enable / disable via command | ✅ | `TELEMETRY ON` / `TELEMETRY OFF` / `TELEMETRY INTERVAL <ms>` in `commands.c` |

---

## §9 Push Button

| Requirement | Status | Evidence |
| :--- | :---: | :--- |
| Held: force all fans ON | ✅ | `push_button_is_pressed()` → `app.button_override` → `fans_required_on` in `app_task()` |
| Released: return to normal automation | ✅ | No sticky state; `fans_required_on` is recalculated every loop iteration |
| Software debounce ≥ 20 ms | ✅ | `DEBOUNCE_MS 20U` in `push_button.c` |
| LED follows "Fans ON" pattern while held | ✅ | `update_led()` — `fans_required_on` is true while button held, selects `ProgramLedFansOn` |

---

## Open Items

| # | Item | Priority |
| :--- | :--- | :--- |
| 1 | Add `GETSETTINGS` USB command to `commands.c` | High |
| 3 | Correct pin number in `requirements.md` §2 (PB3 → PB4) | FALSE CHECK SCHEMATIC |
| 4 | Fan type software override not applied at runtime | Low — 2-wire only deployment |
