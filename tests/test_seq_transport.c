/**
 * @file   test_seq_transport.c
 * @brief  Host unit tests for the sequencer core (core/seq.c): the run
 *         button and the external clock.
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



int main(void)
{
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
    return TEST_RESULT();
}
