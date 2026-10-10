/**
 * @file   clock_output.c
 * @brief  Target implementation of the clock output port: an ordinary output
 *         pin, PD5 (D5 on the Nano). register_init() makes it an output and
 *         leaves it low.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "clock_output_port.h"
#include <stdbool.h>
#include <stdint.h>
#include "atmega328p_regs.h"
#include "bits.h"

#define CLOCK_OUT_BIT BIT_5 // PD5: the clock output



/**
 * @brief Drives the clock output pin high or low (see clock_output_port.h).
 *        Each is a single instruction, so no other pin of the port is
 *        disturbed.
 * @param b_high true for high, false for low
 */
void clock_output_write(bool b_high)
{
    if (b_high)
    {
        PORTD |= CLOCK_OUT_BIT;
    }
    else
    {
        PORTD &= (uint8_t)~CLOCK_OUT_BIT;
    }
}
