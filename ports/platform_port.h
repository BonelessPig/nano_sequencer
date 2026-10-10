#ifndef PLATFORM_PORT_H
#define PLATFORM_PORT_H
/**
 * @file   platform_port.h
 * @brief  Port: one-time bring-up of whatever the other ports depend on.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "port_status.h"

/**
 * @brief  Prepares the platform so every other port can be used: on the
 *         target this sets up the logger, pin directions, the ADC and the
 *         timebase. Call once, before any other port function.
 * @return STATUS_OK on success, otherwise the status of the step that failed.
 * @note   Not ISR-safe. Call from the main context only.
 */
port_status_t platform_init(void);

#endif /* PLATFORM_PORT_H */
