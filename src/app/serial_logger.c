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
 * @param s string to log
 */
void log_serial(LogLevel level, const char *s) {
    if (level >= currentLogLevel) while (*s) add_char_serial(*s++);
}
