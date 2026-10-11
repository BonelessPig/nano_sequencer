#ifndef RUN_INPUT_PORT_H
#define RUN_INPUT_PORT_H
/**
 * @file   run_input_port.h
 * @brief  Port: the run button, which stops the pattern and starts it again.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>

/**
 * @brief  Reads the run button as it is at this instant. Nothing is
 *         debounced here: the reading can flicker for a few milliseconds
 *         as the button is pressed and let go.
 * @return true while the button is held down.
 * @note   Not ISR-safe. Never blocks.
 */
bool run_input_read(void);

#endif /* RUN_INPUT_PORT_H */
