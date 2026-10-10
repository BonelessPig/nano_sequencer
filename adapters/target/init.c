/**
 * @file   init.c
 * @brief  Target implementation of the platform port: brings up the logger
 *         (serial or null, whichever the build links), the MCU registers,
 *         the SPI bus and the timebase.
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
#include "spi.h"
#include "timebase.h"



/**
 * @brief  Initializes the logger, the MCU registers, the SPI bus and the
 *         timebase (see platform_port.h). The timebase comes last because it
 *         switches interrupts on, and is only started if everything before
 *         it worked.
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t platform_init(void)
{
    port_status_t status = logger_init(LOGLVL_DEBUG); // Initialize logger with DEBUG level

    if (STATUS_OK == status)
    {
        status = register_init(); // Initialize registers
    }

    if (STATUS_OK == status)
    {
        spi_init();      // After the registers: the load line is idle by now
        timebase_init(); // Start the 1 kHz tick and enable interrupts
    }

    return status; // STATUS_OK, or the status of the step that failed
}
