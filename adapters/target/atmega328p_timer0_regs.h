#ifndef ATMEGA328P_TIMER0_REGS_H
#define ATMEGA328P_TIMER0_REGS_H
/**
 * @file   atmega328p_timer0_regs.h
 * @brief  Timer/Counter0 registers and bit positions for the ATmega328P.
 * @author BonelessPig
 *
 * These follow the same scheme as atmega328p_regs.h: declared here, placed at
 * their datasheet addresses by atmega328p_regs.ld. They are in a header of
 * their own so that only the file that drives the timer sees them.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

// ---- Timer/Counter0 ----
extern volatile uint8_t TCCR0A; // Timer/Counter0 Control Register A
extern volatile uint8_t TCCR0B; // Timer/Counter0 Control Register B
extern volatile uint8_t OCR0A;  // Timer/Counter0 Output Compare Register A

#define COM0A1 7 // Compare Output Mode A bit 1 in TCCR0A (alone, in fast PWM:
                 // OC0A high at 0 and low at the compare match)
#define WGM01  1 // Waveform Generation Mode bit 1 in TCCR0A
#define WGM00  0 // Waveform Generation Mode bit 0 in TCCR0A (both: fast PWM,
                 // top = 255)
#define CS01   1 // Clock Select bit 1 in TCCR0B
#define CS00   0 // Clock Select bit 0 in TCCR0B (both: system clock / 64)

#endif /* ATMEGA328P_TIMER0_REGS_H */
