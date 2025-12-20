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
//#define BIT_2 (0b1 << 2) // Bit value for bit 2 (0b00000100)
//#define BIT_3 (0b1 << 3) // Bit value for bit 3 (0b00001000)
//#define BIT_4 (0b1 << 4) // Bit value for bit 4 (0b00010000)
#define BIT_5 (0b1 << 5) // Bit value for bit 5 (0b00100000)
//#define BIT_6 (0b1 << 6) // Bit value for bit 6 (0b01000000)
//#define BIT_7 (0b1 << 7) // Bit value for bit 7 (0b10000000)

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