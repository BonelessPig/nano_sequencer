#ifndef __REGISTER_INIT_H__
#define __REGISTER_INIT_H__
/**
 * @file   register_init.h
 * @brief  Header file for register initialization on AVR microcontrollers.
 * @author BonelessPig
 * @date   2025-12-08
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "bits.h"
#include "port_status.h"
#include "atmega328p_regs.h"

/**
 * @brief Initializes the necessary registers for the microcontroller.
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t register_init(void);

#endif