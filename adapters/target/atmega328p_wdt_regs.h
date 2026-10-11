#ifndef ATMEGA328P_WDT_REGS_H
#define ATMEGA328P_WDT_REGS_H
/**
 * @file   atmega328p_wdt_regs.h
 * @brief  Watchdog timer register, bit positions and reset instruction for
 *         the ATmega328P: only what the watchdog adapter needs.
 * @author BonelessPig
 *
 * This follows the same scheme as atmega328p_regs.h: declared here, placed at
 * its datasheet address by atmega328p_regs.ld. It is in a header of its own
 * so that only the file that uses the watchdog sees it.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>
#include "atmega328p_regs.h"

// ---- Watchdog timer ----
extern volatile uint8_t WDTCSR; // Watchdog Timer Control and Status Register

#define WDP2 2 // Watchdog prescaler bit 2 in WDTCSR (alone: about 250 ms)
#define WDE  3 // Watchdog System Reset Enable in WDTCSR
#define WDCE 4 // Watchdog Change Enable in WDTCSR: opens it for four cycles

// The compiler's own name for the watchdog reset instruction (wdr). Declared
// so that the call below has a prototype in sight
extern void __builtin_avr_wdr(void);

/**
 * @brief Starts the watchdog's count again from zero: one wdr instruction.
 */
static inline void wdt_reset_instruction(void)
{
    __builtin_avr_wdr();
}

#endif /* ATMEGA328P_WDT_REGS_H */
