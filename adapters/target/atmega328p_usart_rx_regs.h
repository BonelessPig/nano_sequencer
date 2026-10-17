#ifndef ATMEGA328P_USART_RX_REGS_H
#define ATMEGA328P_USART_RX_REGS_H
/**
 * @file   atmega328p_usart_rx_regs.h
 * @brief  What receiving on USART0 needs beyond atmega328p_usart_regs.h: two
 *         bit positions and the receive interrupt's vector, for the
 *         ATmega328P.
 * @author BonelessPig
 *
 * These are in a header of their own because only midi_input.c uses them,
 * and only the midi build links that file; kept with the rest of the USART
 * they would be definitions that nothing in the debug firmware uses.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "atmega328p_usart_regs.h"

#define FE0    4 // Frame Error flag in UCSR0A, for the byte UDR0 gives next
#define RXCIE0 7 // RX Complete Interrupt Enable bit in UCSR0B

// The startup object's vector table jumps to this name for vector 18
// (counting the reset vector as 0), USART receive complete. A function with
// this name and USART_RX_ISR_ATTR in front is that interrupt's handler.
#define USART_RX_ISR __vector_18

// signal: save and restore every register the handler uses and return with
//         reti. used: keep the function although no C code calls it.
#define USART_RX_ISR_ATTR __attribute__((signal, used))

#endif /* ATMEGA328P_USART_RX_REGS_H */
