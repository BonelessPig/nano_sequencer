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



/**
 * @brief  Initializes the sequencer and serial logger.
 * @return uint8_t status code (0 for success)
 */
int sequencer_init() 
{
    serial_init(LOGLVL_DEBUG); // Initialize serial logger with DEBUG level
    return 0; // Return success
}
