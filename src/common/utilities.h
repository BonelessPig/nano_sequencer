#ifndef __UTILITIES_H__
#define __UTILITIES_H__
/**
 * @file   utilities.h
 * @brief  Header file for utility functions and definitions.
 * @author BonelessPig
 * @date   2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#define delay_clocks(cycles) __builtin_avr_delay_cycles(cycles)
#define clock_cycles_per_ms 16000 // Assuming a 16 MHz clock

 /**
  * @brief  Delays execution for a specified number of milliseconds.
  * @param  milliseconds Number of milliseconds to delay.
  */
 void delay_ms(unsigned int millis);

 void *memset(void *s, int c, unsigned int n);

 void *memmove(void *dest, const void *src, unsigned int n);

#endif