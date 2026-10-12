/**
 * @file   test_env.c
 * @brief  Host unit tests for the envelope (core/env.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "env.h"
#include "test_harness.h"

#define POISON_BYTE (0xA5)
#define LEVEL_MAX   (4095U)

// Amounts and the time of a slope each gives: the amount over 4, squared,
// over 64
#define AMOUNT_NO_TIME (31U)   // 7 squared is 49: 0 ms
#define AMOUNT_1_MS    (32U)   // 8 squared is 64
#define AMOUNT_100_MS  (320U)  // 80 squared is 6400
#define AMOUNT_TOP     (1023U) // 255 squared is 65025: 1016 ms

/**
 * @brief Inputs for one tick of a millisecond.
 */
static env_inputs_t make_inputs(bool b_gate, uint8_t amplitude, uint16_t time)
{
    env_inputs_t in;

    (void)memset(&in, 0, sizeof(in));
    in.b_gate        = b_gate;
    in.amplitude     = amplitude;
    in.time          = time;
    in.elapsed_ticks = 1U;
    return in;
}



/**
 * @brief Runs a number of ticks with the same inputs and gives the level
 *        after the last.
 */
static uint16_t run_ticks(env_state_t *p_state, const env_inputs_t *p_in, unsigned int ticks)
{
    uint16_t level = 0U;

    for (unsigned int tick = 0U; tick < ticks; tick++)
    {
        level = env_tick(p_state, p_in);
    }
    return level;
}



static void test_init_is_at_zero_with_nothing_under_way(void)
{
    env_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    env_init(&state);
    TEST_ASSERT_EQUAL(0, state.level);
    TEST_ASSERT_EQUAL(0, state.peak);
    TEST_ASSERT_EQUAL(0, state.span);
    TEST_ASSERT_EQUAL(0, state.remainder);
    TEST_ASSERT_EQUAL(ENV_IDLE, state.phase);
    TEST_ASSERT(!state.b_gate);
}



static void test_with_no_time_the_level_follows_the_gate(void)
{
    env_state_t  state;
    env_inputs_t in = make_inputs(false, 255U, 0U);

    env_init(&state);
    TEST_ASSERT_EQUAL(0, env_tick(&state, &in));

    in.b_gate = true;
    TEST_ASSERT_EQUAL(LEVEL_MAX, env_tick(&state, &in));
    TEST_ASSERT_EQUAL(LEVEL_MAX, run_ticks(&state, &in, 500U)); // And stays

    in.b_gate = false;
    TEST_ASSERT_EQUAL(0, env_tick(&state, &in));
    TEST_ASSERT_EQUAL(ENV_IDLE, state.phase);
}



static void test_the_peak_is_in_proportion_to_the_amplitude(void)
{
    static const struct
    {
        uint8_t  amplitude;
        uint16_t peak;
    } cases[] =
    {
        { 255U, 4095U }, { 128U, 2056U }, { 16U, 257U }, { 1U, 16U }, { 0U, 0U }
    };

    for (size_t i = 0U; i < (sizeof(cases) / sizeof(cases[0])); i++)
    {
        env_state_t        state;
        const env_inputs_t in = make_inputs(true, cases[i].amplitude, 0U);

        env_init(&state);
        TEST_ASSERT_EQUAL(cases[i].peak, env_tick(&state, &in));
    }
}



static void test_the_amplitude_is_taken_as_the_gate_rises_and_not_after(void)
{
    env_state_t  state;
    env_inputs_t in = make_inputs(true, 255U, 0U);

    env_init(&state);
    TEST_ASSERT_EQUAL(LEVEL_MAX, env_tick(&state, &in));
    in.amplitude = 16U;
    TEST_ASSERT_EQUAL(LEVEL_MAX, env_tick(&state, &in));

    // The next note has it
    in.b_gate = false;
    (void)env_tick(&state, &in);
    in.b_gate = true;
    TEST_ASSERT_EQUAL(257, env_tick(&state, &in));
}



static void test_a_note_rises_to_its_peak_and_falls_at_once_whatever_the_gate(void)
{
    env_state_t  state;
    env_inputs_t in = make_inputs(true, 255U, AMOUNT_100_MS);

    // 4095 in 100 ms is 40.95 a tick: 40 after one, 2047 after fifty, and
    // all of it after a hundred
    env_init(&state);
    TEST_ASSERT_EQUAL(40, env_tick(&state, &in));
    TEST_ASSERT_EQUAL(ENV_ATTACK, state.phase);
    TEST_ASSERT_EQUAL(2047, run_ticks(&state, &in, 49U));

    // The gate closing changes nothing
    in.b_gate = false;
    TEST_ASSERT_EQUAL(LEVEL_MAX, run_ticks(&state, &in, 50U));
    TEST_ASSERT_EQUAL(ENV_DECAY, state.phase);

    // Down again in the same time, and there it stays
    TEST_ASSERT_EQUAL(LEVEL_MAX - 40U, env_tick(&state, &in));
    TEST_ASSERT_EQUAL(LEVEL_MAX - 2047U, run_ticks(&state, &in, 49U));
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, 50U));
    TEST_ASSERT_EQUAL(ENV_IDLE, state.phase);
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, 300U));
}



static void test_a_gate_held_open_sustains_nothing_and_starts_nothing(void)
{
    env_state_t        state;
    const env_inputs_t in = make_inputs(true, 255U, AMOUNT_100_MS);

    env_init(&state);
    TEST_ASSERT_EQUAL(LEVEL_MAX, run_ticks(&state, &in, 100U));
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, 100U));
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, 1000U));
    TEST_ASSERT_EQUAL(ENV_IDLE, state.phase);
}



static void test_a_new_note_starts_from_where_the_level_is(void)
{
    env_state_t  state;
    env_inputs_t in = make_inputs(true, 255U, AMOUNT_100_MS);
    uint16_t     before;

    // Up, and half way down again
    env_init(&state);
    (void)run_ticks(&state, &in, 150U);
    in.b_gate = false;
    before    = env_tick(&state, &in);
    TEST_ASSERT_EQUAL(LEVEL_MAX - 2088U, before); // 51 ms of the decay

    // The gate rises: up from there, with no jump, to the peak in 100 ms
    in.b_gate = true;
    TEST_ASSERT_EQUAL(before + 20U, env_tick(&state, &in)); // 2088 in 100 ms
    TEST_ASSERT_EQUAL(ENV_ATTACK, state.phase);
    TEST_ASSERT_EQUAL(LEVEL_MAX, run_ticks(&state, &in, 99U));
    TEST_ASSERT_EQUAL(ENV_DECAY, state.phase);
}



static void test_a_note_with_a_lower_peak_comes_down_to_it(void)
{
    env_state_t  state;
    env_inputs_t in = make_inputs(true, 255U, AMOUNT_100_MS);

    // At the top, and one tick of the decay
    env_init(&state);
    (void)run_ticks(&state, &in, 100U);
    in.b_gate = false;
    TEST_ASSERT_EQUAL(LEVEL_MAX - 40U, env_tick(&state, &in));

    // A note whose peak is 2056: down to it in 100 ms, then on down to 0
    in.b_gate    = true;
    in.amplitude = 128U;
    TEST_ASSERT_EQUAL(4036, env_tick(&state, &in)); // 1999 in 100 ms
    TEST_ASSERT_EQUAL(2056, run_ticks(&state, &in, 99U));
    TEST_ASSERT_EQUAL(ENV_DECAY, state.phase);
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, 100U));
}



static void test_a_note_at_the_level_it_begins_from_goes_straight_to_the_decay(void)
{
    env_state_t        state;
    const env_inputs_t in = make_inputs(true, 0U, AMOUNT_100_MS);

    // A peak of 0 from a level of 0: nowhere to go, up or down
    env_init(&state);
    TEST_ASSERT_EQUAL(0, env_tick(&state, &in));
    TEST_ASSERT_EQUAL(ENV_DECAY, state.phase);
    TEST_ASSERT_EQUAL(0, env_tick(&state, &in));
    TEST_ASSERT_EQUAL(ENV_IDLE, state.phase);
}



static void test_the_time_is_on_a_curve_from_the_amount(void)
{
    static const struct
    {
        uint16_t     amount;
        unsigned int ms;
    } cases[] =
    {
        { AMOUNT_1_MS, 1U }, { 128U, 16U }, { 256U, 64U }, { 512U, 256U },
        { AMOUNT_TOP, 1016U }, { 5000U, 1016U } // Above the top is the top
    };

    for (size_t i = 0U; i < (sizeof(cases) / sizeof(cases[0])); i++)
    {
        env_state_t        state;
        const env_inputs_t in = make_inputs(true, 255U, cases[i].amount);

        // At the peak after that many ticks and not one sooner
        env_init(&state);
        TEST_ASSERT(run_ticks(&state, &in, cases[i].ms - 1U) < LEVEL_MAX);
        TEST_ASSERT_EQUAL(LEVEL_MAX, env_tick(&state, &in));
    }
}



static void test_an_amount_too_small_for_a_millisecond_is_no_time(void)
{
    env_state_t        state;
    const env_inputs_t in = make_inputs(true, 255U, AMOUNT_NO_TIME);

    env_init(&state);
    TEST_ASSERT_EQUAL(LEVEL_MAX, env_tick(&state, &in));
    TEST_ASSERT_EQUAL(LEVEL_MAX, run_ticks(&state, &in, 100U));
}



static void test_the_slowest_slope_to_the_lowest_peak_still_gets_there(void)
{
    env_state_t  state;
    env_inputs_t in = make_inputs(true, 1U, AMOUNT_TOP);

    // 16 in 1016 ms: a unit every 63.5 ms
    env_init(&state);
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, 63U));
    TEST_ASSERT_EQUAL(1, env_tick(&state, &in));
    TEST_ASSERT_EQUAL(16, run_ticks(&state, &in, 952U));
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, 1016U));
}



static void test_a_changed_time_is_taken_up_by_a_slope_under_way(void)
{
    env_state_t  state;
    env_inputs_t in = make_inputs(true, 255U, AMOUNT_100_MS);

    // Half way up, and then the rest at the rate of a slope of 16 ms: the
    // span is still the whole slope's, so about 256 a tick, with the half
    // unit owed from before going into the first
    env_init(&state);
    TEST_ASSERT_EQUAL(2047, run_ticks(&state, &in, 50U));
    in.time = 128U;
    TEST_ASSERT_EQUAL(2047U + 259U, env_tick(&state, &in));
    TEST_ASSERT_EQUAL(LEVEL_MAX, run_ticks(&state, &in, 7U));
}



static void test_the_time_taken_away_puts_the_level_where_the_gate_has_it(void)
{
    env_state_t  state;
    env_inputs_t in = make_inputs(true, 255U, AMOUNT_100_MS);

    env_init(&state);
    (void)run_ticks(&state, &in, 30U);
    in.time = 0U;
    TEST_ASSERT_EQUAL(LEVEL_MAX, env_tick(&state, &in));

    // And given back with the gate still open, it falls from there
    in.time = AMOUNT_100_MS;
    TEST_ASSERT_EQUAL(LEVEL_MAX - 40U, env_tick(&state, &in));
    TEST_ASSERT_EQUAL(0, run_ticks(&state, &in, 99U));

    // With the gate closed and no time it is 0 at once
    in.b_gate = false;
    (void)env_tick(&state, &in);
    in.b_gate = true;
    (void)run_ticks(&state, &in, 30U);
    in.b_gate = false;
    in.time   = 0U;
    TEST_ASSERT_EQUAL(0, env_tick(&state, &in));
}



static void test_elapsed_ticks_are_that_many_milliseconds(void)
{
    env_state_t  state;
    env_inputs_t in = make_inputs(true, 255U, AMOUNT_100_MS);

    // No time gone by: the note is begun and has not moved
    env_init(&state);
    in.elapsed_ticks = 0U;
    TEST_ASSERT_EQUAL(0, env_tick(&state, &in));
    TEST_ASSERT_EQUAL(ENV_ATTACK, state.phase);

    in.elapsed_ticks = 50U;
    TEST_ASSERT_EQUAL(2047, env_tick(&state, &in));

    // Through the peak and half way down in one go
    in.elapsed_ticks = 100U;
    TEST_ASSERT_EQUAL(LEVEL_MAX - 2047U, env_tick(&state, &in));
}



static void test_an_unknown_phase_moves_nothing(void)
{
    env_state_t  state;
    env_inputs_t in = make_inputs(false, 255U, AMOUNT_100_MS);

    env_init(&state);
    state.level = 1234U;
    state.phase = (env_phase_t)99;
    TEST_ASSERT_EQUAL(1234, env_tick(&state, &in));
}



static void test_null_pointers_are_ignored(void)
{
    env_state_t        state;
    const env_inputs_t in = make_inputs(true, 255U, 0U);

    env_init(NULL); // Must not crash

    env_init(&state);
    TEST_ASSERT_EQUAL(0, env_tick(NULL, &in));
    TEST_ASSERT_EQUAL(0, env_tick(&state, NULL));
    TEST_ASSERT(!state.b_gate); // State untouched
    TEST_ASSERT_EQUAL(0, state.level);
}



int main(void)
{
    RUN_TEST(test_init_is_at_zero_with_nothing_under_way);
    RUN_TEST(test_with_no_time_the_level_follows_the_gate);
    RUN_TEST(test_the_peak_is_in_proportion_to_the_amplitude);
    RUN_TEST(test_the_amplitude_is_taken_as_the_gate_rises_and_not_after);
    RUN_TEST(test_a_note_rises_to_its_peak_and_falls_at_once_whatever_the_gate);
    RUN_TEST(test_a_gate_held_open_sustains_nothing_and_starts_nothing);
    RUN_TEST(test_a_new_note_starts_from_where_the_level_is);
    RUN_TEST(test_a_note_with_a_lower_peak_comes_down_to_it);
    RUN_TEST(test_a_note_at_the_level_it_begins_from_goes_straight_to_the_decay);
    RUN_TEST(test_the_time_is_on_a_curve_from_the_amount);
    RUN_TEST(test_an_amount_too_small_for_a_millisecond_is_no_time);
    RUN_TEST(test_the_slowest_slope_to_the_lowest_peak_still_gets_there);
    RUN_TEST(test_a_changed_time_is_taken_up_by_a_slope_under_way);
    RUN_TEST(test_the_time_taken_away_puts_the_level_where_the_gate_has_it);
    RUN_TEST(test_elapsed_ticks_are_that_many_milliseconds);
    RUN_TEST(test_an_unknown_phase_moves_nothing);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
