#ifndef CONFIG_H
#define CONFIG_H

/* ============================================================================
 * PRODUCT CONFIGURATION
 * Adjust these values to tune the firmware for a specific product variant.
 * ============================================================================ */

/* ── Thermal control defaults ────────────────────────────────────────────── */

#define CONFIG_TEMP_MAX                     180U  /* °C — absolute maximum allowed temperature */

#define CONFIG_TEMP_CRITICAL_DEFAULT        60U   /* °C — overheat shutdown threshold */
#define CONFIG_TEMP_THROTTLE_ON_DEFAULT     40U   /* °C — PWM throttle engages above this */
#define CONFIG_TEMP_FAN_ON_DEFAULT          35U   /* °C — fans turn ON above this */
#define CONFIG_TEMP_FAN_OFF_DEFAULT         30U   /* °C — fans turn OFF below this */

/* ── PWM throttle defaults ───────────────────────────────────────────────── */

#define CONFIG_PWM_THROTTLE_A_DEFAULT       50U   /* % — throttle duty cycle, channel A */
#define CONFIG_PWM_THROTTLE_B_DEFAULT       50U   /* % — throttle duty cycle, channel B */

/* ── Telemetry ───────────────────────────────────────────────────────────── */

#define CONFIG_TELEMETRY_INTERVAL_MS        1000U     /* ms — reporting interval */


#endif /* CONFIG_H */
