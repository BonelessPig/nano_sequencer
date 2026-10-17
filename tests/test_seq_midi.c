/**
 * @file   test_seq_midi.c
 * @brief  Host unit tests for the sequencer core (core/seq.c): the MIDI
 *         clock, Start and Stop. What the transport makes of them is tested
 *         in test_transport.c; these check that the sequencer hands them on
 *         and follows the clock the transport chose.
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

/**
 * @brief One tick with an edge of the MIDI clock on it.
 */
static void midi_edge_tick(seq_state_t *p_state, seq_inputs_t *p_in, seq_outputs_t *p_out)
{
    p_in->b_midi_edge = true;
    seq_tick(p_state, p_in, p_out);
    p_in->b_midi_edge = false;
}

/**
 * @brief One tick with a MIDI Start on it.
 */
static void midi_start_tick(seq_state_t *p_state, seq_inputs_t *p_in, seq_outputs_t *p_out)
{
    p_in->b_midi_start = true;
    seq_tick(p_state, p_in, p_out);
    p_in->b_midi_start = false;
}

/**
 * @brief One tick with a MIDI Stop on it.
 */
static void midi_stop_tick(seq_state_t *p_state, seq_inputs_t *p_in, seq_outputs_t *p_out)
{
    p_in->b_midi_stop = true;
    seq_tick(p_state, p_in, p_out);
    p_in->b_midi_stop = false;
}



static void test_a_midi_edge_takes_the_pattern_over_and_each_is_a_step(void)
{
    static const unsigned int gaps[] = { 100U, 60U, 250U, 130U };
    seq_state_t               state;
    seq_inputs_t              in = make_counting_inputs();
    seq_outputs_t             out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_ONE_TO_ONE;

    // Step 0 on the internal clock, and its clock pulse over
    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, out.transport);
    (void)run_ticks(&state, &in, &out, 60U);

    // The first edge takes it over with no restart: step 1 begins there
    midi_edge_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, out.transport);
    TEST_ASSERT(out.b_transport_changed);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(1, out.step);

    for (size_t i = 0U; i < (sizeof(gaps) / sizeof(gaps[0])); i++)
    {
        TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, gaps[i] - 1U));
        midi_edge_tick(&state, &in, &out);
        TEST_ASSERT(out.b_step_started);
        TEST_ASSERT_EQUAL(i + 2U, out.step);
    }

    // When the clocks stop, so does the pattern
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 5000U));
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, out.transport);
}



static void test_the_clock_that_was_not_first_moves_nothing(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    // MIDI first: an edge on the clock input begins no step
    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_ONE_TO_ONE;
    seq_tick(&state, &in, &out);
    (void)run_ticks(&state, &in, &out, 60U);
    midi_edge_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(1, out.step);
    (void)run_ticks(&state, &in, &out, 99U);
    edge_tick(&state, &in, &out);
    TEST_ASSERT(!out.b_step_started);
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 99U));
    midi_edge_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(2, out.step);

    // The clock input first: a MIDI edge begins none
    seq_init(&state);
    seq_tick(&state, &in, &out);
    (void)run_ticks(&state, &in, &out, 60U);
    edge_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(1, out.step);
    (void)run_ticks(&state, &in, &out, 99U);
    midi_edge_tick(&state, &in, &out);
    TEST_ASSERT(!out.b_step_started);
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 99U));
    edge_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(2, out.step);
}



static void test_a_midi_stop_stops_the_pattern_at_once(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    (void)run_ticks(&state, &in, &out, 110U); // Into step 1, its gate open
    TEST_ASSERT(out.b_gate);

    midi_stop_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);
    TEST_ASSERT(out.b_transport_changed);
    TEST_ASSERT(!out.b_gate);
    TEST_ASSERT(!out.b_clock_out);
    TEST_ASSERT(!out.b_step_started);

    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 1000U));
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);
}



static void test_a_midi_start_plays_the_first_step_on_the_next_midi_edge(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_ONE_TO_ONE;
    (void)run_ticks(&state, &in, &out, 250U); // Steps 0 to 2 at the tempo

    // Running or not, a Start goes back to the beginning and waits
    midi_start_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, out.transport);
    TEST_ASSERT(!out.b_step_started);
    TEST_ASSERT(!out.b_gate);
    TEST_ASSERT(!out.b_clock_out);
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 500U));

    // An edge on the clock input is not what it waits for
    edge_tick(&state, &in, &out);
    TEST_ASSERT(!out.b_step_started);

    midi_edge_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 99U));
    midi_edge_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(1, out.step);

    // And the same from stopped
    midi_stop_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, &out, 50U));
    midi_start_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, out.transport);
    TEST_ASSERT(!out.b_step_started);
    midi_edge_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.step);
}



static void test_a_clock_on_the_tick_of_a_start_begins_the_first_step_there(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_ONE_TO_ONE;
    (void)run_ticks(&state, &in, &out, 250U);

    in.b_midi_start = true;
    in.b_midi_edge  = true;
    seq_tick(&state, &in, &out);
    in.b_midi_start = false;
    in.b_midi_edge  = false;

    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, out.transport);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT(out.b_clock_out);
}



int main(void)
{
    RUN_TEST(test_a_midi_edge_takes_the_pattern_over_and_each_is_a_step);
    RUN_TEST(test_the_clock_that_was_not_first_moves_nothing);
    RUN_TEST(test_a_midi_stop_stops_the_pattern_at_once);
    RUN_TEST(test_a_midi_start_plays_the_first_step_on_the_next_midi_edge);
    RUN_TEST(test_a_clock_on_the_tick_of_a_start_begins_the_first_step_there);
    return TEST_RESULT();
}
