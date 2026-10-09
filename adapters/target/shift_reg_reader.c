/**
 * @file   shift_reg_reader.c
 * @brief  Target implementation of the step input port: reads the 74HC165
 *         shift register chain that holds the step notes.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "step_input_port.h"
#include <stddef.h>
#include "atmega328p_regs.h"
#include "port_status.h"
#include "bits.h"

#define SHIFT_REG_CHAIN_BYTES (8U) // Max daisy-chained 74HC165s supported (8 x 8 bits = 64 bits)
#define BITS_PER_BYTE         (8U)

#define SHIFT_LOAD_BIT BIT_2 // PD2: SH/LD (active-low load pulse)
#define SHIFT_CLK_BIT  BIT_3 // PD3: CLK (shift clock)
#define SHIFT_DATA_BIT BIT_4 // PD4: SER data-in (from QH of the chip nearest the MCU)



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
        PORTD &= (uint8_t)~SHIFT_LOAD_BIT; // Latch parallel inputs
        PORTD |= SHIFT_LOAD_BIT;  // Return to shift mode

        for (uint8_t byte_i = 0U; byte_i < byte_count; byte_i++)
        {
            uint8_t bits = 0U;
            for (uint8_t bit_i = 0U; bit_i < BITS_PER_BYTE; bit_i++)
            {
                bits = (uint8_t)(bits << 1U);
                if (0U != (PIND & SHIFT_DATA_BIT)) // Read current bit before clocking to the next
                {
                    bits |= 1U;
                }
                PORTD |= SHIFT_CLK_BIT;
                PORTD &= (uint8_t)~SHIFT_CLK_BIT;
            }
            p_raw_bits[byte_i] = bits;
        }
    }

    return status;
}
