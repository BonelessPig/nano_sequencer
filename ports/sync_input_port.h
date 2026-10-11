#ifndef SYNC_INPUT_PORT_H
#define SYNC_INPUT_PORT_H
/**
 * @file   sync_input_port.h
 * @brief  Port: the sync switch, which chooses between the internal clock
 *         and the external one.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>

/**
 * @brief  Reads the sync switch as it is at this instant. Nothing is
 *         debounced here: the reading can flicker for a few milliseconds
 *         as the switch is moved.
 * @return true while the switch is on (follow the external clock).
 * @note   Not ISR-safe. Never blocks.
 */
bool sync_input_read(void);

#endif /* SYNC_INPUT_PORT_H */
