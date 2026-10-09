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

#define MS_PER_SECOND       (1000UL)
#define CLOCK_CYCLES_PER_MS ((F_CPU) / MS_PER_SECOND) // Clock cycles in one millisecond (16000 at 16 MHz)

// avr-gcc provides this builtin without any header. The prototype matches the
// compiler's own and is here so every call has a visible declaration.
void __builtin_avr_delay_cycles(unsigned long cycles);



/**
 * @brief  Delays execution for a specified number of milliseconds (see delay_port.h).
 * @param  ms Number of milliseconds to delay.
 */
void delay_wait_ms(uint16_t ms)
{
    for (uint16_t remaining = ms; remaining > 0U; remaining--)
    {
        __builtin_avr_delay_cycles(CLOCK_CYCLES_PER_MS); // One millisecond's worth of clock cycles, derived from F_CPU
    }
}
