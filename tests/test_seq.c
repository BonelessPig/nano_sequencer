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

/**
 * @brief Inputs with everything valid, all step bits 0, the tempo control at
 *        0 and one tick elapsed. The pattern is all sixteen steps, forward
 *        and looping, in the chromatic scale from the lowest pitch, so a
 *        step's pitch is its note value less one.
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
    in.b_reset            = false;
    return in;
}



/**
 * @brief Inputs with everything valid and step n holding the note value n:
 *        step 0 is a rest and step n plays the pitch n - 1.
 */
static seq_inputs_t make_counting_inputs(void)
{
    seq_inputs_t in = make_valid_inputs();

    for (uint8_t byte_index = 0U; byte_index < PANEL_RAW_BYTE_COUNT; byte_index++)
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



static void test_a_step_is_a_sixteenth_note(void)
{
    TEST_ASSERT_EQUAL(24, SEQ_PULSES_PER_STEP);
}



static void test_init_sets_the_power_on_state(void)
{
    seq_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    seq_init(&state);
    TEST_ASSERT_EQUAL(0, state.last_tempo_raw);
    TEST_ASSERT_EQUAL(0, state.engine.address.position);
    TEST_ASSERT(!state.engine.address.b_finished);
    TEST_ASSERT_EQUAL(0, state.clock.phase);
    TEST_ASSERT_EQUAL(SEQ_PULSES_PER_STEP, state.step_pulses); // A step is due
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
    (void)memset(in.raw_steps, 0xFF, sizeof(in.raw_steps));
    seq_tick(&state, &in, &out); // Step 0 begins

    (void)memset(&out, POISON_BYTE, sizeof(out));
    seq_tick(&state, &in, &out);
    TEST_ASSERT(!out.b_step_started);
    TEST_ASSERT(!out.b_note_valid);
    TEST_ASSERT(!out.b_rest);
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(0, out.semitone);
    TEST_ASSERT_EQUAL(1, state.engine.address.position); // Step 1 is still to come
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

        // The first tick begins step 0 and already counts towards step 1,
        // so that one step is a tick short
        TEST_ASSERT_EQUAL(1, ticks_to_next_step(&state, &in, &out));
        TEST_ASSERT_EQUAL(expected_ticks[i] - 1U, ticks_to_next_step(&state, &in, &out));

        // Every step after that is exactly 15000 / BPM ticks
        for (unsigned int step = 2U; step < 40U; step++)
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
    for (unsigned int tick = 1U; tick < 10000U; tick++)
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
                          state.engine.address.position);
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
    TEST_ASSERT_EQUAL(59, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT(out.b_note_valid);
    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT_EQUAL(1, out.semitone); // The step still decodes when tempo fails
}



static void test_several_ticks_at_once_count_in_full(void)
{
    // 150 BPM is 100 ticks per step; handed over ten at a time, a step
    // begins on every tenth call
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw     = TEMPO_RAW_150_BPM;
    in.elapsed_ticks = 10U;
    TEST_ASSERT_EQUAL(1, ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(9, ticks_to_next_step(&state, &in, &out)); // First step is short
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
        TEST_ASSERT_EQUAL((0U == i) ? 1U : ((1U == i) ? 99U : 100U), ticks);
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
    (void)ticks_to_next_step(&state, &in, &out); // The first step is a tick short
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

    seq_tick(&state, NULL, &out);
    TEST_ASSERT_EQUAL(POISON_BYTE, out.semitone);

    seq_tick(&state, &in, NULL);
    TEST_ASSERT_EQUAL(0, state.last_tempo_raw); // State untouched
    TEST_ASSERT_EQUAL(0, state.engine.address.position);
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
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
