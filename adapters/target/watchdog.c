/**
 * @file   watchdog.c
 * @brief  Target implementation of the watchdog port: the MCU's watchdog
 *         timer, which resets the MCU if it is not kicked in time.
 * @author BonelessPig
 *
 * The timer runs from its own 128 kHz oscillator, so it keeps counting
 * whatever the firmware is doing. Its timeout is set once, at start-up, to
 * about 250 ms. After a reset it caused, the hardware starts it again by
 * itself with the shortest timeout (about 16 ms) before any code has run;
 * watchdog_init() is reached well inside that.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "watchdog_port.h"
#include "atmega328p_wdt_regs.h"
#include "watchdog.h"

// Opens the register for a change: the next write, within four clock
// cycles, may set the enable and prescaler bits
#define WDT_OPEN ((uint8_t)((1U << WDCE) | (1U << WDE)))

// Reset on timeout, no interrupt, prescaler 32768 cycles of 128 kHz: 256 ms
#define WDT_RESET_AFTER_250_MS ((uint8_t)((1U << WDE) | (1U << WDP2)))



void watchdog_init(void)
{
    // Start the count afresh first, so that a short timeout left running by
    // a watchdog reset cannot run out half way through
    wdt_reset_instruction();
    WDTCSR = WDT_OPEN;
    WDTCSR = WDT_RESET_AFTER_250_MS;
}



void watchdog_kick(void)
{
    wdt_reset_instruction();
}
