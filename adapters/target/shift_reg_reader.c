/**
 * @file   shift_reg_reader.c
 * @brief  Target implementation of the step input port: reads the 74HC165
 *         shift register chain that holds the step notes, over the SPI bus.
 * @author BonelessPig
 *
 * The chain's clock is the bus clock (PB5, SCK) and its output, QH of the
 * chip nearest the MCU, is the bus data input (PB4, MISO). Its load line is
 * an ordinary output, PB1. A 74HC165 shows its first bit as soon as it is
 * loaded and moves to the next on each rising clock edge, which is the edge
 * SPI mode 0 samples on, so each transfer reads eight bits in order.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "step_input_port.h"
#include <stddef.h>
#include "atmega328p_regs.h"
#include "bits.h"
#include "port_status.h"
#include "spi.h"

#define SHIFT_REG_CHAIN_BYTES (8U) // Max daisy-chained 74HC165s supported (8 x 8 bits = 64 bits)

#define SHIFT_LOAD_BIT BIT_1 // PB1: SH/LD (active-low load pulse)

#define SHIFT_FILL_BYTE (0U) // Sent while reading; nothing on the bus listens



/**
 * @brief Performs one parallel-load + serial-clock-out cycle across the 74HC165
 *        daisy chain and stores the raw bits read (see step_input_port.h).
 * @param p_raw_bits pointer to a buffer of at least byte_count bytes. p_raw_bits[0]
 *                   holds the first 8 bits clocked out (the chip nearest the MCU,
 *                   whose QH feeds the MCU's data-in pin); p_raw_bits[byte_count-1]
 *                   holds the chip farthest from the MCU (SER tied low).
 * @param byte_count number of bytes to read (i.e. number of chained 74HC165s);
 *                   must be between 1 and SHIFT_REG_CHAIN_BYTES
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t step_input_read(uint8_t *p_raw_bits, uint8_t byte_count)
{
    port_status_t status = STATUS_OK;

    if ((NULL == p_raw_bits) || (0U == byte_count) || (byte_count > SHIFT_REG_CHAIN_BYTES))
    {
        status = ERR_INVALID_PARAM;
    }
    else
    {
        // Read into a buffer of our own, so that a read that fails part way
        // leaves the caller's untouched
        uint8_t bytes[SHIFT_REG_CHAIN_BYTES];
        uint8_t count = 0U;

        PORTB &= (uint8_t)~SHIFT_LOAD_BIT; // Latch parallel inputs
        PORTB |= SHIFT_LOAD_BIT;  // Return to shift mode

        while ((STATUS_OK == status) && (count < byte_count))
        {
            status = spi_transfer(SHIFT_FILL_BYTE, &bytes[count]);
            count++;
        }

        if (STATUS_OK == status)
        {
            for (uint8_t i = 0U; i < byte_count; i++)
            {
                p_raw_bits[i] = bytes[i];
            }
        }
    }

    return status;
}
