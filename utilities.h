#ifndef UTILITIES_H
#define UTILITIES_H
/**
 * @file   utilities.h
 * @brief  Header file for utility functions and definitions.
 * @author YourName
 * @date   2024-06-15
 * 
 * @copywright Copyright (c) 2025
 * 
 */

 /**
  * @brief  Delays execution for a specified number of CPU clocks.
  * @param  clocks Number of CPU clocks to delay.
  */
 void delay_clocks(unsigned long clocks);

 /**
  * @brief  Delays execution for a specified number of milliseconds.
  * @param  milliseconds Number of milliseconds to delay.
  */
 void delay_ms(unsigned int millis);

#endif