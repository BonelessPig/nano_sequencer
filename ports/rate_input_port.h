#ifndef RATE_INPUT_PORT_H
#define RATE_INPUT_PORT_H
/**
 * @file   rate_input_port.h
 * @brief  Port: raw position of the rate pot, which sets how fast the
 *         sequencer's own LFO goes.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>
#include "port_status.h"

/**
 * @brief  Reads the rate pot once.
 * @param  p_raw  Where the reading is stored. Must not be null. The value is
 *                a unitless 10-bit position, 0 to 1023 (0 V to the reference
 *                voltage on the target). The core decides what rate it is;
 *                at the bottom of its travel the LFO is off.
 * @return STATUS_OK on success; ERR_INVALID_PARAM for a null pointer;
 *         ERR_TIMEOUT if the conversion did not finish. On any error *p_raw
 *         is not written.
 * @note   Not ISR-safe. Blocks for one conversion (about 0.1 ms on the
 *         target, bounded at a few milliseconds).
 */
port_status_t rate_input_read(uint16_t *p_raw);

#endif /* RATE_INPUT_PORT_H */
