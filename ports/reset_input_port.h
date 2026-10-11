#ifndef RESET_INPUT_PORT_H
#define RESET_INPUT_PORT_H
/**
 * @file   reset_input_port.h
 * @brief  Port: the reset input, which sends the pattern back to its start.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>

/**
 * @brief  Reports whether an edge has arrived on the reset input since the
 *         previous call, and forgets it. An edge is the signal at the input
 *         going from low to high. Call once on every tick: a pulse far
 *         shorter than a tick is still caught.
 * @return true if at least one edge arrived since the previous call.
 * @note   Not ISR-safe. Never blocks.
 */
bool reset_input_edge(void);

#endif /* RESET_INPUT_PORT_H */
