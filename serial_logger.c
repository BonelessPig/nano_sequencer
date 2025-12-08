#include "serial_logger.h"

static LogLevel currentLogLevel = LOG_LEVEL_OFF; 

void serial_init(LogLevel level) {
    UBRR0H = UBRRH_VALUE; // Set baud rate high byte
    UBRR0L = UBRRL_VALUE; // Set baud rate low byte

    // Enable receiver and transmitter
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);

    // Set frame format: 8 data bits, 1 stop bit, no parity
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);

    // Sets the Log Level
    currentLogLevel = level;
}

void add_char_serial(char c) {
    while (!(UCSR0A & (1 << UDRE0))); // Wait for empty transmit buffer
    UDR0 = c; // Put data into buffer, sends the data
}

void log_serial(LogLevel level, const char *s) {
    if (level >= currentLogLevel) while (*s) add_char_serial(*s++);
}