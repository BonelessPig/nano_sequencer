/**
 * @file   level_output.c
 * @brief  Target implementation of the level output port: a light on PD6
 *         (D6 on the Nano), which is Timer/Counter0's compare output A, with
 *         its brightness set by pulse width.
 * @author BonelessPig
 *
 * The timer counts from 0 to 255 over and over, 977 times a second, and the
 * pin is high from 0 until the count matches the compare register. That is
 * never a flat low: a compare value of 0 still gives one count high in every
 * 256. So for a level of 0 the timer is taken off the pin, which goes back
 * to the port's own level; register_init() left that low and nothing else
 * writes it.
 *
 * The compare register is taken up by the timer only as the count goes back
 * to 0, so a change of level never cuts a pulse short.
 *
 * No interrupt is used and nothing else uses the timer.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "level_output_port.h"
#include <stdint.h>
#include "atmega328p_timer0_regs.h"
#include "level_output.h"

// Fast PWM, 8 bits, with the pin left to the port
#define PWM_OFF_PIN ((uint8_t)((1U << WGM01) | (1U << WGM00)))

// The same with the pin driven: high at 0, low at the compare match
#define PWM_ON_PIN  ((uint8_t)(PWM_OFF_PIN | (1U << COM0A1)))

// System clock / 64: 16 MHz / 64 / 256 is 977 periods a second
#define PWM_CLOCK   ((uint8_t)((1U << CS01) | (1U << CS00)))



void level_output_init(void)
{
    OCR0A  = 0U;
    TCCR0A = PWM_OFF_PIN;
    TCCR0B = PWM_CLOCK;
}



/**
 * @brief Sets the light's brightness (see level_output_port.h).
 * @param level 0 for dark, 255 for full brightness
 */
void level_output_write(uint8_t level)
{
    if (0U == level)
    {
        TCCR0A = PWM_OFF_PIN;
    }
    else
    {
        OCR0A  = level;
        TCCR0A = PWM_ON_PIN;
    }
}
