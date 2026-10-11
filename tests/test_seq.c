/**
 * @file   test_seq.c
 * @brief  Host unit tests for the sequencer core (core/seq.c).
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
static seq_inputs_t make_valid_inputs(void)
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
static seq_inputs_t make_counting_inputs(void)
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
static unsigned int ticks_to_next_step(seq_state_t *p_state, const seq_inputs_t *p_in,
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
static void trace_level(level_trace_t *p_trace, bool b_level)
{
    p_trace->high_ticks += b_level ? 1U : 0U;
    p_trace->rises      += (b_level && !p_trace->b_level) ? 1U : 0U;
    p_trace->b_level     = b_level;
}



/**
 * @brief Ticks the sequencer a number of times and follows the gate and the
 *        clock output. A trace carries on from where an earlier run left it.
 */
static void run_and_trace(seq_state_t *p_state, const seq_inputs_t *p_in, unsigned int ticks,
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



static void test_a_step_is_a_sixteenth_note(void)
{
    TEST_ASSERT_EQUAL(24, SEQ_PULSES_PER_STEP);
    TEST_ASSERT_EQUAL(12, SEQ_CLOCK_OUT_PULSES);
}



static void test_init_sets_the_power_on_state(void)
{
    seq_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    seq_init(&state);
    TEST_ASSERT_EQUAL(0, state.last_tempo_raw);
    TEST_ASSERT_EQUAL(0, state.plain.address.position);
    TEST_ASSERT(!state.plain.address.b_finished);
    TEST_ASSERT_EQUAL(0, state.engine); // The plain engine
    TEST_ASSERT_EQUAL(0, state.stage.address.position);
    TEST_ASSERT_EQUAL(0, state.stage.length);
    TEST_ASSERT_EQUAL(0, state.stage.played);
    TEST_ASSERT(!state.klee.b_loaded);
    TEST_ASSERT_EQUAL(0, state.chance.address.position);
    TEST_ASSERT_EQUAL(0, state.cartesian.x_steps);
    TEST_ASSERT_EQUAL(0, state.euclid.beat);
    TEST_ASSERT_EQUAL(0xACE1, state.prng.value); // The same seed every time
    TEST_ASSERT_EQUAL(0, state.clock.phase);
    TEST_ASSERT_EQUAL(SEQ_PULSES_PER_STEP, state.step_pulses); // A step is due
    TEST_ASSERT_EQUAL(0, state.gate.pulses_left);              // Gate closed
    TEST_ASSERT_EQUAL(0, state.cv);
    TEST_ASSERT(state.transport.b_run);                        // Running
    TEST_ASSERT(state.transport.b_starting);
    TEST_ASSERT(!state.clock.b_synced);
}



static void test_first_tick_begins_step_zero(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.raw_steps[0] = 0xC3U; // step 0 = 0xC, step 1 = 0x3
    (void)memset(&out, POISON_BYTE, sizeof(out));
    seq_tick(&state, &in, &out);

    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT(out.b_note_valid);
    TEST_ASSERT(!out.b_rest);
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(0xB, out.semitone); // Note value 0xC
}



static void test_first_tick_begins_step_zero_with_no_time_elapsed(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.elapsed_ticks = 0U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.step);

    // And only the first: with no time passing, nothing more falls due
    seq_tick(&state, &in, &out);
    TEST_ASSERT(!out.b_step_started);
}



static void test_ticks_between_steps_report_nothing(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    (void)memset(in.raw_steps, 0xFF, PANEL_STEP_BYTE_COUNT);
    seq_tick(&state, &in, &out); // Step 0 begins

    (void)memset(&out, POISON_BYTE, sizeof(out));
    seq_tick(&state, &in, &out);
    TEST_ASSERT(!out.b_step_started);
    TEST_ASSERT(!out.b_note_valid);
    TEST_ASSERT(!out.b_rest);
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(0, out.semitone);
    TEST_ASSERT_EQUAL(1, state.plain.address.position); // Step 1 is still to come
}



static void test_step_length_follows_the_tempo(void)
{
    static const uint16_t tempo_raw[] =
    {
        TEMPO_RAW_30_BPM, TEMPO_RAW_60_BPM, TEMPO_RAW_120_BPM, TEMPO_RAW_150_BPM, TEMPO_RAW_250_BPM
    };
    static const unsigned int expected_ticks[] = { 500U, 250U, 125U, 100U, 60U };

    for (size_t i = 0U; i < (sizeof(tempo_raw) / sizeof(tempo_raw[0])); i++)
    {
        seq_state_t   state;
        seq_inputs_t  in = make_valid_inputs();
        seq_outputs_t out;

        seq_init(&state);
        in.tempo_raw = tempo_raw[i];

        // The first tick begins step 0, and its time is not counted
        TEST_ASSERT_EQUAL(1, ticks_to_next_step(&state, &in, &out));

        // Every step, the first included, is exactly 15000 / BPM ticks
        for (unsigned int step = 1U; step < 40U; step++)
        {
            TEST_ASSERT_EQUAL(expected_ticks[i], ticks_to_next_step(&state, &in, &out));
            TEST_ASSERT_EQUAL(step % PANEL_STEP_COUNT, out.step);
        }
    }
}



static void test_readings_within_one_bpm_give_the_same_tempo(void)
{
    // 480 to 483 are all 150 BPM; 484 is 151
    static const uint16_t tempo_raw[] = { 480U, 481U, 483U };

    for (size_t i = 0U; i < (sizeof(tempo_raw) / sizeof(tempo_raw[0])); i++)
    {
        seq_state_t   state;
        seq_inputs_t  in = make_valid_inputs();
        seq_outputs_t out;

        seq_init(&state);
        in.tempo_raw = tempo_raw[i];
        (void)ticks_to_next_step(&state, &in, &out);
        (void)ticks_to_next_step(&state, &in, &out);
        TEST_ASSERT_EQUAL(100, ticks_to_next_step(&state, &in, &out));
    }
}



static void test_fastest_tempo_is_285_bpm(void)
{
    // 285 BPM is 19 steps a second, which does not divide into whole ticks
    // per step, so count the steps that begin in ten seconds instead
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;
    unsigned int  steps = 0U;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_MAX;
    seq_tick(&state, &in, &out); // Step 0, not counted
    for (unsigned int tick = 1U; tick <= 10000U; tick++)
    {
        seq_tick(&state, &in, &out);
        if (out.b_step_started)
        {
            steps++;
        }
    }
    TEST_ASSERT_EQUAL(190, steps);
}



static void test_steps_follow_in_order_and_wrap(void)
{
    seq_state_t  state;
    seq_inputs_t in = make_counting_inputs();

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    // Three times round the pattern: each step is the next one, with its note.
    // Step n holds the note value n, so a wrong byte or nibble shows up as a
    // wrong pitch
    for (unsigned int count = 0U; count < (3U * PANEL_STEP_COUNT); count++)
    {
        seq_outputs_t      out;
        const unsigned int expected_step = count % PANEL_STEP_COUNT;

        TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
        TEST_ASSERT_EQUAL(expected_step, out.step);
        TEST_ASSERT(out.b_note_valid);
        TEST_ASSERT((0U == expected_step) == out.b_rest);
        TEST_ASSERT_EQUAL((0U == expected_step) ? 0U : (expected_step - 1U), out.semitone);
        TEST_ASSERT_EQUAL((expected_step + 1U) % PANEL_STEP_COUNT,
                          state.plain.address.position);
    }
}



static void test_note_comes_from_the_tick_its_step_begins_on(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw    = TEMPO_RAW_250_BPM;
    in.raw_steps[0] = 0x12U; // step 0 = 1, step 1 = 2
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(0, out.semitone);

    // Step 1 is changed on the panel while step 0 plays: the new value is heard
    in.raw_steps[0] = 0x19U;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT_EQUAL(8, out.semitone);
}



static void test_step_begins_on_time_when_steps_are_invalid(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    (void)ticks_to_next_step(&state, &in, &out); // Step 0
    (void)ticks_to_next_step(&state, &in, &out); // Step 1

    // A failed read still takes its turn, and on time: step 2 begins, as
    // neither a pitch nor a rest
    in.b_steps_valid = false;
    (void)memset(&out, POISON_BYTE, sizeof(out));
    TEST_ASSERT_EQUAL(100, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT(!out.b_note_valid);
    TEST_ASSERT(!out.b_rest);
    TEST_ASSERT_EQUAL(2, out.step);
    TEST_ASSERT_EQUAL(0, out.semitone);

    // So the pattern is where it would have been once the reads work again
    in.b_steps_valid = true;
    TEST_ASSERT_EQUAL(100, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(3, out.step);
    TEST_ASSERT_EQUAL(2, out.semitone);
}



static void test_notes_decode_high_nibble_first(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw    = TEMPO_RAW_250_BPM;
    in.raw_steps[0] = 0xA5U; // step 0 = 0xA, step 1 = 0x5
    in.raw_steps[1] = 0x0FU; // step 2 = 0x0, step 3 = 0xF
    // The high nibble of each byte plays before the low one
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT(out.b_note_valid);
    TEST_ASSERT_EQUAL(0x9, out.semitone);
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(0x4, out.semitone);
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT(out.b_rest);
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(0xE, out.semitone);
}



static void test_each_step_is_independent(void)
{
    // Set one step at a time to the maximum; every other step must be a rest
    for (uint8_t target = 0U; target < PANEL_STEP_COUNT; target++)
    {
        seq_state_t   state;
        seq_inputs_t  in = make_valid_inputs();
        const uint8_t byte_index = (uint8_t)(target / 2U);
        const bool    b_high     = (0U == (target % 2U));

        seq_init(&state);
        in.tempo_raw             = TEMPO_RAW_250_BPM;
        in.raw_steps[byte_index] = b_high ? 0xF0U : 0x0FU;

        for (uint8_t step = 0U; step < PANEL_STEP_COUNT; step++)
        {
            seq_outputs_t out;

            (void)ticks_to_next_step(&state, &in, &out);
            TEST_ASSERT_EQUAL(step, out.step);
            TEST_ASSERT((step != target) == out.b_rest);
            TEST_ASSERT_EQUAL((step == target) ? (PANEL_VALUE_MAX - 1U) : 0U, out.semitone);
        }
    }
}



static void test_tempo_is_the_minimum_before_any_valid_reading(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.b_tempo_valid = false;
    in.tempo_raw     = TEMPO_RAW_250_BPM; // Must be ignored: the reading is flagged invalid
    (void)ticks_to_next_step(&state, &in, &out);
    (void)ticks_to_next_step(&state, &in, &out);

    TEST_ASSERT_EQUAL(500, ticks_to_next_step(&state, &in, &out)); // 30 BPM
    TEST_ASSERT_EQUAL(0, state.last_tempo_raw);
}



static void test_invalid_tempo_reuses_last_valid(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    (void)ticks_to_next_step(&state, &in, &out);
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(100, ticks_to_next_step(&state, &in, &out));

    // A failed read keeps the previous tempo, for as many ticks as it lasts
    in.b_tempo_valid = false;
    in.tempo_raw     = 0U;
    TEST_ASSERT_EQUAL(100, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(100, ticks_to_next_step(&state, &in, &out));

    // The next good read takes over again
    in.b_tempo_valid = true;
    in.tempo_raw     = TEMPO_RAW_250_BPM;
    TEST_ASSERT_EQUAL(60, ticks_to_next_step(&state, &in, &out));
}



static void test_steps_and_tempo_validity_are_independent(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.raw_steps[0]  = 0x12U;
    in.tempo_raw     = TEMPO_RAW_250_BPM;
    in.b_steps_valid = false;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT(!out.b_note_valid);
    TEST_ASSERT_EQUAL(TEMPO_RAW_250_BPM, state.last_tempo_raw); // Tempo still taken when steps fail

    in.b_steps_valid = true;
    in.b_tempo_valid = false;
    TEST_ASSERT_EQUAL(60, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT(out.b_note_valid);
    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT_EQUAL(1, out.semitone); // The step still decodes when tempo fails
}



static void test_several_ticks_at_once_count_in_full(void)
{
    // 150 BPM is 100 ticks per step; handed over ten at a time, a step
    // begins on every tenth call. The time on the first call is not counted
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_150_BPM;
    in.elapsed_ticks = 10U;
    TEST_ASSERT_EQUAL(1, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(10, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(10, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(10, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(3, out.step);
}



static void test_steps_owed_after_a_stall_begin_one_per_tick(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_MAX;
    seq_tick(&state, &in, &out); // Step 0

    // 255 ticks at 285 BPM is 116 pulses: four whole steps and most of a fifth
    in.elapsed_ticks = 255U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(1, out.step);

    // None of them is skipped: the rest begin on the ticks that follow
    in.elapsed_ticks = 1U;
    for (unsigned int step = 2U; step <= 4U; step++)
    {
        seq_tick(&state, &in, &out);
        TEST_ASSERT(out.b_step_started);
        TEST_ASSERT_EQUAL(step, out.step);
        TEST_ASSERT_EQUAL(step - 1U, out.semitone);
    }
    seq_tick(&state, &in, &out);
    TEST_ASSERT(!out.b_step_started);
}



static void test_tick_does_not_change_inputs(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_inputs_t  in_before;
    seq_outputs_t out;

    seq_init(&state);
    in.raw_steps[3]       = 0x9CU;
    in.tempo_raw          = 123U;
    in.address.direction  = ADDRESS_PENDULUM;
    in.address.first_step = 3U;
    in.address.b_one_shot = true;
    in.note_map.scale     = NOTE_MAP_SCALE_MINOR;
    in.note_map.root      = 5U;
    in.b_reset            = true;
    in_before = in;
    seq_tick(&state, &in, &out);

    TEST_ASSERT_EQUAL(0, memcmp(in_before.raw_steps, in.raw_steps, sizeof(in.raw_steps)));
    TEST_ASSERT_EQUAL(in_before.tempo_raw, in.tempo_raw);
    TEST_ASSERT_EQUAL(in_before.elapsed_ticks, in.elapsed_ticks);
    TEST_ASSERT(in_before.b_steps_valid == in.b_steps_valid);
    TEST_ASSERT(in_before.b_tempo_valid == in.b_tempo_valid);
    TEST_ASSERT_EQUAL(in_before.address.direction, in.address.direction);
    TEST_ASSERT_EQUAL(in_before.address.first_step, in.address.first_step);
    TEST_ASSERT_EQUAL(in_before.address.last_step, in.address.last_step);
    TEST_ASSERT(in_before.address.b_one_shot == in.address.b_one_shot);
    TEST_ASSERT_EQUAL(in_before.note_map.scale, in.note_map.scale);
    TEST_ASSERT_EQUAL(in_before.note_map.root, in.note_map.root);
    TEST_ASSERT_EQUAL(in_before.gate.length, in.gate.length);
    TEST_ASSERT(in_before.b_reset == in.b_reset);
}



static void test_note_value_zero_is_a_rest(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw       = TEMPO_RAW_250_BPM;
    in.raw_steps[0]    = 0x01U; // step 0 = 0, step 1 = 1
    in.note_map.scale  = NOTE_MAP_SCALE_MAJOR;
    in.note_map.root   = 7U;    // A rest stays a rest whatever the root

    (void)memset(&out, POISON_BYTE, sizeof(out));
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT(out.b_note_valid);
    TEST_ASSERT(out.b_rest);
    TEST_ASSERT_EQUAL(0, out.semitone);

    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT(!out.b_rest);
    TEST_ASSERT_EQUAL(7, out.semitone); // Note value 1 is the root
}



static void test_scale_and_root_set_the_pitch(void)
{
    // Steps 1 to 8 hold note values 1 to 8: one octave of the scale
    static const uint8_t major_from_d[] = { 2U, 4U, 6U, 7U, 9U, 11U, 13U, 14U };
    seq_state_t          state;
    seq_inputs_t         in = make_counting_inputs();
    seq_outputs_t        out;

    seq_init(&state);
    in.tempo_raw      = TEMPO_RAW_250_BPM;
    in.note_map.scale = NOTE_MAP_SCALE_MAJOR;
    in.note_map.root  = 2U;
    (void)ticks_to_next_step(&state, &in, &out); // Step 0, a rest

    for (size_t i = 0U; i < (sizeof(major_from_d) / sizeof(major_from_d[0])); i++)
    {
        (void)ticks_to_next_step(&state, &in, &out);
        TEST_ASSERT_EQUAL(i + 1U, out.step);
        TEST_ASSERT_EQUAL(major_from_d[i], out.semitone);
    }

    // The controls are read when the step begins, like the panel
    in.note_map.scale = NOTE_MAP_SCALE_CHROMATIC;
    in.note_map.root  = 0U;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(9, out.step);
    TEST_ASSERT_EQUAL(8, out.semitone);
}



static void test_addressing_controls_set_the_order_of_steps(void)
{
    static const uint8_t expected_steps[] = { 7U, 6U, 5U, 4U, 7U, 6U };
    seq_state_t          state;
    seq_inputs_t         in = make_counting_inputs();
    seq_outputs_t        out;

    seq_init(&state);
    in.tempo_raw          = TEMPO_RAW_150_BPM;
    in.address.direction  = ADDRESS_REVERSE;
    in.address.first_step = 4U;
    in.address.last_step  = 7U;

    for (size_t i = 0U; i < (sizeof(expected_steps) / sizeof(expected_steps[0])); i++)
    {
        const unsigned int ticks = ticks_to_next_step(&state, &in, &out);

        // The first step of the pattern begins on the first tick, as ever
        TEST_ASSERT_EQUAL((0U == i) ? 1U : 100U, ticks);
        TEST_ASSERT_EQUAL(expected_steps[i], out.step);
        TEST_ASSERT_EQUAL(expected_steps[i] - 1U, out.semitone);
    }
}



static void test_one_shot_stops_after_one_cycle_and_the_clock_runs_on(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw          = TEMPO_RAW_150_BPM;
    in.address.last_step  = 2U;
    in.address.b_one_shot = true;

    for (unsigned int step = 0U; step <= 2U; step++)
    {
        TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
        TEST_ASSERT_EQUAL(step, out.step);
    }

    // Nothing more begins: 2000 ticks is 20 step lengths, to the tick
    (void)memset(&out, POISON_BYTE, sizeof(out));
    TEST_ASSERT_EQUAL(0, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT(!out.b_step_started);
    TEST_ASSERT(!out.b_note_valid);
    TEST_ASSERT(!out.b_rest);
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(0, out.semitone);

    // Looping again picks the pattern up on the beat it never left
    in.address.b_one_shot = false;
    TEST_ASSERT_EQUAL(100, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(0, out.step);
}



static void test_reset_goes_back_to_the_first_step_without_moving_the_clock(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    for (unsigned int step = 0U; step <= 4U; step++)
    {
        (void)ticks_to_next_step(&state, &in, &out);
        TEST_ASSERT_EQUAL(step, out.step);
    }

    // Ten ticks into step 4, a reset lasting one tick. No step begins on it
    for (unsigned int tick = 0U; tick < 10U; tick++)
    {
        seq_tick(&state, &in, &out);
    }
    in.b_reset = true;
    seq_tick(&state, &in, &out);
    TEST_ASSERT(!out.b_step_started);

    // The next step is step 0, and it begins when step 5 would have
    in.b_reset = false;
    TEST_ASSERT_EQUAL(89, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(100, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(1, out.step);
}



static void test_reset_held_keeps_the_pattern_on_its_first_step(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw          = TEMPO_RAW_150_BPM;
    in.address.first_step = 6U;
    in.b_reset            = true;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(6, out.step);
    for (unsigned int count = 0U; count < 4U; count++)
    {
        TEST_ASSERT_EQUAL(6, out.step);
        TEST_ASSERT_EQUAL(100, ticks_to_next_step(&state, &in, &out));
    }

    in.b_reset = false;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(7, out.step); // Step 6 began on the last held tick
}



static void test_reset_plays_a_finished_one_shot_again(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw          = TEMPO_RAW_150_BPM;
    in.address.last_step  = 1U;
    in.address.b_one_shot = true;
    (void)ticks_to_next_step(&state, &in, &out);
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(0, ticks_to_next_step(&state, &in, &out)); // Finished

    in.b_reset = true;
    seq_tick(&state, &in, &out);
    in.b_reset = false;
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT_EQUAL(0, ticks_to_next_step(&state, &in, &out)); // And finished again
}



static void test_gate_opens_with_a_pitch_for_its_length(void)
{
    // 150 BPM: a step is 100 ticks and 12 pulses are exactly 50
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT); // Every step a pitch
    (void)memset(&out, POISON_BYTE, sizeof(out));
    seq_tick(&state, &in, &out); // Step 0 begins
    TEST_ASSERT(out.b_gate);     // The gate opens on the tick the step begins
    TEST_ASSERT(out.b_clock_out);
    run_and_trace(&state, &in, 99U, &gate, &clock_out);
    TEST_ASSERT(!gate.b_level);

    // Sixteen whole steps: one gate each, open for half of it
    gate.rises      = 0U;
    gate.high_ticks = 0U;
    run_and_trace(&state, &in, 1600U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(16, gate.rises);
    TEST_ASSERT_EQUAL(16U * 50U, gate.high_ticks);
}



static void test_gate_length_is_counted_in_pulses(void)
{
    // At 150 BPM pulse n of a step falls 25 * n / 6 ticks into it, rounded up
    static const uint8_t      lengths[]        = { 1U, 6U, 18U, 23U };
    static const unsigned int expected_ticks[] = { 5U, 25U, 75U, 96U };

    for (size_t i = 0U; i < (sizeof(lengths) / sizeof(lengths[0])); i++)
    {
        seq_state_t   state;
        seq_inputs_t  in = make_valid_inputs();
        level_trace_t gate      = { 0U, 0U, false };
        level_trace_t clock_out = { 0U, 0U, false };

        seq_init(&state);
        in.tempo_raw   = TEMPO_RAW_150_BPM;
        in.gate.length = lengths[i];
        (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT);
        run_and_trace(&state, &in, 100U, &gate, &clock_out); // All of step 0
        TEST_ASSERT(!gate.b_level);
        gate.high_ticks = 0U;
        gate.rises      = 0U;
        run_and_trace(&state, &in, 100U, &gate, &clock_out); // All of step 1

        TEST_ASSERT_EQUAL(1, gate.rises);
        TEST_ASSERT_EQUAL(expected_ticks[i], gate.high_ticks);
        TEST_ASSERT(!gate.b_level); // Closed before step 2
    }
}



static void test_gate_stays_closed_for_a_rest_and_an_unreadable_step(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw    = TEMPO_RAW_150_BPM;
    in.raw_steps[0] = 0x50U; // Step 0 a pitch, step 1 a rest
    in.raw_steps[1] = 0x55U; // Steps 2 and 3 pitches

    run_and_trace(&state, &in, 100U, &gate, &clock_out); // Step 0
    TEST_ASSERT_EQUAL(1, gate.rises);
    run_and_trace(&state, &in, 100U, &gate, &clock_out); // Step 1, a rest
    TEST_ASSERT_EQUAL(1, gate.rises);

    in.b_steps_valid = false;
    run_and_trace(&state, &in, 100U, &gate, &clock_out); // Step 2, not readable
    TEST_ASSERT_EQUAL(1, gate.rises);

    in.b_steps_valid = true;
    run_and_trace(&state, &in, 100U, &gate, &clock_out); // Step 3
    TEST_ASSERT_EQUAL(2, gate.rises);
    TEST_ASSERT_EQUAL(50U + 50U, gate.high_ticks);
}



static void test_a_whole_step_gate_ties_notes_and_a_rest_ends_it(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw    = TEMPO_RAW_150_BPM;
    in.gate.length  = SEQ_PULSES_PER_STEP;
    in.raw_steps[0] = 0x55U; // Steps 0 to 2 are pitches, step 3 a rest
    in.raw_steps[1] = 0x50U;
    in.raw_steps[2] = 0x50U; // Step 4 a pitch again

    // Three notes are one gate, 300 ticks long
    run_and_trace(&state, &in, 300U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(1, gate.rises);
    TEST_ASSERT_EQUAL(300, gate.high_ticks);
    TEST_ASSERT(gate.b_level);

    // The rest closes it on the tick step 3 begins
    run_and_trace(&state, &in, 1U, &gate, &clock_out);
    TEST_ASSERT(!gate.b_level);
    run_and_trace(&state, &in, 99U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(300, gate.high_ticks);

    run_and_trace(&state, &in, 1U, &gate, &clock_out); // Step 4
    TEST_ASSERT(gate.b_level);
    TEST_ASSERT_EQUAL(2, gate.rises);
}



static void test_gate_length_is_read_when_the_step_begins(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT);
    run_and_trace(&state, &in, 10U, &gate, &clock_out);

    // Changed while step 0 is held: that note keeps the length it began with
    in.gate.length = 0U;
    run_and_trace(&state, &in, 90U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(50, gate.high_ticks);

    // And step 1 takes the new one: it never opens
    run_and_trace(&state, &in, 100U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(1, gate.rises);
    TEST_ASSERT_EQUAL(50, gate.high_ticks);
}



static void test_gate_of_a_late_step_counts_from_when_it_was_due(void)
{
    // 250 BPM is 60 ticks a step, a pulse every 2.5 ticks. Step 1 is due on
    // tick 60 and its gate should close 30 ticks later, on tick 90
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT);
    in.elapsed_ticks = 0U;
    seq_tick(&state, &in, &out); // Tick 0: step 0

    // The loop stalls and comes back on tick 80, 20 ticks (8 pulses) late
    in.elapsed_ticks = 80U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT(out.b_gate);
    TEST_ASSERT_EQUAL(4, state.gate.pulses_left); // 12 less the 8 already gone
    TEST_ASSERT(out.b_clock_out);

    in.elapsed_ticks = 9U;
    seq_tick(&state, &in, &out); // Tick 89
    TEST_ASSERT(out.b_gate);
    in.elapsed_ticks = 1U;
    seq_tick(&state, &in, &out); // Tick 90
    TEST_ASSERT(!out.b_gate);
    TEST_ASSERT(!out.b_clock_out);

    // A stall longer than the gate: the note is begun, but its gate has gone
    in.elapsed_ticks = 70U;
    seq_tick(&state, &in, &out); // Tick 160: step 2, due on tick 120
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(2, out.step);
    TEST_ASSERT(!out.b_gate);
    TEST_ASSERT(!out.b_clock_out);
}



static void test_clock_out_is_high_for_the_first_half_of_every_step(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs(); // Every step a rest
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    run_and_trace(&state, &in, 100U, &gate, &clock_out); // Step 0
    TEST_ASSERT_EQUAL(1, clock_out.rises);
    TEST_ASSERT_EQUAL(50, clock_out.high_ticks);
    TEST_ASSERT(!clock_out.b_level);

    run_and_trace(&state, &in, 1U, &gate, &clock_out); // It rises as step 1 begins
    TEST_ASSERT(clock_out.b_level);
    run_and_trace(&state, &in, 1599U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(17, clock_out.rises);
    TEST_ASSERT_EQUAL(17U * 50U, clock_out.high_ticks);

    // It does not depend on a note: the gate never opened
    TEST_ASSERT_EQUAL(0, gate.rises);
}



static void test_clock_out_runs_on_when_a_one_shot_has_finished(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw          = TEMPO_RAW_150_BPM;
    in.gate.length        = SEQ_PULSES_PER_STEP; // Tied, so only the end closes it
    in.address.last_step  = 1U;
    in.address.b_one_shot = true;
    (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT);

    // Two steps play as one gate, which ends when the third falls due
    run_and_trace(&state, &in, 199U, &gate, &clock_out);
    TEST_ASSERT(gate.b_level);
    run_and_trace(&state, &in, 800U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(1, gate.rises);
    TEST_ASSERT_EQUAL(200, gate.high_ticks);

    // The clock output kept its beat through all ten step lengths
    TEST_ASSERT_EQUAL(10, clock_out.rises);
    TEST_ASSERT_EQUAL(500, clock_out.high_ticks);
}



static void test_reset_does_not_cut_a_held_note(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT);
    run_and_trace(&state, &in, 10U, &gate, &clock_out);

    in.b_reset = true;
    run_and_trace(&state, &in, 1U, &gate, &clock_out);
    in.b_reset = false;
    run_and_trace(&state, &in, 88U, &gate, &clock_out);

    TEST_ASSERT_EQUAL(50, gate.high_ticks);
    TEST_ASSERT_EQUAL(1, clock_out.rises);
    TEST_ASSERT_EQUAL(50, clock_out.high_ticks);
}



static void test_cv_is_set_by_each_note_on_the_tick_its_gate_opens(void)
{
    // Steps 1 to 4 hold note values 1 to 4: pitches 0 to 3 in the chromatic scale
    static const uint16_t expected_cv[] = { 0U, 83U, 167U, 250U };
    seq_state_t           state;
    seq_inputs_t          in = make_counting_inputs();
    seq_outputs_t         out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    (void)ticks_to_next_step(&state, &in, &out); // Step 0, a rest

    for (size_t i = 0U; i < (sizeof(expected_cv) / sizeof(expected_cv[0])); i++)
    {
        (void)ticks_to_next_step(&state, &in, &out);
        TEST_ASSERT(out.b_gate); // The same tick: the pitch is there as the note starts
        TEST_ASSERT_EQUAL(expected_cv[i], out.cv);
    }

    // The scale and root come first: note value 5 in D major is 9 semitones up
    in.note_map.scale = NOTE_MAP_SCALE_MAJOR;
    in.note_map.root  = 2U;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(9, out.semitone);
    TEST_ASSERT_EQUAL(750, out.cv);
}



static void test_cv_is_given_on_every_tick_and_holds_after_the_gate_closes(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw    = TEMPO_RAW_150_BPM;
    in.raw_steps[0] = 0xD0U; // Step 0 plays pitch 12, an octave up; the rest are rests

    // 100 ticks a step: through the gate, past its end and through step 1's rest
    for (unsigned int tick = 0U; tick < 199U; tick++)
    {
        (void)memset(&out, POISON_BYTE, sizeof(out));
        seq_tick(&state, &in, &out);
        TEST_ASSERT_EQUAL(CV_PER_OCTAVE, out.cv);
    }
    TEST_ASSERT(!out.b_gate);
}



static void test_cv_is_zero_until_the_first_note(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs(); // Every step a rest
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    for (unsigned int step = 0U; step < 3U; step++)
    {
        (void)ticks_to_next_step(&state, &in, &out);
        TEST_ASSERT(out.b_rest);
        TEST_ASSERT_EQUAL(0, out.cv);
    }
}



static void test_cv_holds_through_a_rest_an_unreadable_step_and_a_reset(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw    = TEMPO_RAW_250_BPM;
    in.raw_steps[0] = 0x70U; // Step 0 plays pitch 6; step 1 is a rest
    in.raw_steps[1] = 0xD0U; // Step 2 plays pitch 12

    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(500, out.cv);

    (void)ticks_to_next_step(&state, &in, &out); // The rest
    TEST_ASSERT(out.b_rest);
    TEST_ASSERT_EQUAL(500, out.cv);

    in.b_steps_valid = false; // Step 2 cannot be read
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT(!out.b_note_valid);
    TEST_ASSERT_EQUAL(500, out.cv);

    // A reset sends the pattern back, and the pitch stays until a note plays
    in.b_steps_valid = true;
    in.raw_steps[0]  = 0x00U;
    in.b_reset       = true;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(500, out.cv);
    (void)ticks_to_next_step(&state, &in, &out); // Step 0 again, now a rest
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(500, out.cv);
}



static void test_cv_holds_when_a_one_shot_has_finished(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw          = TEMPO_RAW_250_BPM;
    in.raw_steps[0]       = 0xD0U; // Step 0 plays pitch 12
    in.address.last_step  = 0U;
    in.address.b_one_shot = true;

    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(CV_PER_OCTAVE, out.cv);

    // Steps keep falling due and none begins
    for (unsigned int tick = 0U; tick < 200U; tick++)
    {
        seq_tick(&state, &in, &out);
        TEST_ASSERT(!out.b_step_started);
        TEST_ASSERT_EQUAL(CV_PER_OCTAVE, out.cv);
    }
}



static void test_calibration_table_is_read_when_the_step_begins(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw    = TEMPO_RAW_250_BPM;
    in.raw_steps[0] = 0xDDU; // Steps 0 and 1 both play pitch 12

    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(CV_PER_OCTAVE, out.cv);

    // A table changed mid-note is not heard until the next note
    in.pitch_cal.cv[1] = 1017U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(CV_PER_OCTAVE, out.cv);
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(1017, out.cv);
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
static unsigned int run_ticks(seq_state_t *p_state, const seq_inputs_t *p_in,
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
static void wait_for_external(seq_state_t *p_state)
{
    p_state->transport.b_external = true;
}

/**
 * @brief One tick with an edge of the external clock on it.
 */
static void edge_tick(seq_state_t *p_state, seq_inputs_t *p_in, seq_outputs_t *p_out)
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
static void press_run(seq_state_t *p_state, seq_inputs_t *p_in, seq_outputs_t *p_out)
{
    p_in->b_run_button = true;
    (void)run_ticks(p_state, p_in, p_out, SETTLE_TICKS);
    p_in->b_run_button = false;
}



static void test_transport_is_given_on_every_tick(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    for (unsigned int tick = 0U; tick < 3U; tick++)
    {
        (void)memset(&out, POISON_BYTE, sizeof(out));
        seq_tick(&state, &in, &out);
        TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, out.transport);
        TEST_ASSERT(!out.b_transport_changed);
    }
}



static void test_run_button_stops_the_pattern_at_once(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;
    uint16_t      cv;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    (void)memset(in.raw_steps, 0x77, PANEL_STEP_BYTE_COUNT); // Every step a pitch
    (void)run_ticks(&state, &in, &out, 10U);
    cv = out.cv;
    TEST_ASSERT(cv > 0U);

    // The press counts once it has settled, and the note is cut there
    in.b_run_button = true;
    (void)run_ticks(&state, &in, &out, SETTLE_TICKS - 1U);
    TEST_ASSERT(out.b_gate);
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, out.transport);

    (void)memset(&out, POISON_BYTE, sizeof(out));
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);
    TEST_ASSERT(out.b_transport_changed);
    TEST_ASSERT(!out.b_gate);
    TEST_ASSERT(!out.b_clock_out);
    TEST_ASSERT(!out.b_step_started);
    TEST_ASSERT(!out.b_note_valid);
    TEST_ASSERT(!out.b_rest);
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(0, out.semitone);
    TEST_ASSERT_EQUAL(cv, out.cv); // The pitch holds

    // Nothing begins while it is stopped, however long
    in.b_run_button = false;
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 2000U));
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);
    TEST_ASSERT(!out.b_transport_changed);
    TEST_ASSERT(!out.b_gate);
    TEST_ASSERT(!out.b_clock_out);
    TEST_ASSERT_EQUAL(cv, out.cv);
}



static void test_run_button_starts_again_from_the_first_step(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    TEST_ASSERT_EQUAL(5, run_ticks(&state, &in, &out, 437U)); // Into step 4

    press_run(&state, &in, &out);
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 333U));

    // The first step begins on the tick the press counts, and is a whole one
    (void)memset(&out, POISON_BYTE, sizeof(out));
    press_run(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, out.transport);
    TEST_ASSERT(out.b_transport_changed);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT(out.b_clock_out);

    TEST_ASSERT_EQUAL(100, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT_EQUAL(100, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(2, out.step);
}



static void test_time_on_the_tick_of_a_start_is_not_counted(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    (void)run_ticks(&state, &in, &out, 10U);
    press_run(&state, &in, &out);
    (void)run_ticks(&state, &in, &out, SETTLE_TICKS);

    // A late pass starts the pattern: its time went by before the start
    in.b_run_button  = true;
    in.elapsed_ticks = 250U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.step);

    in.b_run_button  = false;
    in.elapsed_ticks = 1U;
    TEST_ASSERT_EQUAL(100, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(1, out.step);
}



static void test_a_stopped_pattern_ignores_reset_and_the_external_clock(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_ONE_TO_ONE;
    wait_for_external(&state);
    seq_tick(&state, &in, &out);
    press_run(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);

    in.b_reset      = true;
    in.b_clock_edge = true;
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 300U));
    TEST_ASSERT(!out.b_gate);
    TEST_ASSERT(!out.b_clock_out);
    TEST_ASSERT(!state.clock.b_synced); // The edges were not followed
}



static void test_each_external_edge_is_a_step(void)
{
    // Each gap is more than half the one before, so no step is held back
    static const unsigned int gaps[] = { 100U, 60U, 250U, 130U, 70U };
    seq_state_t               state;
    seq_inputs_t              in = make_counting_inputs();
    seq_outputs_t             out;

    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_ONE_TO_ONE;
    wait_for_external(&state);

    // External from the first tick: nothing begins until an edge comes
    (void)memset(&out, POISON_BYTE, sizeof(out));
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, out.transport);
    TEST_ASSERT(out.b_transport_changed);
    TEST_ASSERT(!out.b_step_started);
    TEST_ASSERT(!out.b_gate);
    TEST_ASSERT(!out.b_clock_out);
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 3000U));
    TEST_ASSERT(!out.b_clock_out);

    edge_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT(out.b_clock_out);

    // Each edge after it begins the next step, whatever the gap
    for (size_t i = 0U; i < (sizeof(gaps) / sizeof(gaps[0])); i++)
    {
        TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, gaps[i] - 1U));
        edge_tick(&state, &in, &out);
        TEST_ASSERT(out.b_step_started);
        TEST_ASSERT_EQUAL(i + 1U, out.step);
    }

    // And when the edges stop, so does the pattern
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 5000U));
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, out.transport);
}



static void test_external_gate_and_clock_out_are_half_the_measured_step(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_ONE_TO_ONE;
    wait_for_external(&state);
    (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT);
    run_and_trace(&state, &in, 5U, &gate, &clock_out);

    // The first step has no spacing to go by: it is taken as 20 ticks
    in.b_clock_edge = true;
    run_and_trace(&state, &in, 1U, &gate, &clock_out);
    in.b_clock_edge = false;
    run_and_trace(&state, &in, 99U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(1, gate.rises);
    TEST_ASSERT_EQUAL(10, gate.high_ticks);
    TEST_ASSERT_EQUAL(1, clock_out.rises);
    TEST_ASSERT_EQUAL(10, clock_out.high_ticks);

    // From the second edge the steps are known to be 100 ticks
    for (unsigned int step = 1U; step <= 8U; step++)
    {
        in.b_clock_edge = true;
        run_and_trace(&state, &in, 1U, &gate, &clock_out);
        in.b_clock_edge = false;
        run_and_trace(&state, &in, 99U, &gate, &clock_out);
        TEST_ASSERT_EQUAL(1U + step, gate.rises);
        TEST_ASSERT_EQUAL(10U + (50U * step), gate.high_ticks);
        TEST_ASSERT_EQUAL(1U + step, clock_out.rises);
        TEST_ASSERT_EQUAL(10U + (50U * step), clock_out.high_ticks);
    }
}



static void test_tempo_reading_is_a_ratio_on_the_external_clock(void)
{
    static const struct
    {
        uint16_t tempo_raw;
        uint8_t  divide;
        uint8_t  multiply;
    } cases[] =
    {
        { 0U, 4U, 1U },   { 146U, 4U, 1U }, { 147U, 3U, 1U }, { 293U, 3U, 1U },
        { 294U, 2U, 1U }, { 440U, 2U, 1U }, { 441U, 1U, 1U }, { 587U, 1U, 1U },
        { 588U, 1U, 2U }, { 734U, 1U, 2U }, { 735U, 1U, 3U }, { 881U, 1U, 3U },
        { 882U, 1U, 4U }, { 1023U, 1U, 4U }, { 1030U, 1U, 4U }, { UINT16_MAX, 1U, 4U }
    };

    for (size_t i = 0U; i < (sizeof(cases) / sizeof(cases[0])); i++)
    {
        seq_state_t   state;
        seq_inputs_t  in = make_valid_inputs();
        seq_outputs_t out;

        seq_init(&state);
        in.tempo_raw     = cases[i].tempo_raw;
        wait_for_external(&state);

        // Two sync edges, so that the second has a spacing to multiply into
        for (unsigned int edges = 0U; edges < (2U * cases[i].divide); edges++)
        {
            edge_tick(&state, &in, &out);
            (void)run_ticks(&state, &in, &out, 9U);
        }
        edge_tick(&state, &in, &out);
        TEST_ASSERT_EQUAL(cases[i].divide - 1U, state.clock.edges_left);
        TEST_ASSERT_EQUAL(24U * cases[i].multiply, state.clock.per_sync);
    }
}



static void test_a_multiplied_clock_fits_steps_between_the_edges(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_MULTIPLY_4;
    wait_for_external(&state);

    // Two edges 100 ticks apart measure the spacing; one step each so far
    edge_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 99U));

    // Then four steps to an edge, 25 ticks apart, the first on the edge
    for (unsigned int tick = 0U; tick < 400U; tick++)
    {
        in.b_clock_edge = (0U == (tick % 100U));
        seq_tick(&state, &in, &out);
        TEST_ASSERT_EQUAL(0U == (tick % 25U), out.b_step_started);
        if (out.b_step_started)
        {
            TEST_ASSERT_EQUAL((1U + (tick / 25U)) % PANEL_STEP_COUNT, out.step);
        }
    }
}



static void test_steps_owed_after_an_early_edge_begin_one_per_tick(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_MULTIPLY_4;
    wait_for_external(&state);
    edge_tick(&state, &in, &out);
    (void)run_ticks(&state, &in, &out, 99U);
    edge_tick(&state, &in, &out); // Four steps to an edge from here: step 1
    TEST_ASSERT_EQUAL(1, out.step);

    // The next edge comes after 10 ticks, not 100: three of the four steps
    // had not begun. None is dropped: they begin on the ticks that follow,
    // the first of them a tick after the edge, since step 1 was still high
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 9U));
    edge_tick(&state, &in, &out);
    TEST_ASSERT(!out.b_step_started);
    for (uint8_t step = 2U; step <= 5U; step++)
    {
        seq_tick(&state, &in, &out);
        TEST_ASSERT(out.b_step_started);
        TEST_ASSERT_EQUAL(step, out.step);
    }
}



static void test_an_early_edge_gets_a_tick_of_low_before_its_step(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_ONE_TO_ONE;
    wait_for_external(&state);
    (void)memset(in.raw_steps, 0x33, PANEL_STEP_BYTE_COUNT); // Every step a pitch

    // Two edges 200 ticks apart: the steps are taken to be that long, so
    // the gate and the clock output are high for 100 ticks of each
    in.b_clock_edge = true;
    run_and_trace(&state, &in, 1U, &gate, &clock_out);
    in.b_clock_edge = false;
    run_and_trace(&state, &in, 199U, &gate, &clock_out);
    in.b_clock_edge = true;
    run_and_trace(&state, &in, 1U, &gate, &clock_out);
    in.b_clock_edge = false;
    run_and_trace(&state, &in, 59U, &gate, &clock_out);
    TEST_ASSERT(gate.b_level);
    TEST_ASSERT(clock_out.b_level);
    TEST_ASSERT_EQUAL(2, gate.rises);

    // The third edge comes after 60: both are still high. Its step waits
    // one tick, and both are low for it
    (void)memset(&out, POISON_BYTE, sizeof(out));
    edge_tick(&state, &in, &out);
    TEST_ASSERT(!out.b_step_started);
    TEST_ASSERT(!out.b_gate);
    TEST_ASSERT(!out.b_clock_out);

    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(2, out.step);
    TEST_ASSERT(out.b_gate);
    TEST_ASSERT(out.b_clock_out);

    // And only that step: the next edge, 60 later again, is on time
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 58U));
    TEST_ASSERT(!out.b_clock_out);
    edge_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(3, out.step);
}



static void test_a_tied_gate_is_not_broken_by_an_edge_on_time(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_ONE_TO_ONE;
    wait_for_external(&state);
    in.gate.length   = SEQ_PULSES_PER_STEP;
    (void)memset(in.raw_steps, 0x33, PANEL_STEP_BYTE_COUNT);

    // A whole-step gate ties the notes on the external clock as on the
    // internal one: the clock output has fallen by each edge, so no step
    // is held back
    for (unsigned int step = 0U; step < 5U; step++)
    {
        in.b_clock_edge = true;
        run_and_trace(&state, &in, 1U, &gate, &clock_out);
        in.b_clock_edge = false;
        run_and_trace(&state, &in, 99U, &gate, &clock_out);
    }
    TEST_ASSERT_EQUAL(1, gate.rises);
    TEST_ASSERT_EQUAL(5, clock_out.rises);
}



static void test_a_divided_clock_passes_over_edges(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_DIVIDE_4;
    wait_for_external(&state);

    // Edges 25 ticks apart: a step on the first and on every fourth after
    for (unsigned int tick = 0U; tick < 800U; tick++)
    {
        in.b_clock_edge = (0U == (tick % 25U));
        seq_tick(&state, &in, &out);
        TEST_ASSERT_EQUAL(0U == (tick % 100U), out.b_step_started);
        if (out.b_step_started)
        {
            TEST_ASSERT_EQUAL(tick / 100U, out.step);
        }
    }
}



static void test_last_valid_tempo_reading_is_the_ratio_when_one_fails(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_DIVIDE_4;
    wait_for_external(&state);
    seq_tick(&state, &in, &out);

    in.tempo_raw     = TEMPO_RAW_ONE_TO_ONE;
    in.b_tempo_valid = false;
    edge_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(3, state.clock.edges_left);
}



static void test_an_edge_takes_the_pattern_over_without_a_restart(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    // 155 BPM as a tempo: step 1 begins on tick 98 and its clock pulse and
    // gate are over by tick 160
    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_ONE_TO_ONE;
    TEST_ASSERT_EQUAL(2, run_ticks(&state, &in, &out, 160U));
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, out.transport);
    TEST_ASSERT(!out.b_clock_out);

    // The edge ends step 1 there and begins the next step, not the first
    edge_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, out.transport);
    TEST_ASSERT(out.b_transport_changed);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(2, out.step);

    // From there only an edge begins a step, however long it takes to come
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 5000U));
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, out.transport);
    TEST_ASSERT_EQUAL(SEQ_PULSES_PER_STEP - 1U, state.step_pulses);
    edge_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(3, out.step);
}



static void test_an_edge_while_the_clock_output_is_high_waits_a_tick(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_ONE_TO_ONE;
    TEST_ASSERT_EQUAL(2, run_ticks(&state, &in, &out, 110U));
    TEST_ASSERT(out.b_gate); // Step 1, a pitch, just begun
    TEST_ASSERT(out.b_clock_out);

    // The tick of the edge is the gap between the two steps
    edge_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, out.transport);
    TEST_ASSERT(!out.b_step_started);
    TEST_ASSERT(!out.b_gate);
    TEST_ASSERT(!out.b_clock_out);

    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(2, out.step);
    TEST_ASSERT(out.b_clock_out);
}



static void test_a_stop_and_a_start_go_back_to_the_internal_clock(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    wait_for_external(&state);
    seq_tick(&state, &in, &out);
    edge_tick(&state, &in, &out); // Step 0
    edge_tick(&state, &in, &out); // Step 1, held back a tick
    TEST_ASSERT_EQUAL(1, run_ticks(&state, &in, &out, 200U));

    press_run(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);
    (void)run_ticks(&state, &in, &out, SETTLE_TICKS);

    // The start is a start: the first step, at once, and the tempo sets
    // the pace from there
    press_run(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, out.transport);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT(!state.clock.b_synced);
    TEST_ASSERT_EQUAL(100, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(1, out.step);
}



static void test_reset_on_the_external_clock_plays_the_first_step_at_the_next_edge(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_ONE_TO_ONE;
    wait_for_external(&state);
    for (unsigned int step = 0U; step < 3U; step++)
    {
        edge_tick(&state, &in, &out);
        TEST_ASSERT_EQUAL(step, out.step);
        (void)run_ticks(&state, &in, &out, 49U);
    }

    in.b_reset = true;
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 1U));
    in.b_reset = false;
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 49U));
    edge_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.step);

    // A reset on the same tick as the edge counts for that edge
    (void)run_ticks(&state, &in, &out, 99U);
    in.b_reset = true;
    edge_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.step);
}



static void test_a_start_on_the_external_clock_waits_for_an_edge(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_ONE_TO_ONE;
    wait_for_external(&state);
    for (unsigned int step = 0U; step < 3U; step++)
    {
        edge_tick(&state, &in, &out);
        (void)run_ticks(&state, &in, &out, 49U);
    }

    // The stop forgets the external clock; an edge while stopped brings it
    // back, and begins nothing
    press_run(&state, &in, &out);
    edge_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, SETTLE_TICKS));
    press_run(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, out.transport);
    TEST_ASSERT(!out.b_step_started);
    TEST_ASSERT(!out.b_clock_out);
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 500U));

    edge_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.step);
}



/**
 * @brief Sets one step's four switches in the inputs.
 */
static void set_step(seq_inputs_t *p_in, uint8_t step, uint8_t value)
{
    const uint8_t byte_index = (uint8_t)(step / 2U);
    const uint8_t shift      = (0U == (step % 2U)) ? 4U : 0U;

    p_in->raw_steps[byte_index] =
        (uint8_t)((p_in->raw_steps[byte_index] & (uint8_t)~(uint8_t)(0x0FU << shift))
                  | (uint8_t)(value << shift));
}



/**
 * @brief Ticks the sequencer until a step begins and gives its number, or
 *        255 if none did.
 */
static uint8_t next_step(seq_state_t *p_state, const seq_inputs_t *p_in)
{
    seq_outputs_t out;

    return (0U != ticks_to_next_step(p_state, p_in, &out)) ? out.step : (uint8_t)255U;
}



static void test_mode_switches_choose_the_engine(void)
{
    // Stage 0 lasts two steps, repeating its note, which only the stage
    // engine acts on: it plays 0 0 1, and the plain engine 0 1 2
    static const uint8_t stage_modes[] = { 0x01U, 0x09U, 0xF9U };
    static const uint8_t plain_modes[] = { 0x00U, 0x06U, 0x07U, 0xF8U };

    for (size_t i = 0U; i < (sizeof(stage_modes) / sizeof(stage_modes[0])); i++)
    {
        seq_state_t  state;
        seq_inputs_t in = make_counting_inputs();

        seq_init(&state);
        in.tempo_raw = TEMPO_RAW_250_BPM;
        set_step(&in, 8U, 0x05U);
        in.raw_steps[PANEL_STEP_BYTE_COUNT] = stage_modes[i];
        TEST_ASSERT_EQUAL(0, next_step(&state, &in));
        TEST_ASSERT_EQUAL(0, next_step(&state, &in));
        TEST_ASSERT_EQUAL(1, next_step(&state, &in));
    }

    for (size_t i = 0U; i < (sizeof(plain_modes) / sizeof(plain_modes[0])); i++)
    {
        seq_state_t  state;
        seq_inputs_t in = make_counting_inputs();

        seq_init(&state);
        in.tempo_raw = TEMPO_RAW_250_BPM;
        set_step(&in, 8U, 0x05U);
        in.raw_steps[PANEL_STEP_BYTE_COUNT] = plain_modes[i];
        TEST_ASSERT_EQUAL(0, next_step(&state, &in));
        TEST_ASSERT_EQUAL(1, next_step(&state, &in));
        TEST_ASSERT_EQUAL(2, next_step(&state, &in));
    }
}



static void test_mode_two_is_the_klee_engine(void)
{
    // Stages 0 and 2 loaded, with values 3 and 4. The register moves a
    // stage a step: the active pair sits on 3 + 4, then on stages 1 and 3
    // (0 + 0, a rest), and after sixteen steps it is back
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    set_step(&in, 0U, 0x0BU);
    set_step(&in, 2U, 0x0CU);
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x02U;

    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT(!out.b_rest);
    TEST_ASSERT_EQUAL(6, out.semitone); // Note value 7 in the chromatic scale
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT(out.b_rest);
    for (unsigned int i = 0U; i < 15U; i++)
    {
        TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    }
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(6, out.semitone);

    // A reset loads the register again
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(1, out.step);
    in.b_reset = true;
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(6, out.semitone);
}



static void test_modes_three_to_five_are_the_chance_cartesian_and_euclidean_engines(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    // Chance, shuffling: the steps the generator's seed picks (see
    // test_engine_chance.c), which a reset does not start again
    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x23U;
    TEST_ASSERT_EQUAL(7, next_step(&state, &in));
    TEST_ASSERT_EQUAL(6, next_step(&state, &in));
    in.b_reset = true;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
    in.b_reset = false;

    // Cartesian, with y moving on every step: the diagonal
    seq_init(&state);
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x64U;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
    TEST_ASSERT_EQUAL(5, next_step(&state, &in));
    TEST_ASSERT_EQUAL(10, next_step(&state, &in));

    // Euclidean, one beat to the bar: step 0 sounds and step 1 is a rest
    seq_init(&state);
    (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT);
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x05U;
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT(!out.b_rest);
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT(out.b_rest);
}



static void test_the_engine_is_chosen_from_the_last_readable_panel(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    set_step(&in, 8U, 0x05U);
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x01U;
    in.b_steps_valid = false;
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(0, out.step); // The stage engine's second step of stage 0
    TEST_ASSERT(!out.b_note_valid);
}



static void test_a_change_of_engine_starts_its_pattern_from_the_beginning(void)
{
    seq_state_t  state;
    seq_inputs_t in = make_counting_inputs();

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
    TEST_ASSERT_EQUAL(1, next_step(&state, &in));
    TEST_ASSERT_EQUAL(2, next_step(&state, &in));

    // A switch that chooses nothing new changes nothing
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x06U;
    TEST_ASSERT_EQUAL(3, next_step(&state, &in));

    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x01U;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
    TEST_ASSERT_EQUAL(1, next_step(&state, &in));

    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x00U;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
    TEST_ASSERT_EQUAL(1, next_step(&state, &in));

    // And the stage engine was sent back too, not left where it was
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x01U;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
}



static void test_reset_sends_the_chosen_engine_back(void)
{
    seq_state_t  state;
    seq_inputs_t in = make_counting_inputs();

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    set_step(&in, 9U, 0x07U); // Stage 1 lasts four steps
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x01U;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
    TEST_ASSERT_EQUAL(1, next_step(&state, &in));
    TEST_ASSERT_EQUAL(1, next_step(&state, &in));

    in.b_reset = true;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
    in.b_reset = false;
    TEST_ASSERT_EQUAL(1, next_step(&state, &in));
}



static void test_a_tied_note_is_held_into_the_next_step(void)
{
    // 150 BPM: a step is 100 ticks. Stage 0 holds its note for three steps
    // and the other stages are rests, so one cycle is ten steps
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    set_step(&in, 0U, 0x05U);
    set_step(&in, 8U, 0x0AU);
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x01U;
    run_and_trace(&state, &in, 1000U, &gate, &clock_out);

    // Two whole steps and the half step of the gate length, as one gate;
    // the clock output still pulses on every step
    TEST_ASSERT_EQUAL(1, gate.rises);
    TEST_ASSERT_EQUAL(250, gate.high_ticks);
    TEST_ASSERT_EQUAL(10, clock_out.rises);
}



static void test_a_tie_does_not_shorten_a_longer_gate(void)
{
    // A gate of 30 pulses is longer than a step, so it ties by itself and
    // the engine's tie has nothing to add: stage 0 holds for two steps and
    // the rest that follows ends the gate on the boundary
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw   = TEMPO_RAW_150_BPM;
    in.gate.length = 30U;
    set_step(&in, 0U, 0x05U);
    set_step(&in, 8U, 0x09U);
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x01U;
    run_and_trace(&state, &in, 1U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(30, state.gate.pulses_left);
    run_and_trace(&state, &in, 899U, &gate, &clock_out);

    TEST_ASSERT_EQUAL(1, gate.rises);
    TEST_ASSERT_EQUAL(200, gate.high_ticks);
}



static void test_step_lights_flash_for_the_first_half_of_each_step(void)
{
    // 150 BPM: a step is 100 ticks and the clock output is high for 50
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    TEST_ASSERT_EQUAL(0, state.leds);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    for (unsigned int step = 0U; step < 18U; step++)
    {
        for (unsigned int tick = 0U; tick < 100U; tick++)
        {
            (void)memset(&out, POISON_BYTE, sizeof(out));
            seq_tick(&state, &in, &out);
            // Step 0 is a rest and is lit all the same
            TEST_ASSERT_EQUAL((tick < 50U) ? (1U << (step % 16U)) : 0U, out.leds);
        }
    }
}



static void test_step_lights_show_nothing_for_a_step_that_does_not_begin(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    // A one-shot of two steps: the clock output goes on, the lights do not
    seq_init(&state);
    in.tempo_raw          = TEMPO_RAW_150_BPM;
    in.address.last_step  = 1U;
    in.address.b_one_shot = true;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(0x0001, out.leds);
    for (unsigned int tick = 0U; tick < 100U; tick++)
    {
        seq_tick(&state, &in, &out);
    }
    TEST_ASSERT_EQUAL(0x0002, out.leds);
    for (unsigned int tick = 0U; tick < 100U; tick++)
    {
        seq_tick(&state, &in, &out);
    }
    TEST_ASSERT(out.b_clock_out);
    TEST_ASSERT_EQUAL(0, out.leds);

    // Stopped: nothing, and a start begins with nothing left over
    seq_init(&state);
    in.address.b_one_shot = false;
    seq_tick(&state, &in, &out);
    in.b_run_button = true;
    for (unsigned int tick = 0U; tick < 20U; tick++)
    {
        seq_tick(&state, &in, &out);
    }
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);
    TEST_ASSERT_EQUAL(0, out.leds);
}



static void test_step_lights_show_the_klee_register(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    // Stages 0 and 2 loaded: both lit, then stages 1 and 3
    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    set_step(&in, 0U, 0x0BU);
    set_step(&in, 2U, 0x0CU);
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x02U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(0x0005, out.leds);
    for (unsigned int tick = 0U; tick < 100U; tick++)
    {
        seq_tick(&state, &in, &out);
    }
    TEST_ASSERT_EQUAL(0x000A, out.leds);
}



/**
 * @brief Runs single ticks and returns the clock pulses that fell. The
 *        outputs are poisoned before each tick, so a tick that did not give
 *        its pulses shows; they are left as those of the last tick.
 */
static unsigned int pulses_over(seq_state_t *p_state, const seq_inputs_t *p_in,
                                seq_outputs_t *p_out, unsigned int ticks)
{
    unsigned int pulses = 0U;

    for (unsigned int tick = 0U; tick < ticks; tick++)
    {
        (void)memset(p_out, POISON_BYTE, sizeof(*p_out));
        seq_tick(p_state, p_in, p_out);
        pulses += p_out->pulses;
    }
    return pulses;
}



static void test_pulses_are_24_to_a_step_on_the_internal_clock(void)
{
    static const uint16_t tempo_raw[] =
    {
        TEMPO_RAW_30_BPM, TEMPO_RAW_60_BPM, TEMPO_RAW_120_BPM, TEMPO_RAW_150_BPM,
        TEMPO_RAW_250_BPM, TEMPO_RAW_MAX
    };

    for (size_t i = 0U; i < (sizeof(tempo_raw) / sizeof(tempo_raw[0])); i++)
    {
        seq_state_t   state;
        seq_inputs_t  in = make_valid_inputs();
        seq_outputs_t out;

        seq_init(&state);
        in.tempo_raw = tempo_raw[i];

        // Step 0 begins on the first tick, whose time is not counted
        TEST_ASSERT_EQUAL(0, pulses_over(&state, &in, &out, 1U));
        TEST_ASSERT(out.b_step_started);

        // From there, the tick a step begins on is the one its 24th pulse
        // fell on, and no tick holds two
        for (unsigned int step = 1U; step < 40U; step++)
        {
            unsigned int pulses = 0U;

            do
            {
                const unsigned int fell = pulses_over(&state, &in, &out, 1U);

                TEST_ASSERT(fell <= 1U);
                pulses += fell;
            } while ((!out.b_step_started) && (pulses <= SEQ_PULSES_PER_STEP));
            TEST_ASSERT(out.b_step_started);
            TEST_ASSERT_EQUAL(SEQ_PULSES_PER_STEP, pulses);
        }
    }
}



static void test_pulses_of_a_stall_are_given_on_the_tick_they_fell(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_MAX;
    seq_tick(&state, &in, &out); // Step 0

    // 255 ticks at 285 BPM is 116 pulses, all on this tick, though only
    // the first of the steps they bring due begins on it
    in.elapsed_ticks = 255U;
    (void)memset(&out, POISON_BYTE, sizeof(out));
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(116, out.pulses);
    TEST_ASSERT_EQUAL(1, out.step);

    // The steps owed begin on the next ticks without any pulses of their
    // own: these three ticks hold one between them
    in.elapsed_ticks = 1U;
    TEST_ASSERT_EQUAL(1, pulses_over(&state, &in, &out, 3U));
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(4, out.step);

    // Handed over ten at a time, 150 BPM is 12 pulses every fifth call
    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_150_BPM;
    in.elapsed_ticks = 10U;
    TEST_ASSERT_EQUAL(0, pulses_over(&state, &in, &out, 1U));
    TEST_ASSERT_EQUAL(12, pulses_over(&state, &in, &out, 5U));
    TEST_ASSERT_EQUAL(12, pulses_over(&state, &in, &out, 5U));
    TEST_ASSERT(out.b_step_started);
}



static void test_no_time_is_counted_into_the_pulses_of_a_start(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    // The first tick, however late it is
    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_MAX;
    in.elapsed_ticks = 250U;
    TEST_ASSERT_EQUAL(0, pulses_over(&state, &in, &out, 1U));
    TEST_ASSERT(out.b_step_started);

    // And a start by the button
    in.elapsed_ticks = 1U;
    (void)run_ticks(&state, &in, &out, 10U);
    press_run(&state, &in, &out);
    (void)run_ticks(&state, &in, &out, SETTLE_TICKS);
    in.b_run_button  = true;
    in.elapsed_ticks = 250U;
    TEST_ASSERT_EQUAL(0, pulses_over(&state, &in, &out, 1U));
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, out.transport);

    // The tick after it counts in full: 250 ticks at 285 BPM is 114 pulses
    in.b_run_button = false;
    TEST_ASSERT_EQUAL(114, pulses_over(&state, &in, &out, 1U));
}



static void test_pulses_between_sync_edges_are_24_times_the_multiply(void)
{
    static const struct
    {
        uint16_t     tempo_raw;
        unsigned int edge_gap; // Ticks from one edge to the next
        unsigned int per_sync; // Pulses to every 100 ticks, once measured
    } cases[] =
    {
        { TEMPO_RAW_ONE_TO_ONE, 100U, 24U },
        { TEMPO_RAW_MULTIPLY_4, 100U, 96U },
        { TEMPO_RAW_DIVIDE_4,   25U,  24U }
    };

    for (size_t i = 0U; i < (sizeof(cases) / sizeof(cases[0])); i++)
    {
        seq_state_t   state;
        seq_inputs_t  in = make_valid_inputs();
        seq_outputs_t out;
        unsigned int  pulses = 0U;

        seq_init(&state);
        in.tempo_raw     = cases[i].tempo_raw;
        wait_for_external(&state);

        // A start with an edge on it: the one pulse the step was short of
        in.b_clock_edge = true;
        TEST_ASSERT_EQUAL(1, pulses_over(&state, &in, &out, 1U));
        TEST_ASSERT(out.b_step_started);

        // A sync edge every 100 ticks. The first stretch has no spacing to
        // multiply into and is one step; each one after is exact
        for (unsigned int tick = 1U; tick <= 500U; tick++)
        {
            in.b_clock_edge = (0U == (tick % cases[i].edge_gap));
            pulses         += pulses_over(&state, &in, &out, 1U);
            if (0U == (tick % 100U))
            {
                TEST_ASSERT_EQUAL((100U == tick) ? 24U : cases[i].per_sync, pulses);
                pulses = 0U;
            }
        }
    }
}



static void test_pulses_of_an_early_edge_fall_on_its_tick_not_its_steps(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_MULTIPLY_4;
    wait_for_external(&state);
    edge_tick(&state, &in, &out);
    (void)run_ticks(&state, &in, &out, 99U);
    edge_tick(&state, &in, &out); // Four steps, 96 pulses, to an edge from here

    // The next edge comes after 10 ticks, not 100. Eight pulses had fallen;
    // the edge gives the other 88 at once, on a tick whose step is held back
    TEST_ASSERT_EQUAL(8, pulses_over(&state, &in, &out, 9U));
    in.b_clock_edge = true;
    TEST_ASSERT_EQUAL(88, pulses_over(&state, &in, &out, 1U));
    in.b_clock_edge = false;
    TEST_ASSERT(!out.b_step_started);
    TEST_ASSERT(!out.b_clock_out);
}



static void test_pulses_carry_on_at_the_tempo_while_stopped(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;
    uint16_t      step_pulses;
    uint16_t      cv;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    seq_tick(&state, &in, &out);

    // 150 BPM is 6 pulses every 25 ticks, and a stop on the way loses none
    TEST_ASSERT_EQUAL(120, pulses_over(&state, &in, &out, 500U));
    in.b_run_button = true;
    TEST_ASSERT_EQUAL(6, pulses_over(&state, &in, &out, 25U));
    in.b_run_button = false;
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);
    step_pulses = state.step_pulses;
    cv          = state.cv;

    for (unsigned int count = 0U; count < 39U; count++)
    {
        TEST_ASSERT_EQUAL(6, pulses_over(&state, &in, &out, 25U));
        TEST_ASSERT(!out.b_step_started);
        TEST_ASSERT(!out.b_gate);
        TEST_ASSERT(!out.b_clock_out);
        TEST_ASSERT_EQUAL(0, out.leds);
    }

    // The tempo is followed while stopped: 250 BPM is 2 pulses every 5 ticks
    in.tempo_raw = TEMPO_RAW_250_BPM;
    TEST_ASSERT_EQUAL(400, pulses_over(&state, &in, &out, 1000U));

    // And so is the time, however it is handed over
    in.elapsed_ticks = 10U;
    TEST_ASSERT_EQUAL(400, pulses_over(&state, &in, &out, 100U));

    // A tempo reading that fails keeps the last one
    in.b_tempo_valid = false;
    in.tempo_raw     = TEMPO_RAW_30_BPM;
    TEST_ASSERT_EQUAL(400, pulses_over(&state, &in, &out, 100U));

    // Only the clock moved: the pattern is where the stop left it
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);
    TEST_ASSERT_EQUAL(step_pulses, state.step_pulses);
    TEST_ASSERT_EQUAL(cv, state.cv);
    TEST_ASSERT_EQUAL(cv, out.cv);
    TEST_ASSERT_EQUAL(6, state.plain.address.position); // Step 5 was the last
    TEST_ASSERT_EQUAL(0, state.gate.pulses_left);
}



static void test_a_start_after_pulses_while_stopped_is_still_a_whole_step(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    TEST_ASSERT_EQUAL(5, run_ticks(&state, &in, &out, 437U)); // Into step 4
    press_run(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);

    // Stopped for a time that leaves the clock part of the way to a pulse
    TEST_ASSERT(pulses_over(&state, &in, &out, 333U) > 0U);
    TEST_ASSERT(0U != state.clock.phase);

    // The start puts the clock back: nothing of that is left in the pattern
    in.b_run_button = true;
    (void)run_ticks(&state, &in, &out, SETTLE_TICKS - 1U);
    TEST_ASSERT_EQUAL(0, pulses_over(&state, &in, &out, 1U));
    in.b_run_button = false;
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, out.transport);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT(out.b_clock_out);
    TEST_ASSERT_EQUAL(0, state.clock.phase);
    TEST_ASSERT_EQUAL(0, state.step_pulses);

    // Each step is 100 ticks and 24 pulses, the first included, with the
    // gate of a pitch open for the first 50 of them
    for (unsigned int step = 1U; step <= 3U; step++)
    {
        level_trace_t gate      = { 0U, 0U, out.b_gate };
        level_trace_t clock_out = { 0U, 0U, out.b_clock_out };

        run_and_trace(&state, &in, 99U, &gate, &clock_out);
        TEST_ASSERT_EQUAL((1U == step) ? 0U : 49U, gate.high_ticks);
        TEST_ASSERT_EQUAL(49, clock_out.high_ticks);
        TEST_ASSERT_EQUAL(SEQ_PULSES_PER_STEP - 1U, state.step_pulses);
        TEST_ASSERT_EQUAL(1, pulses_over(&state, &in, &out, 1U));
        TEST_ASSERT(out.b_step_started);
        TEST_ASSERT_EQUAL(step, out.step);
        TEST_ASSERT(out.b_gate);
    }
}



static void test_pulses_while_stopped_stop_at_an_external_edge(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;
    clock_state_t clock;

    // Two edges 100 ticks apart, so the clock is following when it stops
    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_ONE_TO_ONE;
    wait_for_external(&state);
    edge_tick(&state, &in, &out);
    (void)run_ticks(&state, &in, &out, 99U);
    edge_tick(&state, &in, &out);
    (void)run_ticks(&state, &in, &out, 10U);
    press_run(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);

    // The stop forgot the external clock, so the internal one carries on.
    // As a tempo the reading is 155 BPM: 31 pulses every 125 ticks
    TEST_ASSERT(!state.clock.b_synced);
    (void)pulses_over(&state, &in, &out, 1000U - SETTLE_TICKS);
    TEST_ASSERT_EQUAL(248, pulses_over(&state, &in, &out, 1000U));
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);

    // From the first edge nothing falls, edges or not, and the clock is
    // not touched
    clock           = state.clock;
    in.b_clock_edge = true;
    TEST_ASSERT_EQUAL(0, pulses_over(&state, &in, &out, 300U));
    in.b_clock_edge = false;
    TEST_ASSERT_EQUAL(0, pulses_over(&state, &in, &out, 300U));
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);
    TEST_ASSERT_EQUAL(clock.phase, state.clock.phase);
    TEST_ASSERT_EQUAL(clock.b_synced, state.clock.b_synced);
    TEST_ASSERT_EQUAL(clock.sync_ticks, state.clock.sync_ticks);

    // A start on the external clock is as it ever was: a pulse short of
    // the first step, which the first edge brings
    press_run(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, out.transport);
    TEST_ASSERT(!out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.pulses);
    TEST_ASSERT_EQUAL(0, pulses_over(&state, &in, &out, 300U));
    TEST_ASSERT(!out.b_step_started);
    in.b_clock_edge = true;
    TEST_ASSERT_EQUAL(1, pulses_over(&state, &in, &out, 1U));
    in.b_clock_edge = false;
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.step);
}



static void test_null_pointers_are_ignored(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(NULL); // Must not crash

    seq_init(&state);
    in.tempo_raw = 400U;
    (void)memset(&out, POISON_BYTE, sizeof(out));

    seq_tick(NULL, &in, &out);
    TEST_ASSERT_EQUAL(POISON_BYTE, out.semitone); // Outputs untouched
    TEST_ASSERT_EQUAL(POISON_BYTE, out.pulses);

    seq_tick(&state, NULL, &out);
    TEST_ASSERT_EQUAL(POISON_BYTE, out.semitone);
    TEST_ASSERT_EQUAL(POISON_BYTE, out.pulses);

    in.raw_steps[0] = 0x10U; // A pitch that would open the gate
    seq_tick(&state, &in, NULL);
    TEST_ASSERT_EQUAL(0, state.gate.pulses_left);
    TEST_ASSERT_EQUAL(0, state.cv);
    TEST_ASSERT_EQUAL(0, state.last_tempo_raw); // State untouched
    TEST_ASSERT_EQUAL(0, state.plain.address.position);
    TEST_ASSERT_EQUAL(0, state.clock.phase);
    TEST_ASSERT_EQUAL(SEQ_PULSES_PER_STEP, state.step_pulses);
}



int main(void)
{
    RUN_TEST(test_a_step_is_a_sixteenth_note);
    RUN_TEST(test_init_sets_the_power_on_state);
    RUN_TEST(test_first_tick_begins_step_zero);
    RUN_TEST(test_first_tick_begins_step_zero_with_no_time_elapsed);
    RUN_TEST(test_ticks_between_steps_report_nothing);
    RUN_TEST(test_step_length_follows_the_tempo);
    RUN_TEST(test_readings_within_one_bpm_give_the_same_tempo);
    RUN_TEST(test_fastest_tempo_is_285_bpm);
    RUN_TEST(test_steps_follow_in_order_and_wrap);
    RUN_TEST(test_note_comes_from_the_tick_its_step_begins_on);
    RUN_TEST(test_step_begins_on_time_when_steps_are_invalid);
    RUN_TEST(test_notes_decode_high_nibble_first);
    RUN_TEST(test_each_step_is_independent);
    RUN_TEST(test_tempo_is_the_minimum_before_any_valid_reading);
    RUN_TEST(test_invalid_tempo_reuses_last_valid);
    RUN_TEST(test_steps_and_tempo_validity_are_independent);
    RUN_TEST(test_several_ticks_at_once_count_in_full);
    RUN_TEST(test_steps_owed_after_a_stall_begin_one_per_tick);
    RUN_TEST(test_tick_does_not_change_inputs);
    RUN_TEST(test_note_value_zero_is_a_rest);
    RUN_TEST(test_scale_and_root_set_the_pitch);
    RUN_TEST(test_addressing_controls_set_the_order_of_steps);
    RUN_TEST(test_one_shot_stops_after_one_cycle_and_the_clock_runs_on);
    RUN_TEST(test_reset_goes_back_to_the_first_step_without_moving_the_clock);
    RUN_TEST(test_reset_held_keeps_the_pattern_on_its_first_step);
    RUN_TEST(test_reset_plays_a_finished_one_shot_again);
    RUN_TEST(test_gate_opens_with_a_pitch_for_its_length);
    RUN_TEST(test_gate_length_is_counted_in_pulses);
    RUN_TEST(test_gate_stays_closed_for_a_rest_and_an_unreadable_step);
    RUN_TEST(test_a_whole_step_gate_ties_notes_and_a_rest_ends_it);
    RUN_TEST(test_gate_length_is_read_when_the_step_begins);
    RUN_TEST(test_gate_of_a_late_step_counts_from_when_it_was_due);
    RUN_TEST(test_clock_out_is_high_for_the_first_half_of_every_step);
    RUN_TEST(test_clock_out_runs_on_when_a_one_shot_has_finished);
    RUN_TEST(test_reset_does_not_cut_a_held_note);
    RUN_TEST(test_cv_is_set_by_each_note_on_the_tick_its_gate_opens);
    RUN_TEST(test_cv_is_given_on_every_tick_and_holds_after_the_gate_closes);
    RUN_TEST(test_cv_is_zero_until_the_first_note);
    RUN_TEST(test_cv_holds_through_a_rest_an_unreadable_step_and_a_reset);
    RUN_TEST(test_cv_holds_when_a_one_shot_has_finished);
    RUN_TEST(test_calibration_table_is_read_when_the_step_begins);
    RUN_TEST(test_transport_is_given_on_every_tick);
    RUN_TEST(test_run_button_stops_the_pattern_at_once);
    RUN_TEST(test_run_button_starts_again_from_the_first_step);
    RUN_TEST(test_time_on_the_tick_of_a_start_is_not_counted);
    RUN_TEST(test_a_stopped_pattern_ignores_reset_and_the_external_clock);
    RUN_TEST(test_each_external_edge_is_a_step);
    RUN_TEST(test_external_gate_and_clock_out_are_half_the_measured_step);
    RUN_TEST(test_tempo_reading_is_a_ratio_on_the_external_clock);
    RUN_TEST(test_a_multiplied_clock_fits_steps_between_the_edges);
    RUN_TEST(test_steps_owed_after_an_early_edge_begin_one_per_tick);
    RUN_TEST(test_an_early_edge_gets_a_tick_of_low_before_its_step);
    RUN_TEST(test_a_tied_gate_is_not_broken_by_an_edge_on_time);
    RUN_TEST(test_a_divided_clock_passes_over_edges);
    RUN_TEST(test_last_valid_tempo_reading_is_the_ratio_when_one_fails);
    RUN_TEST(test_an_edge_takes_the_pattern_over_without_a_restart);
    RUN_TEST(test_an_edge_while_the_clock_output_is_high_waits_a_tick);
    RUN_TEST(test_a_stop_and_a_start_go_back_to_the_internal_clock);
    RUN_TEST(test_reset_on_the_external_clock_plays_the_first_step_at_the_next_edge);
    RUN_TEST(test_a_start_on_the_external_clock_waits_for_an_edge);
    RUN_TEST(test_mode_switches_choose_the_engine);
    RUN_TEST(test_mode_two_is_the_klee_engine);
    RUN_TEST(test_modes_three_to_five_are_the_chance_cartesian_and_euclidean_engines);
    RUN_TEST(test_the_engine_is_chosen_from_the_last_readable_panel);
    RUN_TEST(test_a_change_of_engine_starts_its_pattern_from_the_beginning);
    RUN_TEST(test_reset_sends_the_chosen_engine_back);
    RUN_TEST(test_a_tied_note_is_held_into_the_next_step);
    RUN_TEST(test_a_tie_does_not_shorten_a_longer_gate);
    RUN_TEST(test_step_lights_flash_for_the_first_half_of_each_step);
    RUN_TEST(test_step_lights_show_nothing_for_a_step_that_does_not_begin);
    RUN_TEST(test_step_lights_show_the_klee_register);
    RUN_TEST(test_pulses_are_24_to_a_step_on_the_internal_clock);
    RUN_TEST(test_pulses_of_a_stall_are_given_on_the_tick_they_fell);
    RUN_TEST(test_no_time_is_counted_into_the_pulses_of_a_start);
    RUN_TEST(test_pulses_between_sync_edges_are_24_times_the_multiply);
    RUN_TEST(test_pulses_of_an_early_edge_fall_on_its_tick_not_its_steps);
    RUN_TEST(test_pulses_carry_on_at_the_tempo_while_stopped);
    RUN_TEST(test_a_start_after_pulses_while_stopped_is_still_a_whole_step);
    RUN_TEST(test_pulses_while_stopped_stop_at_an_external_edge);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
