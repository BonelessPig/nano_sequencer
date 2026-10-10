#ifndef ATMEGA328P_USART_REGS_H
#define ATMEGA328P_USART_REGS_H
/**
 * @file   atmega328p_usart_regs.h
 * @brief  USART0 registers and bit positions for the ATmega328P.
 * @author BonelessPig
 *
 * These follow the same scheme as atmega328p_regs.h: declared here, placed at
 * their datasheet addresses by atmega328p_regs.ld. They are in a header of
 * their own because only serial_logger.c uses them, and the release build does
 * not link that file; kept with the other registers, the bit positions would
 * be definitions that nothing in the release firmware uses.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

// ---- USART0 ----
extern volatile uint8_t UBRR0H; // USART Baud Rate Register High Byte
extern volatile uint8_t UBRR0L; // USART Baud Rate Register Low  Byte

extern volatile uint8_t UCSR0A; // USART Control and Status Register A
extern volatile uint8_t UCSR0B; // USART Control and Status Register B
extern volatile uint8_t UCSR0C; // USART Control and Status Register C

extern volatile uint8_t UDR0;   // USART I/O Data Register

#define UDRE0  5 // USART Data Register Empty flag in UCSR0A
#define U2X0   1 // Double Transmission Speed bit in UCSR0A
#define RXEN0  4 // Rx Enable bit in UCSR0B
#define TXEN0  3 // Tx Enable bit in UCSR0B
#define UCSZ01 2 // Character Size bit 1 in UCSR0C
#define UCSZ00 1 // Character Size bit 0 in UCSR0C

#endif /* ATMEGA328P_USART_REGS_H */
