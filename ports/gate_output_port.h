#ifndef GATE_OUTPUT_PORT_H
#define GATE_OUTPUT_PORT_H
/**
 * @file   gate_output_port.h
 * @brief  Port: the gate output, which is high for as long as a note is held.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>

/**
 * @brief  Sets the level of the gate output. It stays there until the next
 *         call. Writing the level it already has changes nothing, so it can
 *         be called on every tick. platform_init() leaves the output low.
 * @param  b_high  true for high (a note is held), false for low.
 * @note   Not ISR-safe. Never blocks.
 */
void gate_output_write(bool b_high);

#endif /* GATE_OUTPUT_PORT_H */
