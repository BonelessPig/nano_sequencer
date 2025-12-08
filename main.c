/**
 * @file   main.c
 * @brief  Main operating loop for ATmega328P microcontroller.
 * @author BonelessPig
 * @date   2025-12-08
 */
#include <stdio.h>          // For printf
#include <avr/io.h>         // For AVR IO registers
#include <util/delay.h>     // For _delay_ms
#include "init.h"           // For sequencer initialization
#include "serial_logger.h"  // For serial logging

#define DELAY_MS 100
#define BIT_5 0x20

/**
 * @brief  main operating loop
 * @return uint8_t status code (0 for success)
 */
uint8_t main (void) {

    /**
     * @brief Initializes the sequencer and returns a status code as an 8-bit unsigned integer. 
     *        The return value typically indicates success or failure of the initialization process.
     */
    sequencer_init();

    /**
     * @brief Sets Data Direction Registor for port B (DDRB)'s Bit Value(_BV) 
     *        to 1 for bit 5 to make it an output.
     *        DDB5 defined as int value 5, _BV gets the bit value of this
     */
    DDRB |= BIT_5; // Sets 5th but (0b00100000) to 1 to make this an output
    // DDRB |= _BV(DDB5); 

    /**
     * @brief main runtime while loop
     */
    while(1) {
        log_serial(LOGLVL_DEBUG, "Hello from ATmega328P!\r\n"); // Using custom string function

        printf("hello\n");

        PORTB |= BIT_5;  // Sets Bit Value 5 to 1 (LED On)
        _delay_ms(100);
        
        PORTB &= ~BIT_5; // Sets Bit Value 5 to 0 (LED Off)
        _delay_ms(1000);
    }
}