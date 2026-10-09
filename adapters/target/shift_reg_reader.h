#ifndef __SHIFT_REG_READER_H__
#define __SHIFT_REG_READER_H__
/**
 * @file   shift_reg_reader.h
 * @brief  Reads step note values from a daisy-chained 74HC165 shift register bank.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */

#define SHIFT_REG_CHAIN_BYTES 8 // Max daisy-chained 74HC165s supported (8 x 8 bits = 64 bits)
#define NOTE_BITS_PER_STEP    4 // Bits per step's note value; must evenly divide 8 (1, 2, 4, or 8)

/**
 * @brief Performs one parallel-load + serial-clock-out cycle across the 74HC165
 *        daisy chain and stores the raw bits read.
 * @param raw_bits pointer to a buffer of at least chain_bytes bytes. raw_bits[0]
 *                 holds the first 8 bits clocked out (the chip nearest the MCU,
 *                 whose QH feeds the MCU's data-in pin); raw_bits[chain_bytes-1]
 *                 holds the chip farthest from the MCU (SER tied low).
 * @param chain_bytes number of bytes to read (i.e. number of chained 74HC165s);
 *                     must be between 1 and SHIFT_REG_CHAIN_BYTES
 * @return int status code (0 for success)
 */
int read_shift_reg_chain(unsigned char *raw_bits, unsigned char chain_bytes);

/**
 * @brief Extracts one step's NOTE_BITS_PER_STEP-wide note value from a raw bit
 *        buffer previously filled by read_shift_reg_chain(). Note fields are
 *        assumed byte-aligned (NOTE_BITS_PER_STEP divides 8).
 * @param note pointer to where the extracted note value is stored
 * @param raw_bits the buffer previously filled by read_shift_reg_chain()
 * @param raw_bits_len number of valid bytes in raw_bits (for bounds checking)
 * @param step_index which step's note to extract (0-based)
 * @return int status code (0 for success)
 */
int get_step_note(unsigned char *note, const unsigned char *raw_bits,
                   unsigned char raw_bits_len, unsigned char step_index);

/**
 * @brief Reads the entire shift register chain in one transaction and extracts
 *        all step note values.
 * @param notes pointer to a buffer of at least step_count bytes
 * @param step_count number of steps to extract
 * @return int status code (0 for success)
 */
int read_step_notes(unsigned char *notes, unsigned char step_count);

#endif
