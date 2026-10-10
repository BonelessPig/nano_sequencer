/**
 * @file register_init.c
 * @author BonelessPig
 * @brief Implementation of register initialization: I/O pin directions and ADC setup.
 * @version 0.1
 * @date 2025-12-19
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "register_init.h"
#include "port_status.h"

/**
 * @brief Initializes the necessary registers for the microcontroller.
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t register_init(void)
{
    // Set Data Direction Registers
    DDRB |= BIT_5;  // Sets 5th bit (0b00100000) to 1 to make this an output Data Direction Registor for port B (DDRB)
    DDRC &= (uint8_t)~BIT_0; // Clears 0th bit (0b00000001) to 0 to make this an input Data Direction Registor for port C (DDRC)
    DDRC |= BIT_1;  // Sets 1st bit (0b00000010) to 1 to make this an output Data Direction Registor for port C (DDRC)

    DDRD |= BIT_2;  // Shift register SH/LD (load) line, output
    DDRD |= BIT_3;  // Shift register CLK (clock) line, output
    DDRD &= (uint8_t)~BIT_4; // Shift register SER (serial data in) line, input

    // ADC Initialization
    ADCSRA = (uint8_t)((1U << ADEN) |   // Enable ADC
                       (1U << ADPS2) |  // Set ADC prescaler to 128 for 125kHz ADC clock with 16MHz system clock
                       (1U << ADPS1) |
                       (1U << ADPS0));

    return STATUS_OK; // Return success
}
