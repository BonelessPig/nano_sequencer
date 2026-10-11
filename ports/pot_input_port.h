#ifndef POT_INPUT_PORT_H
#define POT_INPUT_PORT_H
/**
 * @file   pot_input_port.h
 * @brief  Port: raw position of the panel's pot.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>
#include "port_status.h"

/**
 * @brief  Reads the pot once.
 * @param  p_raw  Where the reading is stored. Must not be null. The value is
 *                a unitless 10-bit position, 0 to 1023 (0 V to the reference
 *                voltage on the target). The core decides what it sets: the
 *                tempo, or the trim in calibration mode.
 * @return STATUS_OK on success; ERR_INVALID_PARAM for a null pointer;
 *         ERR_TIMEOUT if the conversion did not finish. On any error *p_raw
 *         is not written.
 * @note   Not ISR-safe. Blocks for one conversion (about 0.1 ms on the
 *         target, bounded at a few milliseconds).
 */
port_status_t pot_input_read(uint16_t *p_raw);

#endif /* POT_INPUT_PORT_H */
