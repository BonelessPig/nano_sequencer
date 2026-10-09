/**
 * @file   seq.c
 * @brief  Sequencer core implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "seq.h"
#include <stddef.h>

#define NOTE_MASK ((uint8_t)SEQ_NOTE_MAX)



/**
 * @brief  Extracts one step's note value from the packed raw bytes.
 * @param  p_raw_steps  Packed step bits, SEQ_RAW_BYTE_COUNT bytes, MSB first.
 * @param  step         Step index, 0 to SEQ_STEP_COUNT - 1.
 * @return The note value, 0 to SEQ_NOTE_MAX.
 */
static uint8_t decode_note(const uint8_t *p_raw_steps, uint8_t step)
{
    const uint8_t bit_pos    = (uint8_t)(step * SEQ_NOTE_BITS);
    const uint8_t byte_index = (uint8_t)(bit_pos / SEQ_BITS_PER_BYTE);
    const uint8_t bit_offset = (uint8_t)(bit_pos % SEQ_BITS_PER_BYTE);
    const uint8_t shift      = (uint8_t)(SEQ_BITS_PER_BYTE - SEQ_NOTE_BITS - bit_offset);

    return (uint8_t)((uint8_t)(p_raw_steps[byte_index] >> shift) & NOTE_MASK);
}



void seq_init(seq_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->last_tempo_raw = 0U;
    }
}



void seq_tick(seq_state_t *p_state, const seq_inputs_t *p_in, seq_outputs_t *p_out)
{
    if ((NULL != p_state) && (NULL != p_in) && (NULL != p_out))
    {
        // An invalid tempo reading leaves the last valid one in place
        if (p_in->b_tempo_valid)
        {
            p_state->last_tempo_raw = p_in->tempo_raw;
        }

        p_out->b_notes_valid = p_in->b_steps_valid;
        for (uint8_t step = 0U; step < SEQ_STEP_COUNT; step++)
        {
            if (p_in->b_steps_valid)
            {
                p_out->notes[step] = decode_note(p_in->raw_steps, step);
            }
            else
            {
                p_out->notes[step] = 0U;
            }
        }

        p_out->delay_ms = (uint16_t)(p_state->last_tempo_raw / SEQ_TEMPO_RAW_PER_MS);
    }
}
