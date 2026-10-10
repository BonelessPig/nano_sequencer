#ifndef ATMEGA328P_SPI_REGS_H
#define ATMEGA328P_SPI_REGS_H
/**
 * @file   atmega328p_spi_regs.h
 * @brief  SPI registers and bit positions for the ATmega328P.
 * @author BonelessPig
 *
 * These follow the same scheme as atmega328p_regs.h: declared here, placed at
 * their datasheet addresses by atmega328p_regs.ld. They are in a header of
 * their own so that only the SPI driver sees them.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

// ---- SPI ----
extern volatile uint8_t SPCR; // SPI Control Register
extern volatile uint8_t SPSR; // SPI Status Register
extern volatile uint8_t SPDR; // SPI Data Register

#define SPE  6 // SPI Enable bit in SPCR
#define MSTR 4 // Master/Slave Select bit in SPCR (set: master)
#define SPR0 0 // SPI Clock Rate Select bit 0 in SPCR (alone: system clock / 16)

#define SPIF 7 // SPI Interrupt Flag in SPSR: set when a transfer completes

#endif /* ATMEGA328P_SPI_REGS_H */
