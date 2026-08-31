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

#include "../common/bits.h"
#include "../mcu/atmega328p_regs.h"

/**
 * @brief Initializes the necessary registers for the microcontroller.
 * @return int status code (0 for success)
 */
int register_init(void);

#endif