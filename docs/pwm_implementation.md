# Project Specification: PWM Repeater and Governor

## 1. Project Overview
The system acts as a smart, two-channel PWM repeater and governor. It receives two independent PWM input signals (which are expected to share the same frequency but may not be in phase) and redirects them to two independent PWM outputs. The primary goal is to accurately reproduce the exact frequency and Duty Cycle (DC) of each respective input.

Additionally, each input/output pair features an independent, software-defined "throttle" limit that acts as a hard override on the maximum allowed DC:
* If the measured input DC is greater than the throttle limit, the output DC is clamped to the throttle value.
* If the measured input DC is less than or equal to the throttle limit, the output DC directly mirrors the measured input DC.

## 2. Hardware Specification
* **Microcontroller:** STM32C071 (Cortex-M0+)
* **Core Clock:** 48 MHz
* **Primary Peripheral:** TIM2 (32-bit Timer)

### Pin Mapping
| Pair | Input (IC) | Output (PWM) |
| :--- | :--- | :--- |
| **Channel A** | `PB10` (TIM2_CH3) | `PA0` (TIM2_CH1) |
| **Channel B** | `PB11` (TIM2_CH4) | `PA1` (TIM2_CH2) |

## 3. Operational Constraints & Real-World Scenarios
* **Phase Independence:** The inputs and outputs operate independently; the system must not force phase alignment between Channel A and Channel B.
* **Initialization Blindness:** The system must handle startup states where it is unknown which input channel (or if any) is plugged in first.
* **Hot-Swap & Noise Immunity:** Plugging and unplugging cables introduces electrical noise, spikes, and phantom edges. The system must actively filter and reject these anomalies.
* **Precision Frequency Lock:** The system must identify and match the exact input frequency exclusively within the expected valid range of **40 Hz to 500 Hz**.
* **Failsafe / Timeout:** The system must actively detect when a cable is lost or disconnected. Upon timeout, it must immediately stop the output (force 0% duty cycle).

## 4. Architectural Approaches Under Consideration

### Approach 1: Soft-Defined PWM (EXTI & Timer Assisted)
* **Input:** Relies on EXTI edge-level detection.
* **Logic (Unthrottled):** Output pin state is toggled immediately within the EXTI ISR to directly mirror the incoming signal's phase and DC.
* **Logic (Throttled):** A background timer measures High/Low levels and calculates the DC. On an incoming rising edge, the output is set HIGH. A secondary timer (or compare match) is scheduled to "kill" the high level at the throttled duration, ensuring it drops LOW before the next EXTI interrupt.

### Approach 2: Hardware PWM with Free-Running Timer (Sliding CCR)
* **Timebase:** The hardware timer is set to a continuous Free-Running mode (ARR maxed out at `0xFFFFFFFF`).
* **Logic:** The software calculates the exact duration of the period and duty cycle. Instead of relying on a timer reset, the software continuously calculates the absolute timestamp in the counter's future. It "slides" the Output Compare (CCR) value forward to schedule the exact tick when the pin should toggle HIGH or LOW, preserving independent phase and DC.

### Approach 3: Hardware PWM with Dynamic ARR (Target Range)
* **Timebase:** Uses standard hardware PWM generation where the timer's Auto-Reload Register (ARR) defines the absolute cycle boundary.
* **Logic:** The software determines the input frequency on the fly. If within the valid range (40 - 500 Hz), it dynamically sets the shared hardware `ARR` to match. It then calculates the target DC and scales the output `CCR` to match the newly established `ARR` (applying the throttle logic as a hard cap).

## 5. Firmware Design Guidelines (Critical Rules)
Based on prior testing, the following rules must be strictly adhered to during implementation:

* **No Mid-Cycle Resets:** Do not use the Update Generation bit (`TIM_EGR_UG`) inside the ISR to force register updates. This instantly truncates the current pulse and causes severe output phase jitter. Rely entirely on Auto-Reload Preload (`ARPE`) and Output Compare Preload to safely swap values at the cycle boundary.
* **Scaling Math:** When calculating the output `CCR`, use 64-bit integer math to prevent overflow. Scale the result against the *hardware's current ARR*, not just the measured input period, to prevent flickering during transient frequency shifts.
* **Level-Based Validation:** A valid period must only be accepted if it is the result of consecutive, mathematically valid High and Low levels. Single edges or "ghost" levels caused by noise must immediately reset the validation state machine.