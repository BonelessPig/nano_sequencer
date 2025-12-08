#ifndef SERIAL_LOGGER_H 
#define SERIAL_LOGGER_H

#include <stdio.h>
#include <avr/io.h>

#define F_CPU 16000000UL // ATmega328P's clock frequency
#define BAUD 9600        // baud rate

#include <util/setbaud.h>

// Define the enum for log levels
typedef enum {
    LOGLVL_OFF = 0,    // No logging
    LOGLVL_FATAL,      // Critical errors that cause application termination
    LOGLVL_ERROR,      // Errors that prevent normal operation
    LOGLVL_WARN,       // Warnings, potential issues
    LOGLVL_INFO,       // General information about application flow
    LOGLVL_DEBUG,      // Detailed information for debugging
    LOGLVL_TRACE       // Very detailed, fine-grained information
} LogLevel;

void add_char_serial(char c);

void log_serial(LogLevel level, const char *s);

void serial_init(LogLevel level);

#endif