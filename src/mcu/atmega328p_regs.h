#ifndef __ATMEGA328P_REGS_H__
#define __ATMEGA328P_REGS_H__
/**
 * @file   atmega328p_regs.h
 * @brief  Memory-mapped register addresses and bit positions for the ATmega328P.
 * @author BonelessPig
 *
 * Addresses and bit positions are taken directly from the ATmega328P datasheet.
 * This file has no dependency on avr-libc's <avr/io.h> — it exists so the rest
 * of the codebase doesn't need to know raw addresses, without pulling in
 * avr-libc.
 *
 * @copyright Copyright (c) 2026
 *
 */

// GCC's -Warray-bounds pass mistakes these fixed-address volatile pointer casts
// for indexing into a zero-length array, since it can't know an absolute address
// like 0x24 is a real hardware register rather than an out-of-bounds object. This
// is a known false positive for this style of raw memory-mapped I/O access (the
// same pattern avr-libc's <avr/io.h> uses internally) — not a real bug. GCC ties
// the diagnostic to where a macro is expanded, not where it's #defined, so this
// stays in effect for the rest of any file that includes this header (there is
// deliberately no matching "pop") rather than only around the lines below.
#pragma GCC diagnostic ignored "-Warray-bounds"

// ---- I/O Ports: Data Direction, Output, and Input registers ----
#define DDRA  (*((volatile unsigned char*)0x23)) // Data Direction Register for port A
#define DDRB  (*((volatile unsigned char*)0x24)) // Data Direction Register for port B
#define DDRC  (*((volatile unsigned char*)0x27)) // Data Direction Register for port C
#define DDRD  (*((volatile unsigned char*)0x2A)) // Data Direction Register for port D

#define PORTA (*((volatile unsigned char*)0x22)) // Data Register for port A
#define PORTB (*((volatile unsigned char*)0x25)) // Data Register for port B
#define PORTC (*((volatile unsigned char*)0x28)) // Data Register for port C
#define PORTD (*((volatile unsigned char*)0x2B)) // Data Register for port D

#define PINC  (*(volatile unsigned char*)(0x26)) // Input Pins Address for port C
#define PIND  (*(volatile unsigned char*)(0x29)) // Input Pins Address for port D

// ---- ADC (Analog to Digital Converter) ----
#define ADCSRA (*((volatile unsigned char*)0x7A)) // ADC Control and Status Register A
#define ADMUX  (*((volatile unsigned char*)0x7C)) // ADC Multiplexer Selection Register
#define ADC    (*((volatile unsigned int*)0x78))  // ADC Data Register (10-bit result)

#define ADEN  7 // ADC Enable bit in ADCSRA
#define ADSC  6 // ADC Start Conversion bit in ADCSRA
#define ADPS2 2 // ADC Prescaler Select Bit 2 in ADCSRA
#define ADPS1 1 // ADC Prescaler Select Bit 1 in ADCSRA
#define ADPS0 0 // ADC Prescaler Select Bit 0 in ADCSRA

#define REFS0 6 // Reference Selection Bit 0 in ADMUX

// ---- USART0 ----
#define UBRR0H (*(volatile unsigned char*)0xC5) // USART Baud Rate Register High Byte
#define UBRR0L (*(volatile unsigned char*)0xC4) // USART Baud Rate Register Low  Byte

#define UCSR0A (*(volatile unsigned char*)0xC0) // USART Control and Status Register A
#define UCSR0B (*(volatile unsigned char*)0xC1) // USART Control and Status Register B

#define UDR0   (*(volatile unsigned char*)0xC6) // USART I/O Data Register

#define UDRE0 5 // USART Data Register Empty flag in UCSR0A
#define RXEN0 4 // Rx Enable bit in UCSR0B
#define TXEN0 3 // Tx Enable bit in UCSR0B

#endif
