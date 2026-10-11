#ifndef WATCHDOG_PORT_H
#define WATCHDOG_PORT_H
/**
 * @file   watchdog_port.h
 * @brief  Port: the watchdog, which restarts the firmware if it stops
 *         running.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */

/**
 * @brief  Tells the watchdog the firmware is still running. platform_init()
 *         starts the watchdog if it succeeds; from then on, if this is not
 *         called for about a quarter of a second, the processor is reset
 *         and the firmware starts again, with the watchdog given as the
 *         cause (see reset_cause_port.h).
 * @note   Not ISR-safe by intent: call it from the main loop, so that a
 *         loop that has stopped is what the watchdog catches. Never blocks.
 */
void watchdog_kick(void);

#endif /* WATCHDOG_PORT_H */
