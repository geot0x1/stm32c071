#ifndef WATCHDOG_H
#define WATCHDOG_H

/**
 * @brief Initialize the IWDG with a ~2 s timeout.
 *
 * LSI @ 32 kHz, prescaler /32 → 1 kHz tick.
 * Reload 1999 → timeout = 1999 ms ≈ 2 s.
 * Window = 4095 (windowing disabled).
 *
 * Call once at startup before entering the main loop.
 */
void watchdog_init(void);

/**
 * @brief Refresh the watchdog. Call once per main-loop iteration.
 */
void watchdog_kick(void);

#endif /* WATCHDOG_H */
