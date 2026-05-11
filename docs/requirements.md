# Firmware Requirements - STM32C071 Fan Controller

## 1. PWM Management

### PWM Inputs (2 Channels)
- **Description**: Independent reading of two (2) PWM signals.
- **Frequency Range**: 120 Hz – 200 Hz (synchronized with display dimming).
- **Measurement**: Individual Duty Cycle measurement for each channel.

### PWM Outputs (2 Channels)
- **Description**: Generation of two (2) independent output signals.
- **Fixed Frequency**: 160 Hz (Predefined at compile time).
- **PWM Throttle**: Independent capping for each output channel.
  - **Logic**: Output Duty Cycle = `min(Input_DC, Throttle_Limit)`.

---

## 2. Thermal Monitoring

### Sensors
- **DS18B20**: 1x digital sensor (1-Wire interface, PB3).
- **I2C sensor**: 1x digital sensor (I2C bus, on-board).

### Processing Logic
- Continuous reading of both sensors.
- **Current Temperature**: Defined as the maximum value between the two sensors (`max(DS18B20, I2C)`).

---

## 3. Fan Control

### Hardware Support
- **2-wire Fan**: Power control (ON/OFF).
- **3/4-wire Fan**: PWM control (0% or 100% logic).

### Management
- **Global Fan Control**: Unified control logic applied to all connected fans.

---

## 4. Control Logic (Automation)

### System State Machine
The firmware implements a unified system state machine. In normal operation the state is driven by temperature (thermal states). A dedicated error state exists for fault conditions.

#### Thermal States
Four thermal states are defined, governed by four configurable temperature thresholds:

**Threshold Hierarchy**: `T_critical > T_throttle_on > T_fan_on > T_fan_off`

| State | Entry Condition | PWM Throttle | Fans | Backlight |
| :--- | :--- | :--- | :--- | :--- |
| **SystemLow** | Temperature < $T_{fan\_off}$ | 100% (no cap) | OFF | ON |
| **SystemHigh** | Temperature ≥ $T_{fan\_on}$ | 100% (no cap) | ON | ON |
| **SystemThrottling** | Temperature ≥ $T_{throttle\_on}$ | Capped to configured limit | ON | ON |
| **SystemCritical** | Temperature ≥ $T_{critical}$ | 0% (fully capped) | ON | **OFF** |

#### Hysteresis
To prevent rapid state switching (flickering), all upward transitions are offset from downward transitions:

| Transition | Exit Condition |
| :--- | :--- |
| **SystemCritical → SystemThrottling** | Temperature < $T_{critical}$ − 2°C |
| **SystemThrottling → SystemHigh** | Temperature < $T_{throttle\_on}$ − 2°C |
| **SystemHigh → SystemLow** | Temperature < $T_{fan\_off}$ |

#### Sensor Lost State (SystemSensorLost)
If the temperature sensor returns an invalid reading, the system enters **SystemSensorLost**:
- PWM throttle: 100% (no cap).
- Fans: remain in their current state.
- LED: ERROR pattern.
- Recovery: on the next valid reading, the state machine re-enters normally from the current temperature.

#### Error State (SystemError)
A latching fault state for non-thermal system errors:
- PWM throttle: 0% (fully capped).
- Backlight: OFF.
- LED: ERROR pattern.
- The state machine does not exit **SystemError** autonomously; it requires an explicit external action.

### Temperature Control Diagram

```
Temperature
    ↑
    │  ┌────────────────────────────────────────────────────┐
    │  │ T_critical  (exit: T_critical − 2°C)               │
    │  │ • SystemCritical                                   │
    │  │ • PWM: 0%  |  Fans: ON  |  Backlight: OFF          │
    │  └────────────────────────────────────────────────────┘
    │
    │  ┌────────────────────────────────────────────────────┐
    │  │ T_throttle_on  (exit: T_throttle_on − 2°C)         │
    │  │ • SystemThrottling                                 │
    │  │ • PWM: capped  |  Fans: ON  |  Backlight: ON       │
    │  └────────────────────────────────────────────────────┘
    │
    │  ┌────────────────────────────────────────────────────┐
    │  │ T_fan_on  (exit: T_fan_off)                        │
    │  │ • SystemHigh                                       │
    │  │ • PWM: 100%  |  Fans: ON  |  Backlight: ON         │
    │  └────────────────────────────────────────────────────┘
    │
    │  ┌────────────────────────────────────────────────────┐
    │  │ Below T_fan_off                                    │
    │  │ • SystemLow                                        │
    │  │ • PWM: 100%  |  Fans: OFF  |  Backlight: ON        │
    │  └────────────────────────────────────────────────────┘
    │
    └──────────────────────────────────────────────────────→ Time
```

---

## 5. Status Indicators (Program LED)

| LED Pattern | System State |
| :--- | :--- |
| **ON 100ms / OFF 2000ms** | Fans are **OFF** |
| **ON 100ms / OFF 1000ms** | Fans are **ON** |
| **ON 200ms / OFF 200ms** | **ERROR** (Overheat/Sensor Lost) |

---

## 6. Configuration (DIP Switches)

### Fan Type Selection
| Switch State | Configuration |
| :--- | :--- |
| `00` | 2-wire FAN |
| `01` | 3-wire FAN |
| `10` | 4-wire FAN |

### Implementation Note
For **4-wire fans** (with Tacho), Tacho reading is not mandatory for basic operation. We can potentially simplify the configuration by using 4 switches to select between **Power PWM** or **Remote PWM** for 4 independent fan channels, either through firmware detection or manual selection.

---

## 7. Settings Storage

All user-configurable parameters are persisted to internal flash and restored on power-up. This includes:

- PWM throttle limits (per channel)
- Temperature thresholds ($T_{fan\_off}$, $T_{fan\_on}$, $T_{throttle\_on}$, $T_{critical}$)
- Fan type selection (override of DIP switch, if applicable)

**Defaults**: If no valid settings exist in flash (e.g., first boot or corrupted storage), the firmware boots with compile-time defaults and writes them to flash.

---

## 8. USB Interface

A USB CDC (virtual serial port) interface provides two services using a simple human-readable ASCII protocol.

### 8.1 Settings (Command/Response)

The device accepts ASCII commands over USB to read and write all user-configurable settings (PWM throttle limits, temperature thresholds, fan type). Changes can be saved to flash or reset to compile-time defaults via command.

### 8.2 Telemetry (Periodic Output)

The device periodically emits ASCII telemetry (default interval: 1 s) reporting current system temperature, fan state, input and output duty cycles for both channels, and the current system state. Telemetry output can be enabled or disabled via command.

---

## 9. Push Button (Test / Override)

A momentary push button provides a manual fan test function:

- **Held pressed**: All fans are forced **ON**, regardless of temperature or automation logic.
- **Released**: Fans return to the state determined by normal automation logic (as if the button was never pressed). No settings are modified.

**Debounce**: Software debounce of ≥ 20 ms is required to avoid spurious triggering.  
**LED**: While the button is held, the status LED follows the "Fans ON" pattern (`ON 100ms / OFF 1000ms`).