/**
 * @file   run_input.c
 * @brief  Target implementation of the run button port: a push button
 *         between PD7 (D7 on the Nano) and ground. register_init() makes the
 *         pin an input with its pull-up on, so it reads low while the button
 *         is held down.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "run_input_port.h"
#include <stdbool.h>
#include <stdint.h>
#include "atmega328p_regs.h"
#include "bits.h"

#define RUN_BIT BIT_7 // PD7: the run button, low while pressed



/**
 * @brief  Reads the run button as it is now (see run_input_port.h).
 * @return true while the pin is low
 */
bool run_input_read(void)
{
    return (0U == (PIND & RUN_BIT));
}
