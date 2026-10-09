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
#include "atmega328p_regs.h"
#include "port_status.h"
#include "bits.h"

#define SHIFT_REG_CHAIN_BYTES 8 // Max daisy-chained 74HC165s supported (8 x 8 bits = 64 bits)

#define SHIFT_LOAD_BIT BIT_2 // PD2: SH/LD (active-low load pulse)
#define SHIFT_CLK_BIT  BIT_3 // PD3: CLK (shift clock)
#define SHIFT_DATA_BIT BIT_4 // PD4: SER data-in (from QH of the chip nearest the MCU)



/**
 * @brief Performs one parallel-load + serial-clock-out cycle across the 74HC165
 *        daisy chain and stores the raw bits read (see step_input_port.h).
 * @param raw_bits pointer to a buffer of at least chain_bytes bytes. raw_bits[0]
 *                 holds the first 8 bits clocked out (the chip nearest the MCU,
 *                 whose QH feeds the MCU's data-in pin); raw_bits[chain_bytes-1]
 *                 holds the chip farthest from the MCU (SER tied low).
 * @param chain_bytes number of bytes to read (i.e. number of chained 74HC165s);
 *                     must be between 1 and SHIFT_REG_CHAIN_BYTES
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t step_input_read(uint8_t *raw_bits, uint8_t chain_bytes) {
    if (raw_bits == 0) return ERR_INVALID_PARAM;
    if (chain_bytes == 0 || chain_bytes > SHIFT_REG_CHAIN_BYTES) return ERR_INVALID_PARAM;

    PORTD &= (unsigned char)~SHIFT_LOAD_BIT; // Latch parallel inputs
    PORTD |= SHIFT_LOAD_BIT;  // Return to shift mode

    for (unsigned char byte_i = 0; byte_i < chain_bytes; byte_i++) {
        unsigned char b = 0;
        for (unsigned char bit_i = 0; bit_i < 8; bit_i++) {
            b <<= 1;
            if (PIND & SHIFT_DATA_BIT) b |= 1; // Read current bit before clocking to the next
            PORTD |= SHIFT_CLK_BIT;
            PORTD &= (unsigned char)~SHIFT_CLK_BIT;
        }
        raw_bits[byte_i] = b;
    }
    return STATUS_OK;
}

