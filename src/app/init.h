#ifndef __INIT_H__
#define __INIT_H__
/**
 * @file   init.h
 * @brief  Header file for sequencer initialization on AVR microcontrollers.
 * @author BonelessPig
 * @date   2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <stdio.h> // For FILE type

/**
 * @brief  Initializes the sequencer and serial logger.
 * @return uint8_t status code (0 for success)
 */
uint8_t sequencer_init();

#endif