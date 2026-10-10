#ifndef ATMEGA328P_REGS_H
#define ATMEGA328P_REGS_H
/**
 * @file   atmega328p_regs.h
 * @brief  Memory-mapped registers and bit positions for the ATmega328P.
 * @author BonelessPig
 *
 * Each register is declared here as an ordinary variable. Its address is not
 * in the C source: the linker places every one of these names at its datasheet
 * address, using the list in atmega328p_regs.ld (same folder). A register must
 * appear in both files; one declared here but missing there fails at link time
 * with "undefined reference".
 *
 * The USART0 registers are in atmega328p_usart_regs.h, because only the debug
 * build's serial logger uses them. The Timer/Counter2 registers are in
 * atmega328p_timer2_regs.h.
 *
 * Bit positions are taken directly from the ATmega328P datasheet. This file
 * has no dependency on avr-libc's <avr/io.h> — it exists so the rest of the
 * codebase can name registers without pulling in avr-libc.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

// Marks a register in the low I/O range (data addresses 0x20 to 0x3F). The
// compiler cannot see the address, so this avr-gcc attribute is what lets it
// use the single-instruction bit operations (sbi, cbi, sbic, sbis) there. The
// shift register clock and load pulses rely on those.
#define REG_IO_LOW __attribute__((io_low))

// ---- I/O Ports: Data Direction, Output, and Input registers ----
extern volatile uint8_t DDRB REG_IO_LOW;  // Data Direction Register for port B
extern volatile uint8_t DDRC REG_IO_LOW;  // Data Direction Register for port C
extern volatile uint8_t DDRD REG_IO_LOW;  // Data Direction Register for port D

extern volatile uint8_t PORTB REG_IO_LOW; // Data Register for port B
extern volatile uint8_t PORTC REG_IO_LOW; // Data Register for port C
extern volatile uint8_t PORTD REG_IO_LOW; // Data Register for port D

extern volatile uint8_t PINB REG_IO_LOW;  // Input Pins Address for port B
extern volatile uint8_t PINC REG_IO_LOW;  // Input Pins Address for port C
extern volatile uint8_t PIND REG_IO_LOW;  // Input Pins Address for port D

// ---- ADC (Analog to Digital Converter) ----
extern volatile uint8_t  ADCSRA; // ADC Control and Status Register A
extern volatile uint8_t  ADMUX;  // ADC Multiplexer Selection Register
extern volatile uint16_t ADC;    // ADC Data Register (10-bit result)

#define ADEN  7 // ADC Enable bit in ADCSRA
#define ADSC  6 // ADC Start Conversion bit in ADCSRA
#define ADPS2 2 // ADC Prescaler Select Bit 2 in ADCSRA
#define ADPS1 1 // ADC Prescaler Select Bit 1 in ADCSRA
#define ADPS0 0 // ADC Prescaler Select Bit 0 in ADCSRA

#define REFS0 6 // Reference Selection Bit 0 in ADMUX

// ---- CPU ----
extern volatile uint8_t SREG; // Status Register

#define SREG_I 7 // Global Interrupt Enable bit in SREG

#endif /* ATMEGA328P_REGS_H */
