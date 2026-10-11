#ifndef ATMEGA328P_EEPROM_REGS_H
#define ATMEGA328P_EEPROM_REGS_H
/**
 * @file   atmega328p_eeprom_regs.h
 * @brief  EEPROM registers and bit positions for the ATmega328P.
 * @author BonelessPig
 *
 * These follow the same scheme as atmega328p_regs.h: declared here, placed at
 * their datasheet addresses by atmega328p_regs.ld. They are in a header of
 * their own so that only the EEPROM adapter sees them.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>
#include "atmega328p_regs.h"

// ---- EEPROM ----
extern volatile uint8_t EECR REG_IO_LOW; // EEPROM Control Register
extern volatile uint8_t EEDR;            // EEPROM Data Register
extern volatile uint8_t EEARL;           // EEPROM Address Register, low byte
extern volatile uint8_t EEARH;           // EEPROM Address Register, high byte

#define EERE  0 // EEPROM Read Enable bit in EECR: set to read EEDR from the address
#define EEPE  1 // EEPROM Write Enable bit in EECR: set to write, reads 1 until done
#define EEMPE 2 // EEPROM Master Write Enable bit in EECR: arms EEPE for 4 clocks

#endif /* ATMEGA328P_EEPROM_REGS_H */
