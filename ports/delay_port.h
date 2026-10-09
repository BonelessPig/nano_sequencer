#ifndef DELAY_PORT_H
#define DELAY_PORT_H
/**
 * @file   delay_port.h
 * @brief  Port: blocking wait.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

/**
 * @brief  Blocks for the given time.
 * @param  ms  Time to wait in milliseconds, 0 to 65535. 0 returns
 *             immediately.
 * @note   Not ISR-safe: it is a busy-wait, so calling it from an ISR would
 *         stall everything else for the whole duration.
 */
void delay_wait_ms(uint16_t ms);

#endif /* DELAY_PORT_H */
