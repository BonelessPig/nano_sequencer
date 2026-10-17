/**
 * @file   test_transport.c
 * @brief  Host unit tests for the transport (core/transport.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "transport.h"
#include "test_harness.h"

#define POISON_BYTE  (0xA5)
#define SETTLE_TICKS (20U) // Debounce time, as debounce.h says

static transport_state_t   g_state;
static transport_inputs_t  g_in;
static transport_outputs_t g_out;

/**
 * @brief Starts a fresh transport with the button up, no edge and nothing
 *        from MIDI.
 */
static void start(void)
{
    transport_init(&g_state);
    g_in.b_run_button  = false;
    g_in.b_clock_edge  = false;
    g_in.b_midi_edge   = false;
    g_in.b_midi_start  = false;
    g_in.b_midi_stop   = false;
    g_in.elapsed_ticks = 1U;
    (void)memset(&g_out, POISON_BYTE, sizeof(g_out));
}

/**
 * @brief Runs a number of single ticks with the controls as they stand.
 */
static void run_ticks(unsigned int ticks)
{
    for (unsigned int tick = 0U; tick < ticks; tick++)
    {
        transport_tick(&g_state, &g_in, &g_out);
    }
}

/**
 * @brief Presses the run button for long enough to count, then lets go for
 *        as long. The outputs are those of the tick the press took effect.
 */
static void press_run(void)
{
    transport_outputs_t at_press;

    g_in.b_run_button = true;
    run_ticks(SETTLE_TICKS);
    at_press = g_out;
    g_in.b_run_button = false;
    run_ticks(SETTLE_TICKS);
    g_out = at_press;
}



static void test_the_first_tick_starts_on_the_internal_clock(void)
{
    start();
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, g_out.mode);
    TEST_ASSERT(g_out.b_started);
    TEST_ASSERT(!g_out.b_changed);

    // The start is reported once
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, g_out.mode);
    TEST_ASSERT(!g_out.b_started);
    TEST_ASSERT(!g_out.b_changed);
}



static void test_an_edge_on_the_first_tick_starts_on_the_external_clock(void)
{
    start();
    g_in.b_clock_edge = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, g_out.mode);
    TEST_ASSERT(g_out.b_started);
    TEST_ASSERT(g_out.b_changed);
    TEST_ASSERT(g_out.b_external);

    g_in.b_clock_edge = false;
    run_ticks(1U);
    TEST_ASSERT(!g_out.b_changed);
}



static void test_an_edge_takes_the_clock_over_and_it_stays(void)
{
    start();
    run_ticks(1U);
    TEST_ASSERT(!g_out.b_external);

    // At once: an edge is not a contact, and is not debounced
    g_in.b_clock_edge = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, g_out.mode);
    TEST_ASSERT(g_out.b_changed);
    TEST_ASSERT(g_out.b_external);
    TEST_ASSERT(!g_out.b_started); // A change of clock is not a start

    // However long the next edge takes to come
    g_in.b_clock_edge = false;
    run_ticks(10000U);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, g_out.mode);
    TEST_ASSERT(g_out.b_external);
    TEST_ASSERT(!g_out.b_changed);
}



static void test_a_press_stops_and_the_next_starts_again(void)
{
    start();
    run_ticks(1U);

    press_run();
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
    TEST_ASSERT(g_out.b_changed);
    TEST_ASSERT(!g_out.b_started);

    press_run();
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, g_out.mode);
    TEST_ASSERT(g_out.b_changed);
    TEST_ASSERT(g_out.b_started);

    // Reported once each
    run_ticks(1U);
    TEST_ASSERT(!g_out.b_changed);
    TEST_ASSERT(!g_out.b_started);
}



static void test_holding_the_button_is_one_press(void)
{
    start();
    run_ticks(1U);

    g_in.b_run_button = true;
    run_ticks(500U);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
}



static void test_a_bouncing_press_is_one_press(void)
{
    start();
    run_ticks(1U);

    for (unsigned int bounce = 0U; bounce < 5U; bounce++)
    {
        g_in.b_run_button = true;
        run_ticks(2U);
        g_in.b_run_button = false;
        run_ticks(2U);
    }
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, g_out.mode);

    press_run();
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
}



static void test_a_button_held_at_power_on_is_not_a_press(void)
{
    start();
    g_in.b_run_button = true;
    run_ticks(100U);
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, g_out.mode);

    // Letting go and pressing again is
    g_in.b_run_button = false;
    run_ticks(SETTLE_TICKS);
    press_run();
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
}



static void test_a_stop_forgets_the_external_clock(void)
{
    start();
    g_in.b_clock_edge = true;
    run_ticks(1U);
    g_in.b_clock_edge = false;

    press_run();
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
    TEST_ASSERT(!g_out.b_external);

    press_run();
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, g_out.mode);
    TEST_ASSERT(g_out.b_started);
    TEST_ASSERT(!g_out.b_external);
}



static void test_an_edge_while_stopped_brings_the_external_clock_back(void)
{
    start();
    run_ticks(1U);
    press_run();

    (void)memset(&g_out, POISON_BYTE, sizeof(g_out));
    g_in.b_clock_edge = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
    TEST_ASSERT(!g_out.b_changed);
    TEST_ASSERT(g_out.b_external);

    g_in.b_clock_edge = false;
    press_run();
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, g_out.mode);
    TEST_ASSERT(g_out.b_started);
}



static void test_an_edge_on_the_tick_of_a_stop_is_kept(void)
{
    start();
    run_ticks(1U);

    g_in.b_run_button = true;
    run_ticks(SETTLE_TICKS - 1U);
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, g_out.mode);
    g_in.b_clock_edge = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
    TEST_ASSERT(g_out.b_external);
}



static void test_elapsed_ticks_are_passed_to_the_debounce(void)
{
    start();
    run_ticks(1U);

    g_in.b_run_button  = true;
    g_in.elapsed_ticks = (uint8_t)SETTLE_TICKS;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
}



static void test_the_edge_given_is_the_clock_inputs_once_it_is_followed(void)
{
    start();
    run_ticks(1U);
    TEST_ASSERT(!g_out.b_edge);

    g_in.b_clock_edge = true;
    run_ticks(1U);
    TEST_ASSERT(g_out.b_external);
    TEST_ASSERT(g_out.b_edge);
    TEST_ASSERT(!g_state.b_midi);

    // A MIDI clock that comes after it is passed over
    g_in.b_clock_edge = false;
    g_in.b_midi_edge  = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, g_out.mode);
    TEST_ASSERT(!g_out.b_edge);

    g_in.b_clock_edge = true;
    run_ticks(1U);
    TEST_ASSERT(g_out.b_edge);

    g_in.b_clock_edge = false;
    g_in.b_midi_edge  = false;
    run_ticks(1U);
    TEST_ASSERT(!g_out.b_edge);
}



static void test_a_midi_edge_takes_the_clock_over_if_it_is_first(void)
{
    start();
    run_ticks(1U);

    g_in.b_midi_edge = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, g_out.mode);
    TEST_ASSERT(g_out.b_changed);
    TEST_ASSERT(g_out.b_external);
    TEST_ASSERT(g_out.b_edge);
    TEST_ASSERT(!g_out.b_started); // A change of clock is not a start
    TEST_ASSERT(g_state.b_midi);

    // And an edge on the clock input is the one passed over
    g_in.b_midi_edge  = false;
    g_in.b_clock_edge = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, g_out.mode);
    TEST_ASSERT(!g_out.b_edge);

    g_in.b_midi_edge = true;
    run_ticks(1U);
    TEST_ASSERT(g_out.b_edge);
}



static void test_of_two_first_edges_on_one_tick_the_clock_input_is_taken(void)
{
    start();
    run_ticks(1U);

    g_in.b_clock_edge = true;
    g_in.b_midi_edge  = true;
    run_ticks(1U);
    TEST_ASSERT(g_out.b_external);
    TEST_ASSERT(g_out.b_edge);
    TEST_ASSERT(!g_state.b_midi);

    g_in.b_clock_edge = false;
    run_ticks(1U);
    TEST_ASSERT(!g_out.b_edge);
}



static void test_a_stop_lets_the_other_clock_be_the_first(void)
{
    start();
    g_in.b_clock_edge = true;
    run_ticks(1U);
    g_in.b_clock_edge = false;
    TEST_ASSERT(!g_state.b_midi);

    press_run();
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
    TEST_ASSERT(!g_out.b_external);

    // Stopped, the first edge is still the one that counts
    g_in.b_midi_edge = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
    TEST_ASSERT(g_out.b_external);
    TEST_ASSERT(g_out.b_edge);
    g_in.b_midi_edge = false;

    press_run();
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, g_out.mode);
    TEST_ASSERT(g_out.b_started);

    g_in.b_clock_edge = true;
    run_ticks(1U);
    TEST_ASSERT(!g_out.b_edge);
    g_in.b_clock_edge = false;
    g_in.b_midi_edge  = true;
    run_ticks(1U);
    TEST_ASSERT(g_out.b_edge);
}



static void test_a_midi_stop_stops_as_a_press_does(void)
{
    start();
    g_in.b_clock_edge = true;
    run_ticks(1U);
    g_in.b_clock_edge = false;

    g_in.b_midi_stop = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
    TEST_ASSERT(g_out.b_changed);
    TEST_ASSERT(!g_out.b_started);
    TEST_ASSERT(!g_out.b_external); // And forgets the external clock

    // It stays stopped, and a second Stop changes nothing
    g_in.b_midi_stop = false;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
    TEST_ASSERT(!g_out.b_changed);
    g_in.b_midi_stop = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
    TEST_ASSERT(!g_out.b_changed);
    g_in.b_midi_stop = false;

    // The run button starts it again, on the internal clock
    press_run();
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, g_out.mode);
    TEST_ASSERT(g_out.b_started);
}



static void test_an_edge_on_the_tick_of_a_midi_stop_is_kept(void)
{
    start();
    run_ticks(1U);

    g_in.b_midi_stop = true;
    g_in.b_midi_edge = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
    TEST_ASSERT(g_out.b_external);
    TEST_ASSERT(g_state.b_midi);
}



static void test_a_midi_stop_undoes_a_press_that_starts_on_its_tick(void)
{
    start();
    run_ticks(1U);
    press_run();
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);

    // The press is taken on the last of these ticks, with the Stop
    g_in.b_run_button = true;
    run_ticks(SETTLE_TICKS - 1U);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
    g_in.b_midi_stop = true;
    run_ticks(1U);
    TEST_ASSERT(g_state.b_pressed);
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);
    TEST_ASSERT(!g_out.b_started);
}



static void test_a_midi_start_starts_the_pattern_on_the_midi_clock(void)
{
    start();
    run_ticks(1U);
    g_in.b_midi_stop = true;
    run_ticks(1U);
    g_in.b_midi_stop = false;
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, g_out.mode);

    // With no clock yet: it is the external clock all the same, so the
    // pattern waits for the one that follows
    g_in.b_midi_start = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, g_out.mode);
    TEST_ASSERT(g_out.b_changed);
    TEST_ASSERT(g_out.b_started);
    TEST_ASSERT(g_out.b_external);
    TEST_ASSERT(!g_out.b_edge);
    g_in.b_midi_start = false;

    run_ticks(1U);
    TEST_ASSERT(!g_out.b_started); // Reported once

    g_in.b_clock_edge = true;
    run_ticks(1U);
    TEST_ASSERT(!g_out.b_edge);
    g_in.b_clock_edge = false;
    g_in.b_midi_edge  = true;
    run_ticks(1U);
    TEST_ASSERT(g_out.b_edge);
}



static void test_a_midi_start_while_running_starts_again_and_takes_the_clock(void)
{
    // From the internal clock
    start();
    run_ticks(2U);
    g_in.b_midi_start = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, g_out.mode);
    TEST_ASSERT(g_out.b_changed);
    TEST_ASSERT(g_out.b_started);

    // And from the clock input, which is then the one passed over
    start();
    g_in.b_clock_edge = true;
    run_ticks(2U);
    TEST_ASSERT(g_out.b_edge);
    g_in.b_midi_start = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, g_out.mode);
    TEST_ASSERT(!g_out.b_changed);
    TEST_ASSERT(g_out.b_started);
    TEST_ASSERT(!g_out.b_edge);
    TEST_ASSERT(g_state.b_midi);
}



static void test_a_clock_on_the_tick_of_a_midi_start_is_its_first_edge(void)
{
    start();
    run_ticks(1U);
    g_in.b_midi_stop = true;
    run_ticks(1U);
    g_in.b_midi_stop = false;

    g_in.b_midi_start = true;
    g_in.b_midi_edge  = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, g_out.mode);
    TEST_ASSERT(g_out.b_started);
    TEST_ASSERT(g_out.b_edge);
}



static void test_of_a_midi_start_and_stop_on_one_tick_the_start_is_taken(void)
{
    start();
    run_ticks(1U);

    g_in.b_midi_start = true;
    g_in.b_midi_stop  = true;
    run_ticks(1U);
    TEST_ASSERT_EQUAL(TRANSPORT_EXTERNAL, g_out.mode);
    TEST_ASSERT(g_out.b_started);
}



static void test_init_sets_the_power_on_condition(void)
{
    (void)memset(&g_state, POISON_BYTE, sizeof(g_state));
    transport_init(&g_state);
    TEST_ASSERT(g_state.b_run);
    TEST_ASSERT(g_state.b_starting);
    TEST_ASSERT(g_state.b_pressed);
    TEST_ASSERT_EQUAL(TRANSPORT_INTERNAL, g_state.mode);
    TEST_ASSERT(!g_state.b_external);
    TEST_ASSERT(!g_state.b_midi);
    TEST_ASSERT(!g_state.run_button.b_known);
}



static void test_null_pointers_are_ignored(void)
{
    transport_state_t untouched;

    transport_init(NULL); // Must not crash

    start();
    untouched = g_state;
    transport_tick(NULL, &g_in, &g_out);
    transport_tick(&g_state, NULL, &g_out);
    transport_tick(&g_state, &g_in, NULL);
    TEST_ASSERT_EQUAL(0, memcmp(&untouched, &g_state, sizeof(g_state)));
    TEST_ASSERT_EQUAL(POISON_BYTE, *(const unsigned char *)&g_out);
}



int main(void)
{
    RUN_TEST(test_the_first_tick_starts_on_the_internal_clock);
    RUN_TEST(test_an_edge_on_the_first_tick_starts_on_the_external_clock);
    RUN_TEST(test_an_edge_takes_the_clock_over_and_it_stays);
    RUN_TEST(test_a_press_stops_and_the_next_starts_again);
    RUN_TEST(test_holding_the_button_is_one_press);
    RUN_TEST(test_a_bouncing_press_is_one_press);
    RUN_TEST(test_a_button_held_at_power_on_is_not_a_press);
    RUN_TEST(test_a_stop_forgets_the_external_clock);
    RUN_TEST(test_an_edge_while_stopped_brings_the_external_clock_back);
    RUN_TEST(test_an_edge_on_the_tick_of_a_stop_is_kept);
    RUN_TEST(test_elapsed_ticks_are_passed_to_the_debounce);
    RUN_TEST(test_the_edge_given_is_the_clock_inputs_once_it_is_followed);
    RUN_TEST(test_a_midi_edge_takes_the_clock_over_if_it_is_first);
    RUN_TEST(test_of_two_first_edges_on_one_tick_the_clock_input_is_taken);
    RUN_TEST(test_a_stop_lets_the_other_clock_be_the_first);
    RUN_TEST(test_a_midi_stop_stops_as_a_press_does);
    RUN_TEST(test_an_edge_on_the_tick_of_a_midi_stop_is_kept);
    RUN_TEST(test_a_midi_stop_undoes_a_press_that_starts_on_its_tick);
    RUN_TEST(test_a_midi_start_starts_the_pattern_on_the_midi_clock);
    RUN_TEST(test_a_midi_start_while_running_starts_again_and_takes_the_clock);
    RUN_TEST(test_a_clock_on_the_tick_of_a_midi_start_is_its_first_edge);
    RUN_TEST(test_of_a_midi_start_and_stop_on_one_tick_the_start_is_taken);
    RUN_TEST(test_init_sets_the_power_on_condition);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
