#ifndef TEMPO_INPUT_PORT_H
#define TEMPO_INPUT_PORT_H
/**
 * @file   tempo_input_port.h
 * @brief  Port: raw position of the tempo control.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>
#include "port_status.h"

/**
 * @brief  Reads the tempo control once.
 * @param  p_raw  Where the reading is stored. Must not be null. The value is
 *                a unitless 10-bit position, 0 to 1023 (0 V to the reference
 *                voltage on the target). The core turns it into a delay.
 * @return STATUS_OK on success; ERR_INVALID_PARAM for a null pointer;
 *         ERR_TIMEOUT if the conversion did not finish. On any error *p_raw
 *         is not written.
 * @note   Not ISR-safe. Blocks for one conversion (about 0.1 ms on the
 *         target, bounded at a few milliseconds).
 */
port_status_t tempo_input_read(uint16_t *p_raw);

#endif /* TEMPO_INPUT_PORT_H */
