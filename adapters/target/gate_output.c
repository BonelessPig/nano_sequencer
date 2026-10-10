/**
 * @file   gate_output.c
 * @brief  Target implementation of the gate output port: an ordinary output
 *         pin, PD4 (D4 on the Nano). register_init() makes it an output and
 *         leaves it low.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "gate_output_port.h"
#include <stdbool.h>
#include <stdint.h>
#include "atmega328p_regs.h"
#include "bits.h"

#define GATE_BIT BIT_4 // PD4: the gate output



/**
 * @brief Drives the gate pin high or low (see gate_output_port.h). Each is a
 *        single instruction, so no other pin of the port is disturbed.
 * @param b_high true for high, false for low
 */
void gate_output_write(bool b_high)
{
    if (b_high)
    {
        PORTD |= GATE_BIT;
    }
    else
    {
        PORTD &= (uint8_t)~GATE_BIT;
    }
}
