#ifndef CLOCK_OUTPUT_PORT_H
#define CLOCK_OUTPUT_PORT_H
/**
 * @file   clock_output_port.h
 * @brief  Port: the clock output, one pulse per step for other equipment to
 *         follow.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>

/**
 * @brief  Sets the level of the clock output. It stays there until the next
 *         call. Writing the level it already has changes nothing, so it can
 *         be called on every tick. platform_init() leaves the output low.
 * @param  b_high  true for high, false for low. A step begins on the rising
 *                 edge.
 * @note   Not ISR-safe. Never blocks.
 */
void clock_output_write(bool b_high);

#endif /* CLOCK_OUTPUT_PORT_H */
