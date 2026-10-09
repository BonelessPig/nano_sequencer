/**
 * @file   main.c
 * @brief  Main operating loop for ATmega328P microcontroller.
 * @author BonelessPig
 * @date   2025-12-08
 *
 * @copyright Copyright (c) 2025
 *
 */
#include <stdint.h>
#include "platform_port.h"    // For platform initialization
#include "log_port.h"         // For logging
#include "tempo_input_port.h" // For reading the tempo control
#include "delay_port.h"       // For the inter-sweep delay
#include "port_status.h"      // For status codes
#include "shift_reg_reader.h" // For reading step note values from the 74HC165 chain

#define STEP_COUNT 16 // Number of sequencer steps



/**
 * @brief  main operating loop
 * @return int status code (0 for success)
 */
int main (void) {

    int status = STATUS_OK; // Variable to store status
    status = platform_init();
    if (status != 0) {
        log_error(LOG_ERROR_INIT, (port_status_t)status);
        return status; // Return error status
    }
    uint16_t delay_adc_value = 0; // Variable to store ADC value for delay
    unsigned char step_notes[STEP_COUNT]; // Buffer to store note values for each step

    // Main loop to continuously read step notes and control delay
    while(1) {
        // Read all step note values from the shift register chain
        status = read_step_notes(step_notes, STEP_COUNT);
        if (status != 0) {
            log_error(LOG_ERROR_STEP_READ, (port_status_t)status);
        } else {
            for (int i = 0; i < STEP_COUNT; i++) {
                log_step_note((uint8_t)i, step_notes[i]); // Print note value to serial
            }
        }

        // Read ADC value from channel 6 once per sweep, this will be used to control the delay
        status = tempo_input_read(&delay_adc_value);
        if (status != 0) {
            // delay_adc_value is left untouched on failure, so the last good reading is reused below
            log_error(LOG_ERROR_TEMPO_READ, (port_status_t)status);
        }
        delay_wait_ms(delay_adc_value / 4); // Delay based on ADC value (0-255 ms)
    }
}