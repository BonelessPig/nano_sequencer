#ifndef BREADCRUMB_PORT_H
#define BREADCRUMB_PORT_H
/**
 * @file   breadcrumb_port.h
 * @brief  Port: a note of where the firmware has got to, kept somewhere a
 *         reset does not clear, so that after the watchdog has restarted
 *         the firmware it can be seen where it stopped.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

/**
 * @brief  Notes the place the firmware has reached, over the one before.
 * @param  place  Any value; the caller gives its places their numbers.
 * @note   Not ISR-safe by intent: the main loop marks its own progress.
 *         Never blocks; on the target it is one byte stored in RAM.
 */
void breadcrumb_write(uint8_t place);

/**
 * @brief  Gives the place last noted, which may have been before the
 *         processor was last reset.
 * @return The value last given to breadcrumb_write(). It is kept through a
 *         reset by the watchdog or the reset line, but not with the power
 *         off: after the power has come on, or the supply has dipped, and
 *         before the first write, it can be anything. Look at
 *         reset_cause_read() to know whether it means something.
 * @note   Not ISR-safe by intent. Never blocks.
 */
uint8_t breadcrumb_read(void);

#endif /* BREADCRUMB_PORT_H */
