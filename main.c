#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include "init.h"
#include "serial_logger.h"

#define DELAY_MS 100
#define BIT_5 0x20

int main (void) {

    sequencer_init();

    //char msg[] = "Hello\n";

    // Sets Data Direction Registor for port B (DDRB)'s Bit Value(_BV) 
    // to 1 for bit 5 to make it an output
    // DDB5 defined as int value 5, _BV gets the bit value of this
    // DDRB |= _BV(DDB5); 
    DDRB |= BIT_5; // Sets 5th but (0b00100000) to 1 to make this an output

    while(1) {
        log_serial(LOGLVL_DEBUG, "Hello from ATmega328P!\r\n"); // Using custom string function

        printf("hello\n");

        PORTB |= BIT_5;  // Sets Bit Value 5 to 1 (LED On)
        _delay_ms(100);
        
        PORTB &= ~BIT_5; // Sets Bit Value 5 to 0 (LED Off)
        _delay_ms(1000);
    }
}