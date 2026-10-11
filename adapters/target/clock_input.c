/**
 * @file   clock_input.c
 * @brief  Target implementation of the external clock input port: the input
 *         capture pin of Timer/Counter1, PB0 (D8 on the Nano). The timer's
 *         capture flag latches an edge however short the pulse, and is
 *         polled here; no interrupt is used. register_init() makes the pin
 *         an input with its pull-up on.
 * @author BonelessPig
 *
 * The input stage between the jack and the pin is a transistor that pulls
 * the pin low while the jack is high, so the edge the port reports, low to
 * high at the jack, is a falling edge at the pin.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "clock_input.h"
#include <stdbool.h>
#include <stdint.h>
#include "atmega328p_timer1_regs.h"
#include "clock_input_port.h"

#define CAPTURE_FLAG ((uint8_t)(1U << ICF1))



/**
 * @brief Sets up the capture and forgets any edge so far (see clock_input.h).
 */
void clock_input_init(void)
{
    // Plain counting, no pin driven, and no interrupt: the flag is polled,
    // and the capture vector leads nowhere but a restart
    TCCR1A = 0U;
    TIMSK1 = 0U;

    // Capture on a falling edge (ICES1 clear), through the noise canceller,
    // with the timer counting the system clock divided by 8
    TCCR1B = (uint8_t)((1U << ICNC1) | (1U << CS11));

    // Choosing the edge can set the flag; writing a one clears it
    TIFR1 = CAPTURE_FLAG;
}



/**
 * @brief  Reports and forgets an edge on the clock input (see
 *         clock_input_port.h).
 * @return true if the capture flag was set
 */
bool clock_input_edge(void)
{
    const bool b_edge = (0U != (TIFR1 & CAPTURE_FLAG));

    if (b_edge)
    {
        // Writing a one clears the flag; the zeros leave the others alone
        TIFR1 = CAPTURE_FLAG;
    }

    return b_edge;
}
