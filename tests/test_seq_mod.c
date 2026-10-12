/**
 * @file   test_seq_mod.c
 * @brief  Host unit tests for the sequencer core (core/seq.c): what the
 *         LFO moves.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "seq.h"
#include "test_harness.h"
#include "seq_fixture.h"

#define LFO_CENTRE (512U)  // Where the LFO moves nothing
#define LFO_TOP    (1023U)
#define LFO_HALF_UP (768U) // A depth of 128 at full amount
#define AMOUNT_TOP (1023U)

/**
 * @brief Inputs with the note value 5 on every step, 100 ticks to a step,
 *        and the LFO readable at its centre with every amount at 0.
 */
static seq_inputs_t make_mod_inputs(void)
{
    seq_inputs_t in = make_valid_inputs();

    (void)memset(in.raw_steps, 0x55, PANEL_STEP_BYTE_COUNT);
    in.tempo_raw       = TEMPO_RAW_150_BPM;
    in.mod.lfo         = LFO_CENTRE;
    in.mod.b_lfo_valid = true;
    return in;
}



static void test_the_lfo_moves_the_note_in_the_scale(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_mod_inputs();
    seq_outputs_t out;

    // Chromatic from the lowest pitch: note value n is semitone n - 1. At
    // its top the LFO takes the 5 up seven places
    seq_init(&state);
    in.mod.transpose = AMOUNT_TOP;
    in.mod.lfo       = LFO_TOP;
    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT(!out.b_rest);
    TEST_ASSERT_EQUAL(11, out.semitone);
    TEST_ASSERT_EQUAL(pitch_cal_cv(&in.pitch_cal, 11U), out.cv);

    // At its bottom, down seven: stopped at the lowest note, never a rest
    in.mod.lfo = 0U;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT(!out.b_rest);
    TEST_ASSERT_EQUAL(0, out.semitone);

    // Half way up is four places
    in.mod.lfo = LFO_HALF_UP;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(8, out.semitone);

    // At its centre, with no amount, and when it cannot be read: as set
    in.mod.lfo = LFO_CENTRE;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(4, out.semitone);

    in.mod.lfo       = LFO_TOP;
    in.mod.transpose = 0U;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(4, out.semitone);

    in.mod.transpose   = AMOUNT_TOP;
    in.mod.b_lfo_valid = false;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(4, out.semitone);
}



static void test_the_lfo_stops_at_the_top_note_and_leaves_a_rest_a_rest(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_mod_inputs();
    seq_outputs_t out;

    // Step 0 holds 12 and step 1 nothing
    in.raw_steps[0]  = 0xC0U;
    in.mod.transpose = AMOUNT_TOP;
    in.mod.lfo       = LFO_TOP;

    seq_init(&state);
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(14, out.semitone); // Note value 15
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT(out.b_rest);
}



static void test_the_lfo_moves_the_gate_length(void)
{
    static const struct
    {
        uint16_t lfo;
        bool     b_lfo_valid;
        uint16_t amount;
        uint8_t  length;   // The gate length control
        uint8_t  expected; // Pulses the gate is opened for
    } cases[] =
    {
        { LFO_TOP,     true,  AMOUNT_TOP, 12U, 24U  }, // Half a step more: tied
        { 0U,          true,  AMOUNT_TOP, 12U, 1U   }, // Half a step less: the least
        { LFO_HALF_UP, true,  AMOUNT_TOP, 12U, 18U  },
        { LFO_TOP,     true,  AMOUNT_TOP, 20U, 24U  }, // No more than a step
        { LFO_CENTRE,  true,  AMOUNT_TOP, 12U, 12U  },
        { LFO_TOP,     true,  0U,         12U, 12U  },
        { LFO_TOP,     false, AMOUNT_TOP, 12U, 12U  },
        { LFO_TOP,     true,  0U,         200U, 200U } // Not moved, so not limited
    };

    for (size_t i = 0U; i < (sizeof(cases) / sizeof(cases[0])); i++)
    {
        seq_state_t   state;
        seq_inputs_t  in = make_mod_inputs();
        seq_outputs_t out;

        in.mod.lfo         = cases[i].lfo;
        in.mod.b_lfo_valid = cases[i].b_lfo_valid;
        in.mod.gate_length = cases[i].amount;
        in.gate.length     = cases[i].length;

        seq_init(&state);
        seq_tick(&state, &in, &out);
        TEST_ASSERT(out.b_gate);
        TEST_ASSERT_EQUAL(cases[i].expected, state.gate.pulses_left);
    }
}



static void test_the_lfo_makes_steps_drop_out_by_chance(void)
{
    // The random numbers begin 135, 35, 70, 220, 176, 221, 238, 248 (see
    // test_prng.c), the plain engine draws none, and at a depth of 128 a
    // step drops out on a draw above 127
    static const bool dropped[] = { true, false, false, true, true, true, true, true };
    seq_state_t       state;
    seq_inputs_t      in = make_mod_inputs();
    seq_outputs_t     out;

    in.mod.chance = AMOUNT_TOP;
    in.mod.lfo    = LFO_HALF_UP;

    seq_init(&state);
    for (size_t i = 0U; i < (sizeof(dropped) / sizeof(dropped[0])); i++)
    {
        TEST_ASSERT(ticks_to_next_step(&state, &in, &out) > 0U);
        TEST_ASSERT_EQUAL(i, out.step);
        TEST_ASSERT(out.b_note_valid);
        TEST_ASSERT_EQUAL(dropped[i], out.b_rest);
        TEST_ASSERT_EQUAL(!dropped[i], out.b_gate);
    }
}



static void test_no_chance_leaves_the_random_numbers_alone(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_mod_inputs();
    seq_outputs_t out;

    // The LFO below its centre: no step drops out and nothing is drawn
    in.mod.chance = AMOUNT_TOP;
    in.mod.lfo    = 0U;
    seq_init(&state);
    for (unsigned int step = 0U; step < 4U; step++)
    {
        (void)ticks_to_next_step(&state, &in, &out);
        TEST_ASSERT(!out.b_rest);
    }
    TEST_ASSERT_EQUAL(0xACE1, state.prng.value);

    // Steps that are rests already: nothing to drop, so nothing is drawn
    in.mod.lfo = LFO_TOP;
    (void)memset(in.raw_steps, 0, PANEL_STEP_BYTE_COUNT);
    for (unsigned int step = 0U; step < 4U; step++)
    {
        (void)ticks_to_next_step(&state, &in, &out);
        TEST_ASSERT(out.b_rest);
    }
    TEST_ASSERT_EQUAL(0xACE1, state.prng.value);
}



static void test_the_lfo_moves_the_step_played_and_not_the_pattern(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    // Step n holds the note value n, so plays the pitch n - 1. Half way up
    // the LFO is four of the sixteen places on: step 0 plays as step 4
    in.tempo_raw       = TEMPO_RAW_150_BPM;
    in.mod.b_lfo_valid = true;
    in.mod.position    = AMOUNT_TOP;
    in.mod.lfo         = LFO_HALF_UP;
    seq_init(&state);
    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(4, out.step);
    TEST_ASSERT_EQUAL(3, out.semitone);
    TEST_ASSERT_EQUAL(0x0010, out.leds);

    // At its bottom, eight back: step 1 plays as step 9
    in.mod.lfo = 0U;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(9, out.step);
    TEST_ASSERT_EQUAL(8, out.semitone);

    // At its centre, with no amount, and when it cannot be read: the step
    // the pattern has got to, which the offsets before did not move
    in.mod.lfo = LFO_CENTRE;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(2, out.step);

    in.mod.lfo      = LFO_TOP;
    in.mod.position = 0U;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(3, out.step);

    in.mod.position    = AMOUNT_TOP;
    in.mod.b_lfo_valid = false;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(4, out.step);

    // A depth of 160 below the centre is five back: step 5 plays as step
    // 0, which is a rest
    in.mod.b_lfo_valid = true;
    in.mod.lfo         = 192U;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT(out.b_rest);
    TEST_ASSERT(!out.b_gate);
}



static void test_the_lfo_turns_the_klee_register_and_its_lights(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    // The Klee engine, one bit loaded at stage 0, and stage 4 holding the
    // value 3. Four stages on, the bit sums as stage 4 and lights it
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x02U;
    in.raw_steps[0]    = 0x80U;
    in.raw_steps[2]    = 0x30U;
    in.tempo_raw       = TEMPO_RAW_150_BPM;
    in.mod.b_lfo_valid = true;
    in.mod.position    = AMOUNT_TOP;
    in.mod.lfo         = LFO_HALF_UP;
    seq_init(&state);
    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(2, out.semitone);
    TEST_ASSERT_EQUAL(0x0010, out.leds);
    TEST_ASSERT_EQUAL(0x0001, state.klee.stages);
}



static void test_the_lfo_below_its_centre_brings_the_amplitude_down(void)
{
    static const struct
    {
        uint16_t lfo;
        bool     b_lfo_valid;
        uint16_t amount;
        uint8_t  amplitude;
    } cases[] =
    {
        { LFO_CENTRE,  true,  AMOUNT_TOP, 255U }, // Nothing taken off
        { 0U,          true,  AMOUNT_TOP, 0U   }, // All of it
        { 256U,        true,  AMOUNT_TOP, 127U }, // A depth of 128
        { LFO_TOP,     true,  AMOUNT_TOP, 255U }, // Above the centre: none
        { LFO_HALF_UP, true,  AMOUNT_TOP, 255U },
        { 0U,          true,  0U,         255U }, // No amount
        { 0U,          false, AMOUNT_TOP, 255U }  // Not readable
    };

    for (size_t i = 0U; i < (sizeof(cases) / sizeof(cases[0])); i++)
    {
        seq_state_t   state;
        seq_inputs_t  in = make_mod_inputs();
        seq_outputs_t out;

        in.mod.lfo         = cases[i].lfo;
        in.mod.b_lfo_valid = cases[i].b_lfo_valid;
        in.mod.amplitude   = cases[i].amount;

        seq_init(&state);
        seq_tick(&state, &in, &out);
        TEST_ASSERT(out.b_step_started);
        TEST_ASSERT_EQUAL(cases[i].amplitude, out.amplitude);
        TEST_ASSERT_EQUAL(4, out.semitone); // And nothing else is moved

        // Given with the step, and 0 on the ticks between
        seq_tick(&state, &in, &out);
        TEST_ASSERT(!out.b_step_started);
        TEST_ASSERT_EQUAL(0, out.amplitude);
    }
}



static void test_the_lfo_moves_the_pitch_of_a_note_on_every_tick(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_mod_inputs();
    seq_outputs_t out;

    // Every step plays semitone 4, which the table puts at 333; two
    // semitones up is 500 and two down 167. At its centre nothing moves
    seq_init(&state);
    in.mod.vibrato = AMOUNT_TOP;
    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(333, out.cv);

    // Between steps the LFO is followed tick by tick: to the top, half way
    // up (84 of the 167), and to the bottom
    in.mod.lfo = LFO_TOP;
    seq_tick(&state, &in, &out);
    TEST_ASSERT(!out.b_step_started);
    TEST_ASSERT_EQUAL(500, out.cv);
    in.mod.lfo = LFO_HALF_UP;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(417, out.cv);
    in.mod.lfo = 0U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(167, out.cv);

    // The note itself is not moved: the next step is the pitch it was
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(4, out.semitone);
    TEST_ASSERT_EQUAL(167, out.cv);

    // With no amount, and when the LFO cannot be read: as the note set it
    in.mod.vibrato = 0U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(333, out.cv);
    in.mod.vibrato     = AMOUNT_TOP;
    in.mod.b_lfo_valid = false;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(333, out.cv);
}



static void test_the_vibrato_goes_no_lower_than_the_lowest_pitch(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_mod_inputs();
    seq_outputs_t out;

    // Step 0 plays semitone 1, at 83, and step 1 semitone 0, at 0
    in.raw_steps[0] = 0x21U;
    in.mod.vibrato  = AMOUNT_TOP;
    in.mod.lfo      = 0U;
    seq_init(&state);
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(1, out.semitone);
    TEST_ASSERT_EQUAL(0, out.cv);
    in.mod.lfo = LFO_TOP;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(250, out.cv);

    in.mod.lfo = 0U;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(0, out.semitone);
    TEST_ASSERT_EQUAL(0, out.cv);
    in.mod.lfo = LFO_TOP;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(167, out.cv);
}



static void test_the_vibrato_goes_on_through_a_rest_and_a_stop(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_mod_inputs();
    seq_outputs_t out;

    // Before the first note it is about the lowest pitch
    in.mod.vibrato = AMOUNT_TOP;
    in.mod.lfo     = LFO_TOP;
    (void)memset(in.raw_steps, 0, PANEL_STEP_BYTE_COUNT);
    seq_init(&state);
    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_rest);
    TEST_ASSERT_EQUAL(167, out.cv);

    // Step 1 plays semitone 4, and step 2 is a rest
    in.raw_steps[0] = 0x05U;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(500, out.cv);
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT(out.b_rest);
    TEST_ASSERT_EQUAL(500, out.cv);
    in.mod.lfo = 0U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(167, out.cv);

    // Stopped, it still follows the LFO
    in.b_run_button = false;
    seq_tick(&state, &in, &out);
    press_run(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);
    in.mod.lfo = LFO_HALF_UP;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(417, out.cv);
}



static void test_the_vibrato_follows_a_table_that_falls(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_mod_inputs();
    seq_outputs_t out;

    // A lowest octave that goes down: semitone 4 is at 667 and 6 at 500
    in.pitch_cal.cv[0] = 1000U;
    in.pitch_cal.cv[1] = 0U;
    in.mod.vibrato     = AMOUNT_TOP;
    in.mod.lfo         = LFO_TOP;
    seq_init(&state);
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(4, out.semitone);
    TEST_ASSERT_EQUAL(500, out.cv);
    in.mod.lfo = 0U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(833, out.cv);
}



int main(void)
{
    RUN_TEST(test_the_lfo_moves_the_pitch_of_a_note_on_every_tick);
    RUN_TEST(test_the_vibrato_goes_no_lower_than_the_lowest_pitch);
    RUN_TEST(test_the_vibrato_goes_on_through_a_rest_and_a_stop);
    RUN_TEST(test_the_vibrato_follows_a_table_that_falls);
    RUN_TEST(test_the_lfo_below_its_centre_brings_the_amplitude_down);
    RUN_TEST(test_the_lfo_moves_the_step_played_and_not_the_pattern);
    RUN_TEST(test_the_lfo_turns_the_klee_register_and_its_lights);
    RUN_TEST(test_the_lfo_moves_the_note_in_the_scale);
    RUN_TEST(test_the_lfo_stops_at_the_top_note_and_leaves_a_rest_a_rest);
    RUN_TEST(test_the_lfo_moves_the_gate_length);
    RUN_TEST(test_the_lfo_makes_steps_drop_out_by_chance);
    RUN_TEST(test_no_chance_leaves_the_random_numbers_alone);
    return TEST_RESULT();
}
