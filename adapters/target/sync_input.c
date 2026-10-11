/**
 * @file   sync_input.c
 * @brief  Target implementation of the sync switch port: a toggle switch
 *         between PD3 (D3 on the Nano) and ground. register_init() makes the
 *         pin an input with its pull-up on, so it reads low while the switch
 *         is on.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "sync_input_port.h"
#include <stdbool.h>
#include <stdint.h>
#include "atmega328p_regs.h"
#include "bits.h"

#define SYNC_BIT BIT_3 // PD3: the sync switch, low while on



/**
 * @brief  Reads the sync switch as it is now (see sync_input_port.h).
 * @return true while the pin is low
 */
bool sync_input_read(void)
{
    return (0U == (PIND & SYNC_BIT));
}
