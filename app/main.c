/**
 * @file   main.c
 * @brief  Main operating loop for ATmega328P microcontroller.
 * @author BonelessPig
 * @date   2025-12-08
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "app.h"         // For the application's init and tick functions
#include "port_status.h" // For status codes



/**
 * @brief  main operating loop
 * @return int status code (0 for success); only returns if initialization failed
 */
int main(void)
{
    const port_status_t status = app_init();

    if (STATUS_OK == status)
    {
        // Main loop: each pass runs a sequencer tick if one is due and never waits
        for (;;)
        {
            app_run_once();
        }
    }

    return (int)status; // Return error status
}
