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
#include "engine.h"



/**
 * @brief  Converts a tempo control reading to a tempo.
 * @param  tempo_raw  Tempo control position, 0 to 1023.
 * @return Beats per minute, SEQ_BPM_MIN to SEQ_BPM_MIN + 255.
 */
static uint16_t tempo_bpm(uint16_t tempo_raw)
{
    return (uint16_t)(SEQ_BPM_MIN + (tempo_raw / SEQ_TEMPO_RAW_PER_BPM));
}



/**
 * @brief  Begins the step that is due, if the engine has one to play, and
 *         works out its pitch.
 * @param  p_state  Sequencer state; the engine moves on.
 * @param  p_in     Inputs for this tick.
 * @param  p_out    Outputs, already cleared; filled in if a step begins.
 */
static void begin_step(seq_state_t *p_state, const seq_inputs_t *p_in, seq_outputs_t *p_out)
{
    engine_inputs_t engine_in;
    engine_step_t   played;

    engine_in.p_raw_steps = p_in->raw_steps;
    engine_in.p_address   = &p_in->address;
    played.step           = 0U;
    played.note           = 0U;

    // The step takes its turn whether or not its note is readable
    if (engine_plain_step(&p_state->engine, &engine_in, &played))
    {
        p_out->b_step_started = true;
        p_out->step           = played.step;
        p_out->b_note_valid   = p_in->b_steps_valid;
        if (p_in->b_steps_valid)
        {
            p_out->b_rest = !note_map_semitone(&p_in->note_map, played.note,
                                               &p_out->semitone);
        }
    }
}



void seq_init(seq_state_t *p_state)
{
    if (NULL != p_state)
    {
        clock_init(&p_state->clock);
        p_state->last_tempo_raw = 0U;
        p_state->step_pulses    = SEQ_PULSES_PER_STEP; // A step is due at once
        engine_plain_init(&p_state->engine);
        gate_init(&p_state->gate);
    }
}



void seq_tick(seq_state_t *p_state, const seq_inputs_t *p_in, seq_outputs_t *p_out)
{
    if ((NULL != p_state) && (NULL != p_in) && (NULL != p_out))
    {
        uint8_t  pulses;
        uint16_t gate_pulses;

        // An invalid tempo reading leaves the last valid one in place
        if (p_in->b_tempo_valid)
        {
            p_state->last_tempo_raw = p_in->tempo_raw;
        }

        if (p_in->b_reset)
        {
            engine_plain_init(&p_state->engine);
        }

        pulses = clock_advance(&p_state->clock, tempo_bpm(p_state->last_tempo_raw),
                               p_in->elapsed_ticks);
        p_state->step_pulses = (uint16_t)(p_state->step_pulses + pulses);
        gate_pulses          = pulses;

        p_out->b_step_started = false;
        p_out->step           = 0U;
        p_out->b_note_valid   = false;
        p_out->b_rest         = false;
        p_out->semitone       = 0U;

        if (p_state->step_pulses >= SEQ_PULSES_PER_STEP)
        {
            p_state->step_pulses = (uint16_t)(p_state->step_pulses - SEQ_PULSES_PER_STEP);
            begin_step(p_state, p_in, p_out);

            // The boundary ends the last step's gate, and only a pitch opens
            // another. Its length counts from the boundary, so only the
            // pulses already into the new step come off it
            gate_begin(&p_state->gate, &p_in->gate,
                       p_out->b_note_valid && !p_out->b_rest);
            gate_pulses = p_state->step_pulses;
        }

        p_out->b_gate      = gate_advance(&p_state->gate, gate_pulses);
        p_out->b_clock_out = (p_state->step_pulses < SEQ_CLOCK_OUT_PULSES);
    }
}
