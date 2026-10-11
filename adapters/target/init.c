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
#include "clock_input.h"
#include "logger.h"
#include "midi_output.h"
#include "register_init.h"
#include "port_status.h"
#include "reset_input.h"
#include "spi.h"
#include "timebase.h"
#include "watchdog.h"



/**
 * @brief  Initializes the logger, the MCU registers, the SPI bus, the edge
 *         inputs, the MIDI output (the USART or nothing, whichever the
 *         build links), the watchdog and the timebase (see
 *         platform_port.h). The timebase
 *         comes last because it switches interrupts on, and it and the
 *         watchdog are only started if everything before them worked: a
 *         bring-up that fails is left halted, not reset over and over.
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
        spi_init();         // After the registers: the load line is idle by now
        clock_input_init(); // After the registers: the pins are inputs by now
        reset_input_init();
        midi_output_init(); // The USART, in the build that sends MIDI
        watchdog_init();    // While interrupts are still off; kicked on every tick
        timebase_init();    // Start the 1 kHz tick and enable interrupts
    }

    return status; // STATUS_OK, or the status of the step that failed
}
