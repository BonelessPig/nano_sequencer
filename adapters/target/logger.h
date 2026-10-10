#ifndef LOGGER_H
#define LOGGER_H
/**
 * @file   logger.h
 * @brief  Bring-up and log levels for the target's logger. The logging
 *         functions themselves are declared in log_port.h. Two source files
 *         implement both headers and the build links exactly one of them:
 *         serial_logger.c (debug build, text over USART0) or null_logger.c
 *         (release build, discards everything).
 * @author BonelessPig
 * @date   2025-12-08
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "port_status.h"

/**
 * @brief Log levels, least to most verbose. A message is sent when its level
 *        is at or below the level given to logger_init().
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
 * @brief Initializes the logger with the specified log level. Call once,
 *        before any log_port.h function.
 * @param level level to set for logging; the null logger ignores it
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t logger_init(log_level_t level);

#endif /* LOGGER_H */
