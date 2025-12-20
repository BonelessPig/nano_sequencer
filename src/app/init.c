/**
 * @file   init.c
 * @brief  Implementation of sequencer initialization on AVR microcontrollers.
 * @author BonelessPig
 * @date   2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "init.h"
#include "serial_logger.h"

// Set up a FILE stream for serial output
static const FILE serial_output = FDEV_SETUP_STREAM(add_char_serial, NULL, _FDEV_SETUP_WRITE);

/**
 * @brief  Initializes the sequencer and serial logger.
 * @return uint8_t status code (0 for success)
 */
uint8_t sequencer_init() 
{
    serial_init(LOGLVL_DEBUG); // Initialize serial logger with DEBUG level
    stdout = &serial_output;   // Redirect standard output to serial
}
