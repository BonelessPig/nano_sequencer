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
        log_serial(LOGLVL_ERROR, "Initialization failed, status code = %d\r\n", status);
        return status; // Return error status
    }
    short delay_adc_value = 0; // Variable to store ADC value for delay
    short adc_values[6]; // Buffer to store ADC values for 6 channels

    while(1) {
        for (int i = 0; i < 6; i++) {
            status = read_analog_value(&adc_values[i], i); // Read ADC value from channel i
            if (status != 0) {
                log_serial(LOGLVL_ERROR, "ADC read failed, status code = %d\r\n", status);
                continue; // Skip this iteration on error
            }
            log_serial(LOGLVL_DEBUG, "ADC %d Value = %d\r\n", i, adc_values[i]); // Print ADC value to serial

            status = read_analog_value(&delay_adc_value, 6); // Read ADC value from channel 6, this will be used to control the delay
            delay_ms(delay_adc_value / 4); // Delay based on ADC value (0-255 ms)
        }
    }
}