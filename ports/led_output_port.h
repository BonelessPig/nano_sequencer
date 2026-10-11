#ifndef LED_OUTPUT_PORT_H
#define LED_OUTPUT_PORT_H
/**
 * @file   led_output_port.h
 * @brief  Port: the sixteen step lights on the front panel.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

/**
 * @brief  Says which of the step lights are to be lit. They stay that way
 *         until the next call. Giving what they already show changes
 *         nothing, so it can be called on every tick.
 *
 *         The lights need not change at once: they show the latest value
 *         given by the time the step inputs are next read (see
 *         step_input_port.h), so a change is seen within one input scan.
 *         Until the first such read they show nothing in particular.
 * @param  leds  One bit a light, bit 0 for step 0 up to bit 15 for step 15;
 *               1 is lit.
 * @note   Not ISR-safe. Never blocks.
 */
void led_output_write(uint16_t leds);

#endif /* LED_OUTPUT_PORT_H */
