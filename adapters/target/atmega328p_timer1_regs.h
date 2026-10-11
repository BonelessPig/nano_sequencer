#ifndef ATMEGA328P_TIMER1_REGS_H
#define ATMEGA328P_TIMER1_REGS_H
/**
 * @file   atmega328p_timer1_regs.h
 * @brief  Timer/Counter1 registers and bit positions for the ATmega328P:
 *         only those the input capture on ICP1 (PB0) needs.
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
#include "atmega328p_regs.h"

// ---- Timer/Counter1 ----
extern volatile uint8_t TCCR1A;           // Timer/Counter1 Control Register A
extern volatile uint8_t TCCR1B;           // Timer/Counter1 Control Register B
extern volatile uint8_t TIMSK1;           // Timer/Counter1 Interrupt Mask Register
extern volatile uint8_t TIFR1 REG_IO_LOW; // Timer/Counter1 Interrupt Flag Register

#define ICNC1 7 // Input Capture Noise Canceler bit in TCCR1B
#define CS11  1 // Clock Select bit 1 in TCCR1B (alone: system clock / 8)
#define ICF1  5 // Input Capture Flag in TIFR1: set by an edge, cleared by writing 1

#endif /* ATMEGA328P_TIMER1_REGS_H */
