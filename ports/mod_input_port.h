#ifndef MOD_INPUT_PORT_H
#define MOD_INPUT_PORT_H
/**
 * @file   mod_input_port.h
 * @brief  Port: the modulation input, a slow signal from outside (an LFO)
 *         that swings either side of a centre.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>
#include "port_status.h"

/**
 * @brief  Reads the modulation input once.
 * @param  p_raw  Where the reading is stored. Must not be null. The value is
 *                a unitless 10-bit number, 0 to 1023, with 512 for a signal
 *                at its centre, which is also what an input with nothing
 *                plugged into it reads. The core decides what it moves.
 * @return STATUS_OK on success; ERR_INVALID_PARAM for a null pointer;
 *         ERR_TIMEOUT if the conversion did not finish. On any error *p_raw
 *         is not written.
 * @note   Not ISR-safe. Blocks for one conversion (about 0.1 ms on the
 *         target, bounded at a few milliseconds).
 */
port_status_t mod_input_read(uint16_t *p_raw);

#endif /* MOD_INPUT_PORT_H */
