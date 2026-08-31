/**
 * @file   shift_reg_reader.c
 * @brief  Implementation of 74HC165 shift register chain reading for step notes.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "shift_reg_reader.h"
#include "../mcu/atmega328p_regs.h"
#include "../common/common_types.h"
#include "../common/bits.h"

_Static_assert(8 % NOTE_BITS_PER_STEP == 0, "NOTE_BITS_PER_STEP must divide 8");

#define SHIFT_LOAD_BIT BIT_2 // PD2: SH/LD (active-low load pulse)
#define SHIFT_CLK_BIT  BIT_3 // PD3: CLK (shift clock)
#define SHIFT_DATA_BIT BIT_4 // PD4: SER data-in (from QH of the chip nearest the MCU)



/**
 * @brief Performs one parallel-load + serial-clock-out cycle across the 74HC165
 *        daisy chain and stores the raw bits read.
 * @param raw_bits pointer to a buffer of at least chain_bytes bytes
 * @param chain_bytes number of bytes to read (i.e. number of chained 74HC165s)
 * @return int status code (0 for success)
 */
int read_shift_reg_chain(unsigned char *raw_bits, unsigned char chain_bytes) {
    if (raw_bits == 0) return ERR_INVALID_PARAM;
    if (chain_bytes == 0 || chain_bytes > SHIFT_REG_CHAIN_BYTES) return ERR_INVALID_PARAM;

    PORTD &= ~SHIFT_LOAD_BIT; // Latch parallel inputs
    PORTD |= SHIFT_LOAD_BIT;  // Return to shift mode

    for (unsigned char byte_i = 0; byte_i < chain_bytes; byte_i++) {
        unsigned char b = 0;
        for (unsigned char bit_i = 0; bit_i < 8; bit_i++) {
            b <<= 1;
            if (PIND & SHIFT_DATA_BIT) b |= 1; // Read current bit before clocking to the next
            PORTD |= SHIFT_CLK_BIT;
            PORTD &= ~SHIFT_CLK_BIT;
        }
        raw_bits[byte_i] = b;
    }
    return STATUS_OK;
}



/**
 * @brief Extracts one step's NOTE_BITS_PER_STEP-wide note value from a raw bit buffer.
 * @param note pointer to where the extracted note value is stored
 * @param raw_bits the buffer previously filled by read_shift_reg_chain()
 * @param raw_bits_len number of valid bytes in raw_bits
 * @param step_index which step's note to extract (0-based)
 * @return int status code (0 for success)
 */
int get_step_note(unsigned char *note, const unsigned char *raw_bits,
                   unsigned char raw_bits_len, unsigned char step_index) {
    if (note == 0 || raw_bits == 0) return ERR_INVALID_PARAM;

    unsigned char bit_pos    = step_index * NOTE_BITS_PER_STEP;
    unsigned char byte_index = bit_pos / 8;
    if (byte_index >= raw_bits_len) return ERR_INVALID_PARAM;

    unsigned char bit_offset = bit_pos % 8;
    unsigned char mask       = (1 << NOTE_BITS_PER_STEP) - 1;
    *note = (raw_bits[byte_index] >> (8 - NOTE_BITS_PER_STEP - bit_offset)) & mask;
    return STATUS_OK;
}



/**
 * @brief Reads the entire shift register chain in one transaction and extracts
 *        all step note values.
 * @param notes pointer to a buffer of at least step_count bytes
 * @param step_count number of steps to extract
 * @return int status code (0 for success)
 */
int read_step_notes(unsigned char *notes, unsigned char step_count) {
    if (notes == 0) return ERR_INVALID_PARAM;

    unsigned char needed_bytes = (step_count * NOTE_BITS_PER_STEP + 7) / 8;
    if (needed_bytes > SHIFT_REG_CHAIN_BYTES) return ERR_INVALID_PARAM;

    unsigned char raw_bits[SHIFT_REG_CHAIN_BYTES];
    int status = read_shift_reg_chain(raw_bits, needed_bytes);
    if (status != STATUS_OK) return status;

    for (unsigned char i = 0; i < step_count; i++) {
        status = get_step_note(&notes[i], raw_bits, needed_bytes, i);
        if (status != STATUS_OK) return status;
    }
    return STATUS_OK;
}
