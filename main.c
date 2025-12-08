#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include "init.h"
#include "serial_logger.h"

#define DELAY_MS 1000

int main (void) {

    sequencer_init();

    //char msg[] = "Hello\n";

    DDRB |= _BV(DDB5);
    while(1){
        log_serial("Hello from ATmega328P!\r\n"); // Using custom string function

        printf("hello\n");

        PORTB |= _BV(PORTB5);
        _delay_ms(DELAY_MS);
        
        PORTB &= ~_BV(PORTB5);
        _delay_ms(DELAY_MS);
    }
}