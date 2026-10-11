#ifndef CLOCK_INPUT_H
#define CLOCK_INPUT_H
/**
 * @file   clock_input.h
 * @brief  Set-up of the external clock input. Private to adapters/target;
 *         the port the app uses is clock_input_port.h.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */

/**
 * @brief  Sets Timer/Counter1 to latch an edge on its input capture pin
 *         (PB0), with no interrupt, and forgets any edge already latched.
 *         register_init() makes the pin an input first.
 * @note   Not ISR-safe. Never blocks. Call once, from platform_init().
 */
void clock_input_init(void);

#endif /* CLOCK_INPUT_H */
