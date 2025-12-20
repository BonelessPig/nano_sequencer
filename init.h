#ifndef SEQUENCER_INIT_H
#define SEQUENCER_INIT_H
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
#include <avr/io.h> // For AVR IO registers
#include "serial_logger.h" // For serial logging functions

/**
 * @brief  Initializes the sequencer and serial logger.
 * @return uint8_t status code (0 for success)
 */
uint8_t sequencer_init();

#endif