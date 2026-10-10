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



/**
 * @brief  Gives the step that follows the given one, wrapping after the last.
 * @param  step  Step index, 0 to SEQ_STEP_COUNT - 1.
 * @return The next step index, 0 to SEQ_STEP_COUNT - 1.
 */
static uint8_t next_step(uint8_t step)
{
    return (uint8_t)((step + 1U) % SEQ_STEP_COUNT);
}



/**
 * @brief  Converts a tempo control reading to a tempo.
 * @param  tempo_raw  Tempo control position, 0 to 1023.
 * @return Beats per minute, SEQ_BPM_MIN to SEQ_BPM_MIN + 255.
 */
static uint16_t tempo_bpm(uint16_t tempo_raw)
{
    return (uint16_t)(SEQ_BPM_MIN + (tempo_raw / SEQ_TEMPO_RAW_PER_BPM));
}



void seq_init(seq_state_t *p_state)
{
    if (NULL != p_state)
    {
        clock_init(&p_state->clock);
        p_state->last_tempo_raw = 0U;
        p_state->step_pulses    = SEQ_PULSES_PER_STEP; // A step is due at once
        p_state->current_step   = 0U;
    }
}



void seq_tick(seq_state_t *p_state, const seq_inputs_t *p_in, seq_outputs_t *p_out)
{
    if ((NULL != p_state) && (NULL != p_in) && (NULL != p_out))
    {
        uint8_t pulses;

        // An invalid tempo reading leaves the last valid one in place
        if (p_in->b_tempo_valid)
        {
            p_state->last_tempo_raw = p_in->tempo_raw;
        }

        pulses = clock_advance(&p_state->clock, tempo_bpm(p_state->last_tempo_raw),
                               p_in->elapsed_ticks);
        p_state->step_pulses = (uint16_t)(p_state->step_pulses + pulses);

        p_out->b_step_started = false;
        p_out->step           = 0U;
        p_out->note           = 0U;
        p_out->b_note_valid   = false;

        // Begin the next step when it is due, whether or not its note is readable
        if (p_state->step_pulses >= SEQ_PULSES_PER_STEP)
        {
            p_state->step_pulses = (uint16_t)(p_state->step_pulses - SEQ_PULSES_PER_STEP);

            p_out->b_step_started = true;
            p_out->step           = p_state->current_step;
            p_out->b_note_valid   = p_in->b_steps_valid;
            if (p_in->b_steps_valid)
            {
                p_out->note = decode_note(p_in->raw_steps, p_state->current_step);
            }
            p_state->current_step = next_step(p_state->current_step);
        }
    }
}
