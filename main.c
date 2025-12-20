/**
 * @file   main.c
 * @brief  Main operating loop for ATmega328P microcontroller.
 * @author BonelessPig
 * @date   2025-12-08
 * 
 * @copyright Copyright (c) 2025    
 * 
 */
#include "init.h"           // For sequencer initialization
#include "serial_logger.h"  // For serial logging
#include "register_init.h"  // For register initialization
#include "utilities.h"      // For utility functions



/**
 * @brief  main operating loop
 * @return uint8_t status code (0 for success)
 */
int main (void) {

    sequencer_init();
    register_init();

    while(1) {
        log_serial(LOGLVL_DEBUG, "Hello from ATmega328P!\r\n"); // Using custom string function

        //log_serial(LOGLVL_DEBUG, "Port C0 = %d\n", (PINC & BIT_0) ? 1 : 0); // Reads the value of Pin C0 and prints it to serial

        //PORTC1 |= PORTC2;  // Sets Bit Value 1 to 1 (Output High)

        PORTB |= BIT_5;  // Sets Bit Value 5 to 1 (LED On)
        delay_ms(5000000);
        
        PORTB &= ~BIT_5; // Sets Bit Value 5 to 0 (LED Off)
        delay_ms(5000000);
    }
}