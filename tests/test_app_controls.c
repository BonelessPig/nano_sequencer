/**
 * @file   test_app_controls.c
 * @brief  Host tests for the application loop (app/app.c) against the fake
 *         ports in adapters/host: the run button, the clock
 *         and reset inputs, the selector and pot, and the level light.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "app.h"
#include "cal_record.h"
#include "host_ports.h"
#include "midi_output_port.h"
#include "seq.h"
#include "test_harness.h"
#include "app_fixture.h"

static void test_the_run_button_stops_and_starts_the_pattern_and_is_logged(void)
{
    start_app();
    run_ticks(10U);
    TEST_ASSERT_EQUAL(1, host_event_count());
    TEST_ASSERT(host_gate_level());

    // The press counts on the twentieth tick and is applied on the next
    host_set_run_button(true);
    run_ticks(SETTLE_TICKS);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT_EQUAL(1, host_event_count());
    run_ticks(1U);
    TEST_ASSERT(!host_gate_level());
    TEST_ASSERT(!host_clock_output_level());
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(1U, HOST_EVENT_TRANSPORT, (uint16_t)TRANSPORT_STOPPED, 0U);

    // Stopped: nothing plays and nothing more is logged
    host_set_run_button(false);
    run_ticks(5U * TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(2, host_event_count());
    TEST_ASSERT_EQUAL(1, host_gate_rise_count());
    TEST_ASSERT_EQUAL(1, host_clock_output_rise_count());

    // The next press starts the pattern from step 0: the transport is logged
    // first, then the step
    host_set_run_button(true);
    run_ticks(SETTLE_TICKS + 1U);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT_EQUAL(4, host_event_count());
    expect_event(2U, HOST_EVENT_TRANSPORT, (uint16_t)TRANSPORT_INTERNAL, 0U);
    expect_event(3U, HOST_EVENT_STEP_NOTE, 0U, 14U);

    // And step 1 follows a whole step later
    host_set_run_button(false);
    run_ticks(TICKS_PER_STEP - 1U);
    TEST_ASSERT_EQUAL(4, host_event_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(5, host_event_count());
    expect_event(4U, HOST_EVENT_STEP_NOTE, 1U, 13U);
}



static void test_an_external_edge_takes_the_pattern_over_from_the_clock_input(void)
{
    start_app();
    host_set_tempo(SYNC_RAW_ONE_TO_ONE, STATUS_OK);

    // The internal clock to begin with: step 0, and its clock pulse over
    run_ticks(60U);
    TEST_ASSERT_EQUAL(1, host_event_count());
    TEST_ASSERT(!host_clock_output_level());

    // An edge is taken on the next tick, whether or not it is a scan tick,
    // and applied on the one after: the change of clock is logged, and the
    // next step begins there
    host_pulse_clock_input();
    run_ticks(1U);
    TEST_ASSERT_EQUAL(1, host_event_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(3, host_event_count());
    expect_event(1U, HOST_EVENT_TRANSPORT, (uint16_t)TRANSPORT_EXTERNAL, 0U);
    expect_event(2U, HOST_EVENT_STEP_NOTE, 1U, 13U);
    TEST_ASSERT_EQUAL(2, host_gate_rise_count());
    run_ticks(35U);

    for (uint16_t step = 2U; step < 5U; step++)
    {
        host_pulse_clock_input();
        run_ticks(1U);
        TEST_ASSERT_EQUAL(step + 1U, host_event_count());
        run_ticks(1U);
        TEST_ASSERT_EQUAL(step + 2U, host_event_count());
        expect_event((uint16_t)(step + 1U), HOST_EVENT_STEP_NOTE, step, (uint16_t)(14U - step));
        TEST_ASSERT_EQUAL(step + 1U, host_gate_rise_count());

        // One edge is one step, however long the wait for the next
        run_ticks(35U + (step * 50U));
        TEST_ASSERT_EQUAL(step + 2U, host_event_count());
    }
}



static void test_the_reset_input_sends_the_pattern_back_to_its_start(void)
{
    start_app();
    run_ticks(250U); // Steps 0 to 2 have begun
    TEST_ASSERT_EQUAL(3, host_event_count());

    // The pattern goes back, and the first step plays where step 3 would have
    host_pulse_reset_input();
    run_ticks(51U);
    TEST_ASSERT_EQUAL(3, host_event_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(4, host_event_count());
    expect_event(3U, HOST_EVENT_STEP_NOTE, 0U, 14U);

    // The edge is acted on once: step 1 follows
    run_ticks(TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(5, host_event_count());
    expect_event(4U, HOST_EVENT_STEP_NOTE, 1U, 13U);
}



static void test_init_starts_a_stopped_pattern_running_again(void)
{
    start_app();
    run_ticks(1U);
    host_set_run_button(true);
    run_ticks(SETTLE_TICKS + 1U);
    host_set_run_button(false);
    run_ticks(TICKS_PER_STEP);
    TEST_ASSERT(!host_gate_level());

    // A fresh start: running, with no change of transport to report
    start_app();
    run_ticks(2U);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 14U);
}



static void test_the_pot_sets_the_tempo_only_while_the_selector_is_on_it(void)
{
    uint16_t events;

    // 150 BPM from the pot as it stood at power-on: a step every 100 ticks
    start_app();
    run_ticks(2U);
    TEST_ASSERT_EQUAL(1, host_event_count());

    // The selector moved off the tempo and the pot turned right up: the
    // tempo holds
    set_selector_and_pot(1U, 1023U);
    run_ticks(10U * TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(11, host_event_count());

    // Back on the tempo, the pot is not where the tempo was left: it still
    // holds
    set_selector_and_pot(0U, 1023U);
    run_ticks(5U * TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(16, host_event_count());

    // Turned back to meet it, the pot has the tempo again: 285 BPM is 19
    // steps in the time 150 BPM takes for 10
    set_selector_and_pot(0U, TEMPO_RAW_150_BPM);
    run_ticks(2U * TICKS_PER_STEP);
    set_selector_and_pot(0U, 1023U);
    events = host_event_count();
    run_ticks(10U * TICKS_PER_STEP);
    TEST_ASSERT(host_event_count() >= (events + 17U));
}



static void test_the_lfo_and_the_note_amount_reach_the_sequencer(void)
{
    // Step 0 as it is set. The LFO is read with the panel, every scan
    start_app();
    run_ticks(2U);
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 14U);
    TEST_ASSERT_EQUAL(host_step_read_count(), host_mod_read_count());

    // Position 1 is the note. Full amount and the LFO at its top is seven
    // places up, which takes step 1's 14 to the top note
    set_amount(1U, 1023U);
    host_set_mod(1023U, STATUS_OK);
    run_ticks(TICKS_PER_STEP);
    expect_event(1U, HOST_EVENT_STEP_NOTE, 1U, 14U);

    // At its bottom, seven down: step 2's 13 is 6
    host_set_mod(0U, STATUS_OK);
    run_ticks(TICKS_PER_STEP);
    expect_event(2U, HOST_EVENT_STEP_NOTE, 2U, 5U);

    // An LFO that cannot be read moves nothing, and nothing is logged of it
    host_set_mod(1023U, ERR_TIMEOUT);
    run_ticks(TICKS_PER_STEP);
    expect_event(3U, HOST_EVENT_STEP_NOTE, 3U, 11U);
    TEST_ASSERT_EQUAL(4, host_event_count());
}



static void test_positions_2_and_3_are_the_gate_length_and_the_chance(void)
{
    uint16_t rises;

    // Position 2, full amount, the LFO at its top: a gate of a whole step,
    // which ties, so it rises once more and stays up
    start_app();
    run_ticks(2U);
    set_amount(2U, 1023U);
    host_set_mod(1023U, STATUS_OK);
    run_ticks(2U * TICKS_PER_STEP);
    rises = host_gate_rise_count();
    TEST_ASSERT_EQUAL(2, rises);
    run_ticks(3U * TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(rises, host_gate_rise_count());
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT_EQUAL(6, host_event_count()); // Steps 0 to 5 have begun

    // Position 3, the same: a step drops out on any draw but 0, and the
    // random numbers begin 135, 35, 70, 220
    set_amount(3U, 1023U);
    run_ticks(4U * TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(10, host_event_count());
    for (uint16_t step = 6U; step < 10U; step++)
    {
        expect_event(step, HOST_EVENT_STEP_REST, step, 0U);
    }
    TEST_ASSERT(!host_gate_level());
}



static void test_position_4_is_the_step_played(void)
{
    start_app();
    run_ticks(2U);
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 14U);

    // Full amount and the LFO half way up is four of the sixteen places
    // on: step 1 plays as step 5, with step 5's note
    set_amount(4U, 1023U);
    host_set_mod(768U, STATUS_OK);
    run_ticks(TICKS_PER_STEP);
    expect_event(1U, HOST_EVENT_STEP_NOTE, 5U, 9U);

    // At its bottom, eight back: step 2 plays as step 10
    host_set_mod(0U, STATUS_OK);
    run_ticks(TICKS_PER_STEP);
    expect_event(2U, HOST_EVENT_STEP_NOTE, 10U, 4U);
}



static void test_the_level_light_shows_the_amount_the_selector_is_on(void)
{
    // Written on every tick, a tick behind: nothing has been worked out
    // when the first tick applies
    start_app();
    run_ticks(1U);
    TEST_ASSERT_EQUAL(1, host_level_write_count());
    TEST_ASSERT_EQUAL(0, host_level());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(2, host_level_write_count());
    TEST_ASSERT_EQUAL(TEMPO_RAW_150_BPM / 4U, host_level());

    // Another amount, which is 0 until the pot has met it and gone up
    set_selector_and_pot(1U, TEMPO_RAW_150_BPM);
    run_ticks(10U);
    TEST_ASSERT_EQUAL(0, host_level());
    set_selector_and_pot(1U, 0U);
    run_ticks(10U);
    set_selector_and_pot(1U, 600U);
    run_ticks(10U);
    TEST_ASSERT_EQUAL(150, host_level());

    // And back: the tempo is where it was left
    set_selector_and_pot(0U, 600U);
    run_ticks(10U);
    TEST_ASSERT_EQUAL(TEMPO_RAW_150_BPM / 4U, host_level());
    TEST_ASSERT_EQUAL(42, host_level_write_count());
}



static void test_the_level_light_is_dark_in_calibration_mode(void)
{
    start_calibration(2U, CAL_TRIM_CENTRE);
    run_ticks(50U);
    TEST_ASSERT_EQUAL(50, host_level_write_count());
    TEST_ASSERT_EQUAL(0, host_level());
}



static void test_calibration_mode_leaves_the_transport_alone(void)
{
    start_calibration(2U, CAL_TRIM_CENTRE);
    host_set_run_button(true);
    host_pulse_clock_input();
    host_pulse_reset_input();
    run_ticks(3U * SETTLE_TICKS);

    TEST_ASSERT_EQUAL(0, host_event_count());
    TEST_ASSERT_EQUAL(2000, host_cv_millivolts());
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT(!host_clock_output_level());
}



int main(void)
{
    RUN_TEST(test_the_run_button_stops_and_starts_the_pattern_and_is_logged);
    RUN_TEST(test_an_external_edge_takes_the_pattern_over_from_the_clock_input);
    RUN_TEST(test_the_reset_input_sends_the_pattern_back_to_its_start);
    RUN_TEST(test_init_starts_a_stopped_pattern_running_again);
    RUN_TEST(test_the_pot_sets_the_tempo_only_while_the_selector_is_on_it);
    RUN_TEST(test_the_lfo_and_the_note_amount_reach_the_sequencer);
    RUN_TEST(test_positions_2_and_3_are_the_gate_length_and_the_chance);
    RUN_TEST(test_position_4_is_the_step_played);
    RUN_TEST(test_the_level_light_shows_the_amount_the_selector_is_on);
    RUN_TEST(test_the_level_light_is_dark_in_calibration_mode);
    RUN_TEST(test_calibration_mode_leaves_the_transport_alone);
    return TEST_RESULT();
}
