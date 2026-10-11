#ifndef WATCHDOG_H
#define WATCHDOG_H
/**
 * @file   watchdog.h
 * @brief  Target-private interface of the watchdog adapter: its start-up.
 *         The port function it implements is in watchdog_port.h.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */

/**
 * @brief  Starts the watchdog with a timeout of about 250 ms: if
 *         watchdog_kick() is not called for that long, the MCU is reset.
 *         There is no stopping it afterwards.
 * @note   Call with interrupts off, which they are until timebase_init():
 *         the two register writes that set it must not be parted by more
 *         than four clock cycles.
 */
void watchdog_init(void);

#endif /* WATCHDOG_H */
