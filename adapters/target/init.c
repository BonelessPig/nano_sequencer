/**
 * @file   init.c
 * @brief  Target implementation of the platform port: brings up the logger
 *         (serial or null, whichever the build links) and the MCU registers.
 * @author BonelessPig
 * @date   2025-12-08
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "platform_port.h"
#include "logger.h"
#include "register_init.h"
#include "port_status.h"



/**
 * @brief  Initializes the logger and the MCU registers (see platform_port.h).
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t platform_init(void)
{
    port_status_t status = logger_init(LOGLVL_DEBUG); // Initialize logger with DEBUG level

    if (STATUS_OK == status)
    {
        status = register_init(); // Initialize registers
    }

    return status; // STATUS_OK, or the status of the step that failed
}
