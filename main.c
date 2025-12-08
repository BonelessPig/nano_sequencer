#define F_CPU 16000000UL // Or your ATmega328P's clock frequency
#define BAUD 9600 // Desired baud rate

#include <avr/io.h>
#include <util/delay.h>
#include <util/setbaud.h>
#include <stdio.h>


#define DELAY_MS 1000

void uart_init(void) {
    UBRR0H = UBRRH_VALUE; // Set baud rate high byte
    UBRR0L = UBRRL_VALUE; // Set baud rate low byte

    // Enable receiver and transmitter
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);

    // Set frame format: 8 data bits, 1 stop bit, no parity
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_putchar(char c) {
    while (!(UCSR0A & (1 << UDRE0))); // Wait for empty transmit buffer
    UDR0 = c; // Put data into buffer, sends the data
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putchar(*s++);
    }
}

//static FILE uart_output = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

int main (void) {

    uart_init();
    //stdout = &uart_output;  

    //char msg[] = "Hello\n";

    DDRB |= _BV(DDB5);
    while(1){
        uart_puts("Hello from ATmega328P!\r\n"); // Using custom string function


        printf("hello\n");

        PORTB |= _BV(PORTB5);
        _delay_ms(DELAY_MS);
        
        PORTB &= ~_BV(PORTB5);
        _delay_ms(DELAY_MS);
    }
}