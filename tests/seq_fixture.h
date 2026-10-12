#ifndef SEQ_FIXTURE_H
#define SEQ_FIXTURE_H
/**
 * @file   seq_fixture.h
 * @brief  What the test_seq programs share: the inputs they start from and
 *         the helpers that tick the sequencer.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "seq.h"
#include "test_harness.h"

// Fills every byte of an object so tests can prove a field really was written
#define POISON_BYTE (0xA5)

#define TICK_LIMIT (2000U) // More ticks than any step lasts (500 at the slowest tempo)

// Tempo readings that give a whole number of ticks per step (15000 / BPM)
#define TEMPO_RAW_30_BPM  (0U)    // 500 ticks per step
#define TEMPO_RAW_60_BPM  (120U)  // 250 ticks per step
#define TEMPO_RAW_120_BPM (360U)  // 125 ticks per step
#define TEMPO_RAW_150_BPM (480U)  // 100 ticks per step
#define TEMPO_RAW_250_BPM (880U)  // 60 ticks per step
#define TEMPO_RAW_MAX     (1023U) // 285 BPM: 19 steps every 1000 ticks

#define GATE_HALF_STEP (12U) // Gate length the tests start from, in pulses

#define CV_PER_OCTAVE (1000U) // Spacing of the calibration table the tests start from

/**
 * @brief Inputs with everything valid, all step bits 0, the tempo control at
 *        0 and one tick elapsed. The pattern is all sixteen steps, forward
 *        and looping, in the chromatic scale from the lowest pitch, so a
 *        step's pitch is its note value less one. A gate lasts half a step,
 *        and the calibration table is evenly spaced, CV_PER_OCTAVE apart.
 */
static inline seq_inputs_t make_valid_inputs(void)
{
    seq_inputs_t in;

    (void)memset(&in, 0, sizeof(in));
    in.b_steps_valid      = true;
    in.b_tempo_valid      = true;
    in.elapsed_ticks      = 1U;
    in.address.direction  = ADDRESS_FORWARD;
    in.address.first_step = 0U;
    in.address.last_step  = (uint8_t)(PANEL_STEP_COUNT - 1U);
    in.address.b_one_shot = false;
    in.note_map.scale     = NOTE_MAP_SCALE_CHROMATIC;
    in.note_map.root      = 0U;
    in.gate.length        = GATE_HALF_STEP;
    in.b_reset            = false;
    for (uint8_t point = 0U; point < PITCH_CAL_POINT_COUNT; point++)
    {
        in.pitch_cal.cv[point] = (uint16_t)(point * CV_PER_OCTAVE);
    }
    return in;
}



/**
 * @brief Inputs with everything valid and step n holding the note value n:
 *        step 0 is a rest and step n plays the pitch n - 1.
 */
static inline seq_inputs_t make_counting_inputs(void)
{
    seq_inputs_t in = make_valid_inputs();

    for (uint8_t byte_index = 0U; byte_index < PANEL_STEP_BYTE_COUNT; byte_index++)
    {
        const uint8_t even_step = (uint8_t)(byte_index * 2U);
        in.raw_steps[byte_index] = (uint8_t)((uint8_t)(even_step << PANEL_VALUE_BITS)
                                             | (uint8_t)(even_step + 1U));
    }
    return in;
}



/**
 * @brief  Ticks the sequencer until a step begins.
 * @return How many ticks that took, counting the one the step began on, or
 *         0 if none began within TICK_LIMIT ticks.
 */
static inline unsigned int ticks_to_next_step(seq_state_t *p_state, const seq_inputs_t *p_in,
                                              seq_outputs_t *p_out)
{
    unsigned int ticks = 0U;

    for (unsigned int tick = 1U; (0U == ticks) && (tick <= TICK_LIMIT); tick++)
    {
        seq_tick(p_state, p_in, p_out);
        if (p_out->b_step_started)
        {
            ticks = tick;
        }
    }
    return ticks;
}



/**
 * @brief What an output did over a run of ticks.
 */
typedef struct
{
    unsigned int high_ticks; // Ticks it was high after
    unsigned int rises;      // Times it went from low to high
    bool         b_level;    // Its level after the last tick
} level_trace_t;

/**
 * @brief Notes one tick's level of an output in its trace.
 */
static inline void trace_level(level_trace_t *p_trace, bool b_level)
{
    p_trace->high_ticks += b_level ? 1U : 0U;
    p_trace->rises      += (b_level && !p_trace->b_level) ? 1U : 0U;
    p_trace->b_level     = b_level;
}



/**
 * @brief Ticks the sequencer a number of times and follows the gate and the
 *        clock output. A trace carries on from where an earlier run left it.
 */
static inline void run_and_trace(seq_state_t *p_state, const seq_inputs_t *p_in, unsigned int ticks,
                                 level_trace_t *p_gate, level_trace_t *p_clock_out)
{
    for (unsigned int tick = 0U; tick < ticks; tick++)
    {
        seq_outputs_t out;

        seq_tick(p_state, p_in, &out);
        trace_level(p_gate, out.b_gate);
        trace_level(p_clock_out, out.b_clock_out);
    }
}



#define SETTLE_TICKS (20U) // Debounce time of the run button

// Tempo readings that are a ratio on the external clock
#define TEMPO_RAW_DIVIDE_4   (0U)    // A step to every four edges
#define TEMPO_RAW_ONE_TO_ONE (500U)  // A step to an edge
#define TEMPO_RAW_MULTIPLY_4 (1023U) // Four steps to an edge

/**
 * @brief Runs single ticks and returns the steps that began. The outputs
 *        are those of the last tick.
 */
static inline unsigned int run_ticks(seq_state_t *p_state, const seq_inputs_t *p_in,
                                     seq_outputs_t *p_out, unsigned int ticks)
{
    unsigned int steps = 0U;

    for (unsigned int tick = 0U; tick < ticks; tick++)
    {
        seq_tick(p_state, p_in, p_out);
        steps += p_out->b_step_started ? 1U : 0U;
    }
    return steps;
}

/**
 * @brief Makes the external clock the one the next start is on, as an edge
 *        that came while the pattern was stopped does. Called straight
 *        after seq_init(), it has the first tick start there, so that
 *        nothing begins until an edge comes.
 */
static inline void wait_for_external(seq_state_t *p_state)
{
    p_state->transport.b_external = true;
}

/**
 * @brief One tick with an edge of the external clock on it.
 */
static inline void edge_tick(seq_state_t *p_state, seq_inputs_t *p_in, seq_outputs_t *p_out)
{
    p_in->b_clock_edge = true;
    seq_tick(p_state, p_in, p_out);
    p_in->b_clock_edge = false;
}

/**
 * @brief Holds the run button down until the press counts, which is on the
 *        last tick, and lets go without running a tick. The outputs are
 *        those of the tick the press took effect on.
 */
static inline void press_run(seq_state_t *p_state, seq_inputs_t *p_in, seq_outputs_t *p_out)
{
    p_in->b_run_button = true;
    (void)run_ticks(p_state, p_in, p_out, SETTLE_TICKS);
    p_in->b_run_button = false;
}

#endif /* SEQ_FIXTURE_H */
