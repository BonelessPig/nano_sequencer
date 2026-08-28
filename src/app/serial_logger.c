/**
 * @file   serial_logger.c
 * @brief  Implementation of serial logging functionality on AVR microcontrollers.
 * @author BonelessPig
 * @date   2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "serial_logger.h"
#include "../common/common_types.h"

static LogLevel currentLogLevel = LOGLVL_OFF; // Default log level


/**
 * @brief Initializes the serial logger with the specified log level.
 * @param level level to set for logging
 * @return int status code (0 for success)
 */
int serial_init(LogLevel level) {
    UBRR0H = UBRRH_VALUE; // Set baud rate high byte
    UBRR0L = UBRRL_VALUE; // Set baud rate low byte

    UCSR0B = (1 << RXEN0) | (1 << TXEN0); // Enable receiver and transmitter

    currentLogLevel = level; // Sets the Log Level
    return STATUS_OK; // Return success
}



/**
 * @brief Adds a character to the serial output.
 * @param c character to add
 */
void add_char_serial(char c) {
    while (!(UCSR0A & (1 << UDRE0))); // Wait for empty transmit buffer
    UDR0 = c; // Put data into buffer, sends the data
}



/**
 * @brief Logs a string to the serial output if the log level is appropriate.
 * @param level LogLevel of the message
 * @param format format string (like printf)
 * @param ... additional arguments for the format string
 */
void log_serial(LogLevel level, const char *format, ...) {
    
    if (level < currentLogLevel) return; // Skip logging if level is too low

    static char buffer[64]; // Buffer for formatted output

    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    const char* s = buffer;
    while (*s) add_char_serial(*s++);

    if (len >= sizeof(buffer) - 1) {
        // If the message was truncated, indicate this in the output
        add_char_serial('\n');
        add_char_serial('[');
        add_char_serial('T');
        add_char_serial('R');
        add_char_serial('U');
        add_char_serial('N');
        add_char_serial('C');
        add_char_serial('A');
        add_char_serial('T');
        add_char_serial('E');
        add_char_serial('D');
        add_char_serial(']');
    }
}
