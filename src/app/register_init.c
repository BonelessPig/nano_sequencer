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

/**
 * @brief Initializes the necessary registers for the microcontroller.
 */
void register_init(void) {
    DDRB |= BIT_5;  // Sets 5th bit (0b00100000) to 1 to make this an output Data Direction Registor for port B (DDRB)
    DDRD |= BIT_0;  // Sets 0th bit (0b00000001) to 1 to make this an output Data Direction Registor for port D (DDRD)
    DDRC &= ~BIT_0; // Sets 0th bit (0b00000001) to 1 to make this an input Data Direction Registor for port C (DDRC)
    DDRC |= BIT_1;  // Sets 1st bit (0b00000010) to 1 to make this an output Data Direction Registor for port C (DDRC)
}