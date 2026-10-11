#ifndef CLOCK_INPUT_PORT_H
#define CLOCK_INPUT_PORT_H
/**
 * @file   clock_input_port.h
 * @brief  Port: the external clock input, which the pattern can be stepped
 *         from in place of the internal clock.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>

/**
 * @brief  Reports whether an edge has arrived on the external clock since
 *         the previous call, and forgets it. An edge is the signal at the
 *         input going from low to high. Call once on every tick: edges that
 *         arrive closer together than the calls are reported as one, and a
 *         pulse far shorter than a tick is still caught.
 * @return true if at least one edge arrived since the previous call.
 * @note   Not ISR-safe. Never blocks.
 */
bool clock_input_edge(void);

#endif /* CLOCK_INPUT_PORT_H */
