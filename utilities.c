/**
 * @file utilities.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-12-19
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "utilities.h"

/**
 * @brief  Delays execution for a specified number of CPU clocks.
 * @param  clocks Number of CPU clocks to delay.
 */
void delay_clocks(unsigned long clocks) {
    while (clocks--) {
        __asm__ __volatile__ ("nop");
    }
}

/**
 * @brief  Delays execution for a specified number of milliseconds.
 * @param  millis Number of milliseconds to delay.
 */ 
void delay_ms(unsigned int millis) {
    while (millis--) {
        delay_clocks(16000); // Assuming a 16 MHz clock, 16000 clocks = 1 ms
    }
}