/**
 * @file   main.c
 * @brief  Main operating loop for ATmega328P microcontroller.
 * @author BonelessPig
 * @date   2025-12-08
 * 
 * @copyright Copyright (c) 2025    
 * 
 */
#include "app/init.h"           // For sequencer initialization
#include "app/serial_logger.h"  // For serial logging
#include "app/register_init.h"  // For register initialization
#include "common/utilities.h"      // For utility functions
#include "common/common_types.h"    // For common type definitions



/**
 * @brief  main operating loop
 * @return int status code (0 for success)
 */
int main (void) {

    int status = STATUS_OK; // Variable to store status
    status = sequencer_init();
    if (status != 0) {
        log_serial(LOGLVL_ERROR, "Initialization failed\r\n");
        return status; // Return error status
    }
    char buffer[64]; // Buffer for formatted output
    memset(buffer, 0, sizeof(buffer)); // Clear the buffer
    short adc_value = 0; // Variable to store ADC value

    while(1) {
        status = read_analog_value(&adc_value, 0); // Read ADC value from channel 0
        if (status != 0) {
            snprintf(buffer, sizeof(buffer), "ADC read failed, status code = %d\r\n", status);
            log_serial(LOGLVL_ERROR, buffer);
            continue; // Skip this iteration on error
        }
        snprintf(buffer, sizeof(buffer), "ADC Value = %d\r\n", adc_value);
        log_serial(LOGLVL_DEBUG, buffer); // Print ADC value to serial
        delay_ms(50);
    }
}