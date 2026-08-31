#ifndef __SERIAL_LOGGER_H__
#define __SERIAL_LOGGER_H__
/**
 * @file   serial_logger.h
 * @brief  Header file for serial logging functionality on AVR microcontrollers.
 * @author BonelessPig
 * @date   2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */


// F_CPU is provided by the build (see Makefile / -DF_CPU) rather than defined
// here, so there is a single source of truth for the clock speed.
#ifndef F_CPU
#error "F_CPU must be defined by the build (e.g. -DF_CPU=16000000UL)"
#endif

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
 * @param format format string (like printf)
 * @param ... additional arguments for the format string
 */
void log_serial(LogLevel level, const char *s, ...);

/**
 * @brief Initializes the serial logger with the specified log level.
 * @param level level to set for logging
 * @return int status code (0 for success)
 */
int serial_init(LogLevel level);

#endif