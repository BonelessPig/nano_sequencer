#ifndef ATMEGA328P_TIMER2_REGS_H
#define ATMEGA328P_TIMER2_REGS_H
/**
 * @file   atmega328p_timer2_regs.h
 * @brief  Timer/Counter2 registers, bit positions and compare match A
 *         interrupt vector for the ATmega328P.
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

// ---- Timer/Counter2 ----
extern volatile uint8_t TCCR2A; // Timer/Counter2 Control Register A
extern volatile uint8_t TCCR2B; // Timer/Counter2 Control Register B
extern volatile uint8_t TCNT2;  // Timer/Counter2 count
extern volatile uint8_t OCR2A;  // Timer/Counter2 Output Compare Register A
extern volatile uint8_t TIMSK2; // Timer/Counter2 Interrupt Mask Register

#define WGM21  1 // Waveform Generation Mode bit 1 in TCCR2A (alone: CTC, top = OCR2A)
#define CS22   2 // Clock Select bit 2 in TCCR2B (alone: system clock / 64)
#define OCIE2A 1 // Output Compare Match A Interrupt Enable bit in TIMSK2

// The startup object's vector table jumps to this name for vector 7 (counting
// the reset vector as 0), Timer/Counter2 compare match A. A function with this
// name and TIMER2_COMPA_ISR_ATTR in front is that interrupt's handler.
#define TIMER2_COMPA_ISR __vector_7

// Makes a function an interrupt handler: the compiler saves every register it
// uses and returns with reti. "used" keeps the function although nothing in
// the C source calls it.
#define TIMER2_COMPA_ISR_ATTR __attribute__((signal, used))

#endif /* ATMEGA328P_TIMER2_REGS_H */
