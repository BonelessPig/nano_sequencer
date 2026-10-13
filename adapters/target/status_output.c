/**
 * @file   status_output.c
 * @brief  Target implementation of the status light port: an ordinary output
 *         pin, PD3 (D3 on the Nano), high for lit. register_init() makes it
 *         an output and leaves it low.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "status_output_port.h"
#include <stdbool.h>
#include <stdint.h>
#include "atmega328p_regs.h"
#include "bits.h"

#define STATUS_BIT BIT_3 // PD3: the status light



/**
 * @brief Drives the light's pin high or low (see status_output_port.h). Each
 *        is a single instruction, so no other pin of the port is disturbed.
 * @param b_lit true for lit (high), false for dark (low)
 */
void status_output_write(bool b_lit)
{
    if (b_lit)
    {
        PORTD |= STATUS_BIT;
    }
    else
    {
        PORTD &= (uint8_t)~STATUS_BIT;
    }
}
