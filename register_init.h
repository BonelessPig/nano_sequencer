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

#define BIT_0 0x01 // Bit value for bit 0 (0b00000001)
#define BIT_1 0x02 // Bit value for bit 1 (0b00000010)
#define BIT_5 0x20 // Bit value for bit 5 (0b00100000)

#define DDRB (*((volatile unsigned char*)0x24))
#define DDRC (*((volatile unsigned char*)0x27))
#define DDRD (*((volatile unsigned char*)0x2A))

#define PORTB (*((volatile unsigned char*)0x25))
#define PORTC (*((volatile unsigned char*)0x28))

/**
 * @brief Initializes the necessary registers for the microcontroller.
 */
void register_init(void);
#endif