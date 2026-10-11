#ifndef LEVEL_OUTPUT_H
#define LEVEL_OUTPUT_H
/**
 * @file   level_output.h
 * @brief  Set-up of the level light. Private to adapters/target; the port
 *         the app uses is level_output_port.h.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */

/**
 * @brief  Starts Timer/Counter0 as an 8-bit pulse width modulator, with its
 *         output not yet on the pin, so the light stays dark.
 *         register_init() makes the pin (PD6) a low output first.
 * @note   Not ISR-safe. Never blocks. Call once, from platform_init().
 */
void level_output_init(void);

#endif /* LEVEL_OUTPUT_H */
