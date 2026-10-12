/**
 * @file   test_seq_pulses.c
 * @brief  Host unit tests for the sequencer core (core/seq.c): the clock
 *         pulses it gives for MIDI.
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



int main(void)
{
    RUN_TEST(test_pulses_are_24_to_a_step_on_the_internal_clock);
    RUN_TEST(test_pulses_of_a_stall_are_given_on_the_tick_they_fell);
    RUN_TEST(test_no_time_is_counted_into_the_pulses_of_a_start);
    RUN_TEST(test_pulses_between_sync_edges_are_24_times_the_multiply);
    RUN_TEST(test_pulses_of_an_early_edge_fall_on_its_tick_not_its_steps);
    RUN_TEST(test_pulses_carry_on_at_the_tempo_while_stopped);
    RUN_TEST(test_a_start_after_pulses_while_stopped_is_still_a_whole_step);
    RUN_TEST(test_pulses_while_stopped_stop_at_an_external_edge);
    return TEST_RESULT();
}
