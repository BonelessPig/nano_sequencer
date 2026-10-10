#ifndef STEP_INPUT_PORT_H
#define STEP_INPUT_PORT_H
/**
 * @file   step_input_port.h
 * @brief  Port: raw step-note bits as set on the front panel.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>
#include "port_status.h"

/**
 * @brief  Takes one snapshot of the step-note inputs and returns it as raw
 *         bytes. The core decodes the bytes into per-step note values.
 * @param  p_raw_bits  Destination buffer of at least byte_count bytes. Must
 *                     not be null. Byte 0 is the first byte read (on the
 *                     target, the shift register nearest the MCU), and within
 *                     each byte bit 7 is the first bit read.
 * @param  byte_count  Number of bytes to read, 1 to 8. One byte is 8 input
 *                     bits (two 4-bit steps).
 * @return STATUS_OK on success; ERR_INVALID_PARAM for a null buffer or a
 *         byte_count outside 1 to 8; ERR_TIMEOUT if the hardware did not
 *         answer. The buffer is only written on success.
 * @note   Not ISR-safe. Blocks for the duration of the read (well under a
 *         millisecond on the target).
 */
port_status_t step_input_read(uint8_t *p_raw_bits, uint8_t byte_count);

#endif /* STEP_INPUT_PORT_H */
