#ifndef __UTILITIES_H__
#define __UTILITIES_H__
/**
 * @file   utilities.h
 * @brief  Header file for utility functions and definitions.
 * @author YourName
 * @date   2024-06-15
 * 
 * @copywright Copyright (c) 2025
 * 
 */

#define delay_clocks(cycles) __builtin_avr_delay_cycles(cycles)
#define clock_cycles_per_ms 16000 // Assuming a 16 MHz clock

 /**
  * @brief  Delays execution for a specified number of milliseconds.
  * @param  milliseconds Number of milliseconds to delay.
  */
 void delay_ms(unsigned int millis);

#endif