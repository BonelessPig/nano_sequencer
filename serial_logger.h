#ifndef SERIAL_LOGGER_H 
#define SERIAL_LOGGER_H

#include <stdio.h>
#include <avr/io.h>

#define F_CPU 16000000UL // ATmega328P's clock frequency
#define BAUD 9600        // Desired baud rate

#include <util/setbaud.h>

// Define the enum for log levels
typedef enum {
    LOG_LEVEL_OFF = 0,    // No logging
    LOG_LEVEL_FATAL,      // Critical errors that cause application termination
    LOG_LEVEL_ERROR,      // Errors that prevent normal operation
    LOG_LEVEL_WARN,       // Warnings, potential issues
    LOG_LEVEL_INFO,       // General information about application flow
    LOG_LEVEL_DEBUG,      // Detailed information for debugging
    LOG_LEVEL_TRACE       // Very detailed, fine-grained information
} LogLevel;

void add_char_serial(char c);

void log_serial(LogLevel level, const char *s);

void serial_init(LogLevel logLevel);

#endif