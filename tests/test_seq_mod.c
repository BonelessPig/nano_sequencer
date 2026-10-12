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



int main(void)
{
    RUN_TEST(test_the_lfo_moves_the_note_in_the_scale);
    RUN_TEST(test_the_lfo_stops_at_the_top_note_and_leaves_a_rest_a_rest);
    RUN_TEST(test_the_lfo_moves_the_gate_length);
    RUN_TEST(test_the_lfo_makes_steps_drop_out_by_chance);
    RUN_TEST(test_no_chance_leaves_the_random_numbers_alone);
    return TEST_RESULT();
}
