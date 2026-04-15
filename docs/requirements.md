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
The fan operation is based on temperature hysteresis to prevent rapid switching (flickering):

| Threshold | Action |
| :--- | :--- |
| **Upper Threshold ($T_{high}$)** | All fans turn **ON** when temperature exceeds this limit. |
| **Lower Threshold ($T_{low}$)** | All fans turn **OFF** when temperature drops below this limit. |

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