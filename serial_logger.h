#ifndef SERIAL_LOGGER_H 
#define SERIAL_LOGGER_H
/**
 * @file   serial_logger.h
 * @brief  Header file for serial logging functionality on AVR microcontrollers.
 * @author BonelessPig
 * @date   2025-12-08
 */

#include <stdio.h> // For FILE type
#include <avr/io.h> // For AVR IO registers

// Define CPU frequency and baud rate for serial communication
#define F_CPU 16000000UL // 16 MHz clock speed
#define BAUD 9600 // Desired baud rate        

#include <util/setbaud.h> // For UBRR calculation

/**
 * @brief Log levels for serial logging
 */
typedef enum {
    LOGLVL_OFF = 0,    // No logging
    LOGLVL_FATAL,      // Critical errors that cause application termination
    LOGLVL_ERROR,      // Errors that prevent normal operation
    LOGLVL_WARN,       // Warnings, potential issues
    LOGLVL_INFO,       // General information about application flow
    LOGLVL_DEBUG,      // Detailed information for debugging
    LOGLVL_TRACE       // Very detailed, fine-grained information
} LogLevel;

/**
 * @brief Adds a character to the serial output.
 * @param c character to add
 */
void add_char_serial(char c);

/**
 * @brief Logs a string to the serial output if the log level is appropriate.
 * @param level LogLevel of the message
 * @param s string to log
 */
void log_serial(LogLevel level, const char *s);

/**
 * @brief Initializes the serial logger with the specified log level.
 * @param level level to set for logging
 */
void serial_init(LogLevel level);

#endif