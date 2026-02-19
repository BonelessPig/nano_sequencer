/**
 * @file register_init.c
 * @author BonelessPig (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-12-19
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "register_init.h"
#include "../common/common_types.h"

/**
 * @brief Initializes the necessary registers for the microcontroller.
 * @return int status code (0 for success)
 */
int register_init(void) {
    // Set Data Direction Registers
    DDRB |= BIT_5;  // Sets 5th bit (0b00100000) to 1 to make this an output Data Direction Registor for port B (DDRB)
    DDRD |= BIT_0;  // Sets 0th bit (0b00000001) to 1 to make this an output Data Direction Registor for port D (DDRD)
    DDRC &= ~BIT_0; // Sets 0th bit (0b00000001) to 1 to make this an input Data Direction Registor for port C (DDRC)
    DDRC |= BIT_1;  // Sets 1st bit (0b00000010) to 1 to make this an output Data Direction Registor for port C (DDRC)

    // ADC Initialization
    ADCSRA = (1 << ADEN) |  // Enable ADC
             (1 << ADPS2) | // Set ADC prescaler to 128 for 125kHz ADC clock with 16MHz system clock
             (1 << ADPS1) | 
             (1 << ADPS0);

    return STATUS_OK; // Return success
}