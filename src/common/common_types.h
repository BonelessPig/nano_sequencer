#ifndef __COMMON_TYPES_H__
#define __COMMON_TYPES_H__
/**
 * @file common_types.h
 * @author BonelessPig
 * @brief common type definitions and macros for AVR microcontroller programming
 * @version 0.1
 * @date 2026-02-18
 * 
 * @copyright Copyright (c) 2026
 * 
 */

typedef enum {
    STATUS_OK         = 0, // Operation completed successfully
    ERR_GENERAL       = 1, // General error
    ERR_INVALID_PARAM = 2, // Invalid parameter provided
    ERR_BUSY          = 3, // Resource is busy
    ERR_TIMEOUT       = 4, // Operation timed out
    ERR_NOT_SUPPORTED = 5, // Operation not supported
    ERR_UNKNOWN       = 99 // Unknown error
} Status;


#endif