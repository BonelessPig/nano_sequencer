#ifndef RESET_INPUT_H
#define RESET_INPUT_H
/**
 * @file   reset_input.h
 * @brief  Set-up of the reset input. Private to adapters/target; the port
 *         the app uses is reset_input_port.h.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */

/**
 * @brief  Sets external interrupt 0 (PD2) to latch an edge, with the
 *         interrupt itself off, and forgets any edge already latched.
 *         register_init() makes the pin an input first.
 * @note   Not ISR-safe. Never blocks. Call once, from platform_init().
 */
void reset_input_init(void);

#endif /* RESET_INPUT_H */
