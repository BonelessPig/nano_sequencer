#ifndef PORT_STATUS_H
#define PORT_STATUS_H
/**
 * @file   port_status.h
 * @brief  Status codes returned by port functions that can fail.
 * @author BonelessPig
 * @date   2026-02-18
 *
 * @copyright Copyright (c) 2026
 *
 */

/**
 * @brief Result of a port call. The numeric values appear in serial log
 *        output, so they must not be renumbered.
 */
typedef enum
{
    STATUS_OK         = 0, // Operation completed successfully
    ERR_GENERAL       = 1, // General error
    ERR_INVALID_PARAM = 2, // Invalid parameter provided
    ERR_BUSY          = 3, // Resource is busy
    ERR_TIMEOUT       = 4, // Operation timed out
    ERR_NOT_SUPPORTED = 5, // Operation not supported
    ERR_UNKNOWN       = 99 // Unknown error
} port_status_t;

#endif /* PORT_STATUS_H */
