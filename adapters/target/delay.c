/**
 * @file   delay.c
 * @brief  Target implementation of the delay port: a calibrated busy-wait.
 * @author BonelessPig
 * @date   2025-12-19
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "delay_port.h"

// F_CPU is provided by the build (see Makefile / -DF_CPU) rather than defined
// here, so there is a single source of truth for the clock speed.
#ifndef F_CPU
#error "F_CPU must be defined by the build (e.g. -DF_CPU=16000000UL)"
#endif

#define delay_clocks(cycles) __builtin_avr_delay_cycles(cycles)
#define clock_cycles_per_ms ((F_CPU) / 1000UL) // Clock cycles in one millisecond (16000 at 16 MHz)



/**
 * @brief  Delays execution for a specified number of milliseconds (see delay_port.h).
 * @param  ms Number of milliseconds to delay.
 */
void delay_wait_ms(uint16_t ms) {
    while (ms--) {
        delay_clocks(clock_cycles_per_ms); // One millisecond's worth of clock cycles, derived from F_CPU
    }
}
