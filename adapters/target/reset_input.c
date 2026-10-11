/**
 * @file   reset_input.c
 * @brief  Target implementation of the reset input port: the external
 *         interrupt 0 pin, PD2 (D2 on the Nano). Its flag latches an edge
 *         however short the pulse, and is polled here; the interrupt itself
 *         stays off. register_init() makes the pin an input with its
 *         pull-up on.
 * @author BonelessPig
 *
 * The input stage between the jack and the pin is a transistor that pulls
 * the pin low while the jack is high, so the edge the port reports, low to
 * high at the jack, is a falling edge at the pin.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "reset_input.h"
#include <stdbool.h>
#include <stdint.h>
#include "atmega328p_extint_regs.h"
#include "reset_input_port.h"

#define EDGE_FLAG ((uint8_t)(1U << INTF0))



/**
 * @brief Sets up the edge flag and forgets any edge so far (see reset_input.h).
 */
void reset_input_init(void)
{
    // No external interrupt is enabled: the flag is polled, and the vector
    // leads nowhere but a restart
    EIMSK = 0U;

    // The flag is set by a falling edge on the pin
    EICRA = (uint8_t)(1U << ISC01);

    // Choosing the edge can set the flag; writing a one clears it
    EIFR = EDGE_FLAG;
}



/**
 * @brief  Reports and forgets an edge on the reset input (see
 *         reset_input_port.h).
 * @return true if the edge flag was set
 */
bool reset_input_edge(void)
{
    const bool b_edge = (0U != (EIFR & EDGE_FLAG));

    if (b_edge)
    {
        // Writing a one clears the flag; the zeros leave the others alone
        EIFR = EDGE_FLAG;
    }

    return b_edge;
}
