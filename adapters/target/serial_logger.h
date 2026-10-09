#ifndef SERIAL_LOGGER_H
#define SERIAL_LOGGER_H
/**
 * @file   serial_logger.h
 * @brief  Header file for serial logging functionality on AVR microcontrollers.
 *         The logging functions themselves are declared in log_port.h; this
 *         header covers what only the target needs: bring-up and log levels.
 * @author BonelessPig
 * @date   2025-12-08
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "port_status.h"

/**
 * @brief Log levels for serial logging, least to most verbose. A message is
 *        sent when its level is at or below the level given to serial_init().
 */
typedef enum
{
    LOGLVL_OFF = 0,    // No logging
    LOGLVL_FATAL,      // Critical errors that cause application termination
    LOGLVL_ERROR,      // Errors that prevent normal operation
    LOGLVL_WARN,       // Warnings, potential issues
    LOGLVL_INFO,       // General information about application flow
    LOGLVL_DEBUG,      // Detailed information for debugging
    LOGLVL_TRACE       // Very detailed, fine-grained information
} log_level_t;

/**
 * @brief Initializes the serial logger with the specified log level.
 * @param level level to set for logging
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t serial_init(log_level_t level);

#endif /* SERIAL_LOGGER_H */
