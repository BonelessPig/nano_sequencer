/**
 * @file   main.c
 * @brief  Main operating loop for ATmega328P microcontroller.
 * @author BonelessPig
 * @date   2025-12-08
 * 
 * @copyright Copyright (c) 2025    
 * 
 */
#include "app/init.h"            // For sequencer initialization
#include "app/serial_logger.h"   // For serial logging
#include "app/register_init.h"   // For register initialization
#include "app/analog_reader.h"   // For reading the tempo ADC channel
#include "app/shift_reg_reader.h" // For reading step note values from the 74HC165 chain
#include "common/utilities.h"      // For utility functions
#include "common/common_types.h"    // For common type definitions

#define STEP_COUNT 16 // Number of sequencer steps



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
    unsigned char step_notes[STEP_COUNT]; // Buffer to store note values for each step

    // Main loop to continuously read step notes and control delay
    while(1) {
        // Read all step note values from the shift register chain
        status = read_step_notes(step_notes, STEP_COUNT);
        if (status != 0) {
            log_serial(LOGLVL_ERROR, "Step note read failed, status code = %d\r\n", status);
        } else {
            for (int i = 0; i < STEP_COUNT; i++) {
                log_serial(LOGLVL_DEBUG, "Step %d Note = %d\r\n", i, step_notes[i]); // Print note value to serial
            }
        }

        // Read ADC value from channel 6 once per sweep, this will be used to control the delay
        status = read_analog_value(&delay_adc_value, 6);
        if (status != 0) {
            log_serial(LOGLVL_ERROR, "ADC read failed for delay channel, status code = %d\r\n", status);
            continue; // Skip this iteration on error
        }
        delay_ms(delay_adc_value / 4); // Delay based on ADC value (0-255 ms)
    }
}