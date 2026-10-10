#ifndef TIMEBASE_H
#define TIMEBASE_H
/**
 * @file   timebase.h
 * @brief  Bring-up of the target's timebase. The function the application
 *         reads time with is declared in timebase_port.h.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */

/**
 * @brief  Starts the 1 kHz tick: sets Timer/Counter2 to interrupt once a
 *         millisecond, then enables interrupts globally. Call once, last in
 *         the platform bring-up, because interrupts are on when it returns.
 * @note   Not ISR-safe. Call from the main context only.
 */
void timebase_init(void);

#endif /* TIMEBASE_H */
