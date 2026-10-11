#ifndef ATMEGA328P_EXTINT_REGS_H
#define ATMEGA328P_EXTINT_REGS_H
/**
 * @file   atmega328p_extint_regs.h
 * @brief  External interrupt registers and bit positions for the ATmega328P:
 *         only those the edge flag of INT0 (PD2) needs.
 * @author BonelessPig
 *
 * These follow the same scheme as atmega328p_regs.h: declared here, placed at
 * their datasheet addresses by atmega328p_regs.ld. They are in a header of
 * their own so that only the file that uses the external interrupt sees them.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>
#include "atmega328p_regs.h"

// ---- External interrupts ----
extern volatile uint8_t EICRA;            // External Interrupt Control Register A
extern volatile uint8_t EIMSK REG_IO_LOW; // External Interrupt Mask Register
extern volatile uint8_t EIFR REG_IO_LOW;  // External Interrupt Flag Register

#define ISC01 1 // Interrupt Sense Control 0 bit 1 in EICRA (alone: falling edge)
#define INTF0 0 // External Interrupt Flag 0 in EIFR: set by an edge, cleared by writing 1

#endif /* ATMEGA328P_EXTINT_REGS_H */
