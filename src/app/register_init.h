#ifndef __REGISTER_INIT_H__
#define __REGISTER_INIT_H__
/**
 * @file   register_init.h
 * @brief  Header file for register initialization on AVR microcontrollers.
 * @author BonelessPig
 * @date   2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#define BIT_0  0b1       // Bit value for bit 0 (0b00000001)
#define BIT_1 (0b1 << 1) // Bit value for bit 1 (0b00000010)
#define BIT_2 (0b1 << 2) // Bit value for bit 2 (0b00000100)
#define BIT_3 (0b1 << 3) // Bit value for bit 3 (0b00001000)
#define BIT_4 (0b1 << 4) // Bit value for bit 4 (0b00010000)
#define BIT_5 (0b1 << 5) // Bit value for bit 5 (0b00100000)
#define BIT_6 (0b1 << 6) // Bit value for bit 6 (0b01000000)
#define BIT_7 (0b1 << 7) // Bit value for bit 7 (0b10000000)

// ADC Register Definitions (Analog to Digital Converter)
#define ADCSRA (*((volatile unsigned char*)0x7A)) // ADC Control and Status Register A
#define ADMUX  (*((volatile unsigned char*)0x7C)) // ADC Multiplexer Selection Register

// ADC Register Bit Definitions
#define ADEN  7                                 // ADC Enable bit in ADCSRA
#define ADSC  6                                 // ADC Start Conversion bit in ADCSRA
#define ADPS2 2                                 // ADC Prescaler Select Bit 2 in ADCSRA
#define ADPS1 1                                 // ADC Prescaler Select Bit 1 in ADCSRA
#define ADPS0 0                                 // ADC Prescaler Select Bit 0 in ADCSRA

#define ADC   (*((volatile unsigned int*)0x78)) // ADC Data Register (10-bit result from ADC)

#define DDRA (*((volatile unsigned char*)0x23)) // Data Direction Register for port A
#define DDRB (*((volatile unsigned char*)0x24)) // Data Direction Register for port B
#define DDRC (*((volatile unsigned char*)0x27)) // Data Direction Register for port C
#define DDRD (*((volatile unsigned char*)0x2A)) // Data Direction Register for port D

#define PORTA (*((volatile unsigned char*)0x22)) // Data Register for port A
#define PORTB (*((volatile unsigned char*)0x25)) // Data Register for port B
#define PORTC (*((volatile unsigned char*)0x28)) // Data Register for port C
#define PORTD (*((volatile unsigned char*)0x2B)) // Data Register for port D

#define PINC (*(volatile uint8_t *)(0x06)) // Input Pins Address for port C

#define REFS0 6 // Reference Selection Bit 0 in ADMUX

/**
 * @brief Initializes the necessary registers for the microcontroller.
 */
void register_init(void);

#endif