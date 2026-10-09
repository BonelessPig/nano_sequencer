/**
 * @file   init.c
 * @brief  Target implementation of the platform port: brings up the serial
 *         logger and the MCU registers.
 * @author BonelessPig
 * @date   2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "platform_port.h"
#include "serial_logger.h"
#include "register_init.h"
#include "port_status.h"



/**
 * @brief  Initializes the serial logger and the MCU registers (see platform_port.h).
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t platform_init(void)
{
    port_status_t status = STATUS_OK; // Variable to store status

    status = serial_init(LOGLVL_DEBUG); // Initialize serial logger with DEBUG level
    if (status != STATUS_OK) return status; // Return if initialization failed

    status = register_init(); // Initialize registers
    if (status != STATUS_OK) return status; // Return if initialization failed

    return STATUS_OK; // Return success
}
