/**
 * @file   timebase.c
 * @brief  Target implementation of the timebase port: a 1 kHz tick from
 *         Timer/Counter2. The interrupt only counts; the main loop asks how
 *         many ticks have gone by.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "timebase_port.h"
#include <stdint.h>
#include "atmega328p_regs.h"
#include "atmega328p_timer2_regs.h"
#include "timebase.h"

// F_CPU is provided by the build (see Makefile / -DF_CPU) rather than defined
// here, so there is a single source of truth for the clock speed.
#ifndef F_CPU
#error "F_CPU must be defined by the build (e.g. -DF_CPU=16000000UL)"
#endif

// The timer counts the system clock divided by 64 and starts again after
// TIMER_COUNTS_PER_TICK counts: 16 MHz / 64 / 250 is exactly 1000 Hz.
#define TICK_HZ               (1000UL)
#define TIMER_PRESCALER       (64UL) // Must match the clock select bits below
#define TIMER_COUNTS_PER_TICK ((F_CPU) / (TIMER_PRESCALER * TICK_HZ))
#define TIMER_TOP             ((uint8_t)(TIMER_COUNTS_PER_TICK - 1UL))

// Ticks since start-up, wrapping at 256. Written only by the interrupt and
// read only by timebase_elapsed_ticks(). It is one byte, so a read is a
// single instruction and needs no interrupt masking.
static volatile uint8_t g_tick_count = 0U;

static uint8_t g_ticks_reported = 0U; // g_tick_count as of the last report



/**
 * @brief Timer/Counter2 compare match A interrupt, once a millisecond: counts
 *        the tick and does nothing else.
 */
TIMER2_COMPA_ISR_ATTR void TIMER2_COMPA_ISR(void);
/* DEVIATION: MISRA 8.7 (advisory) - a false positive rather than a real
 * departure from the rule. This function is referenced from outside this
 * file, but not from C: the interrupt vector table in the toolchain's startup
 * object (crtatmega328p.o, added at link time) holds a jump to the name
 * __vector_7, and the linker binds that jump to this function. The linker can
 * only match global symbols, so the function must have external linkage.
 * cppcheck analyses the C source alone, never sees the vector table, and so
 * reports the function as used in one file only. A vector table of our own
 * would be assembly and equally invisible to it, so the finding does not
 * depend on where the table comes from. */
// cppcheck-suppress misra-c2012-8.7
void TIMER2_COMPA_ISR(void)
{
    g_tick_count++;
}



void timebase_init(void)
{
    g_tick_count     = 0U;
    g_ticks_reported = 0U;

    TCCR2B = 0U;                       // Stop the timer while it is set up
    TCNT2  = 0U;
    OCR2A  = TIMER_TOP;
    TCCR2A = (uint8_t)(1U << WGM21);   // Clear on compare match, top = OCR2A
    TIMSK2 = (uint8_t)(1U << OCIE2A);  // Interrupt on compare match A
    TCCR2B = (uint8_t)(1U << CS22);    // Run from the system clock / 64

    SREG |= (uint8_t)(1U << SREG_I);   // Enable interrupts globally
}



/**
 * @brief  Reports the ticks since the previous call (see timebase_port.h).
 * @return Elapsed ticks, 0 to 255
 */
uint8_t timebase_elapsed_ticks(void)
{
    const uint8_t now     = g_tick_count;
    const uint8_t elapsed = (uint8_t)(now - g_ticks_reported);

    g_ticks_reported = now;

    return elapsed;
}
