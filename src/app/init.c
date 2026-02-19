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
#include "register_init.h"
#include "../common/common_types.h"



/**
 * @brief  Initializes the sequencer and serial logger.
 * @return uint8_t status code (0 for success)
 */
int sequencer_init() 
{
    int status = STATUS_OK; // Variable to store status

    status = serial_init(LOGLVL_DEBUG); // Initialize serial logger with DEBUG level
    if (status != STATUS_OK) return status; // Return if initialization failed

    status = register_init(); // Initialize registers
    if (status != STATUS_OK) return status; // Return if initialization failed

    return STATUS_OK; // Return success
}
