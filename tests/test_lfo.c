/**
 * @file   test_lfo.c
 * @brief  Host unit tests for the sequencer's own LFO (core/lfo.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "lfo.h"
#include "test_harness.h"

#define POISON_BYTE (0xA5)
#define CENTRE      (512U)
#define TOP         (1023U)

// Rate readings and the step each gives in a tick, in 65536ths of a cycle:
// the reading over 4, squared, over 48
#define RATE_OFF     (27U)   // 6 squared is 36: nothing
#define RATE_SLOWEST (28U)   // 7 squared is 49: 1
#define RATE_QUARTER (256U)  // 64 squared is 4096: 85
#define RATE_HALF    (512U)  // 128 squared is 16384: 341
#define STEP_TOP     (1354U) // 255 squared is 65025

// At the slowest rate, with 128 ticks gone by at each call, this many
// calls are a quarter of a cycle
#define TICKS_A_CALL    (128U)
#define CALLS_A_QUARTER (128U)

/**
 * @brief Inputs for one call: nothing from outside, and a rate.
 */
static lfo_inputs_t make_inputs(uint16_t rate, uint8_t elapsed_ticks)
{
    lfo_inputs_t in;

    (void)memset(&in, 0, sizeof(in));
    in.rate             = rate;
    in.b_rate_valid     = true;
    in.external         = CENTRE;
    in.b_external_valid = true;
    in.elapsed_ticks    = elapsed_ticks;
    return in;
}



/**
 * @brief Runs a number of calls with the same inputs and gives the signal
 *        after the last.
 */
static uint16_t run_calls(lfo_state_t *p_state, const lfo_inputs_t *p_in, unsigned int calls)
{
    uint16_t signal = 0U;

    for (unsigned int call = 0U; call < calls; call++)
    {
        signal = lfo_tick(p_state, p_in);
    }
    return signal;
}



static void test_init_is_off(void)
{
    lfo_state_t  state;
    lfo_inputs_t in = make_inputs(TOP, 1U);

    (void)memset(&state, POISON_BYTE, sizeof(state));
    lfo_init(&state);
    TEST_ASSERT_EQUAL(0, state.phase);
    TEST_ASSERT_EQUAL(0, state.rate);

    // Until a rate is read there is none
    in.b_rate_valid = false;
    TEST_ASSERT_EQUAL(CENTRE, run_calls(&state, &in, 100U));
    TEST_ASSERT_EQUAL(0, state.phase);
}



static void test_a_cycle_goes_up_to_the_top_back_down_to_the_bottom_and_back(void)
{
    lfo_state_t        state;
    const lfo_inputs_t in = make_inputs(RATE_SLOWEST, TICKS_A_CALL);

    lfo_init(&state);
    TEST_ASSERT_EQUAL(TOP, run_calls(&state, &in, CALLS_A_QUARTER));
    TEST_ASSERT_EQUAL(CENTRE, run_calls(&state, &in, CALLS_A_QUARTER));
    TEST_ASSERT_EQUAL(1, run_calls(&state, &in, CALLS_A_QUARTER));
    TEST_ASSERT_EQUAL(CENTRE, run_calls(&state, &in, CALLS_A_QUARTER));
    TEST_ASSERT_EQUAL(0, state.phase);

    // And round again
    TEST_ASSERT_EQUAL(TOP, run_calls(&state, &in, CALLS_A_QUARTER));
}



static void test_between_the_table_s_points_it_is_a_straight_line(void)
{
    lfo_state_t        state;
    const lfo_inputs_t in = make_inputs(RATE_SLOWEST, TICKS_A_CALL);

    // Half way across the first segment, whose end is 25 of 255: 13, which
    // is 26 of the swing. The same place in each quarter is as far out
    lfo_init(&state);
    TEST_ASSERT_EQUAL(CENTRE + 26U, run_calls(&state, &in, 4U));
    TEST_ASSERT_EQUAL(CENTRE + 26U, run_calls(&state, &in, (2U * CALLS_A_QUARTER) - 8U));
    TEST_ASSERT_EQUAL(CENTRE - 26U, run_calls(&state, &in, 8U));
    TEST_ASSERT_EQUAL(CENTRE - 26U, run_calls(&state, &in, (2U * CALLS_A_QUARTER) - 8U));
}



static void test_the_first_quarter_only_rises(void)
{
    lfo_state_t        state;
    const lfo_inputs_t in     = make_inputs(RATE_SLOWEST, 1U);
    uint16_t           before = CENTRE;

    lfo_init(&state);
    for (unsigned int tick = 0U; tick < 16384U; tick++)
    {
        const uint16_t signal = lfo_tick(&state, &in);

        TEST_ASSERT(signal >= before);
        before = signal;
    }
    TEST_ASSERT_EQUAL(TOP, before);
}



static void test_the_rate_is_on_a_curve(void)
{
    static const struct
    {
        uint16_t rate;
        uint16_t step;
    } cases[] =
    {
        { RATE_SLOWEST, 1U },
        { RATE_QUARTER, 85U },
        { RATE_HALF,    341U },
        { TOP,          STEP_TOP },
        { 5000U,        STEP_TOP } // Past the top is the top
    };

    for (size_t i = 0U; i < (sizeof(cases) / sizeof(cases[0])); i++)
    {
        lfo_state_t        state;
        const lfo_inputs_t in = make_inputs(cases[i].rate, 1U);

        lfo_init(&state);
        (void)lfo_tick(&state, &in);
        TEST_ASSERT_EQUAL(cases[i].step, state.phase);
    }
}



static void test_at_the_top_rate_it_is_1354_cycles_in_65536_ticks(void)
{
    lfo_state_t        state;
    const lfo_inputs_t in     = make_inputs(TOP, 1U);
    unsigned int       cycles = 0U;
    uint16_t           before = 0U;

    // A cycle begins each time the place in it comes round, and the first
    // half of every one is at the centre or above it, the second at it or
    // below
    lfo_init(&state);
    for (unsigned long tick = 0UL; tick < 65536UL; tick++)
    {
        const uint16_t signal = lfo_tick(&state, &in);

        cycles += (state.phase < before) ? 1U : 0U;
        before  = state.phase;
        TEST_ASSERT((state.phase < 0x8000U) ? (signal >= CENTRE) : (signal <= CENTRE));
    }
    TEST_ASSERT_EQUAL(STEP_TOP, cycles);
    TEST_ASSERT_EQUAL(0, state.phase);
}



static void test_a_stall_moves_the_cycle_on_by_all_the_ticks_it_took(void)
{
    lfo_state_t        state;
    const lfo_inputs_t in = make_inputs(TOP, 255U);

    // 1354 * 255, less the five whole cycles in it
    lfo_init(&state);
    (void)lfo_tick(&state, &in);
    TEST_ASSERT_EQUAL(17590, state.phase);
}



static void test_off_it_is_the_signal_from_outside(void)
{
    lfo_state_t  state;
    lfo_inputs_t in = make_inputs(RATE_OFF, 1U);

    lfo_init(&state);
    in.external = 700U;
    TEST_ASSERT_EQUAL(700, run_calls(&state, &in, 50U));
    TEST_ASSERT_EQUAL(0, state.phase);

    // Past the top is the top, and one that cannot be read is the centre
    in.external = 2000U;
    TEST_ASSERT_EQUAL(TOP, lfo_tick(&state, &in));
    in.b_external_valid = false;
    TEST_ASSERT_EQUAL(CENTRE, lfo_tick(&state, &in));
}



static void test_its_swing_is_added_to_the_signal_from_outside(void)
{
    lfo_state_t  state;
    lfo_inputs_t in = make_inputs(RATE_SLOWEST, TICKS_A_CALL);

    // At its top, 511 up: no further than the top
    lfo_init(&state);
    in.external = 100U;
    TEST_ASSERT_EQUAL(611, run_calls(&state, &in, CALLS_A_QUARTER));
    in.elapsed_ticks = 0U;
    in.external      = 700U;
    TEST_ASSERT_EQUAL(TOP, lfo_tick(&state, &in));
    in.b_external_valid = false;
    TEST_ASSERT_EQUAL(TOP, lfo_tick(&state, &in));

    // At its bottom, 511 down: no further than 0
    in.b_external_valid = true;
    in.elapsed_ticks    = TICKS_A_CALL;
    in.external         = 600U;
    TEST_ASSERT_EQUAL(89, run_calls(&state, &in, 2U * CALLS_A_QUARTER));
    in.elapsed_ticks = 0U;
    in.external      = 300U;
    TEST_ASSERT_EQUAL(0, lfo_tick(&state, &in));
    in.external = 511U;
    TEST_ASSERT_EQUAL(0, lfo_tick(&state, &in));
}



static void test_a_rate_that_cannot_be_read_leaves_the_last_one(void)
{
    lfo_state_t  state;
    lfo_inputs_t in = make_inputs(RATE_SLOWEST, TICKS_A_CALL);

    lfo_init(&state);
    (void)lfo_tick(&state, &in);
    in.rate         = 0U;
    in.b_rate_valid = false;
    TEST_ASSERT_EQUAL(TOP, run_calls(&state, &in, CALLS_A_QUARTER - 1U));
    TEST_ASSERT_EQUAL(RATE_SLOWEST, state.rate);
}



static void test_turned_off_it_starts_again_from_the_centre_going_up(void)
{
    lfo_state_t  state;
    lfo_inputs_t in = make_inputs(RATE_SLOWEST, TICKS_A_CALL);

    // Stopped on its way down, below the centre
    lfo_init(&state);
    TEST_ASSERT(run_calls(&state, &in, (2U * CALLS_A_QUARTER) + 8U) < CENTRE);
    in.rate = 0U;
    TEST_ASSERT_EQUAL(CENTRE, lfo_tick(&state, &in));

    in.rate = RATE_SLOWEST;
    TEST_ASSERT_EQUAL(CENTRE + 26U, run_calls(&state, &in, 4U));
}



static void test_null_pointers_are_ignored(void)
{
    lfo_state_t        state;
    const lfo_inputs_t in = make_inputs(TOP, 1U);

    lfo_init(NULL); // Must not crash
    lfo_init(&state);
    TEST_ASSERT_EQUAL(CENTRE, lfo_tick(NULL, &in));
    TEST_ASSERT_EQUAL(CENTRE, lfo_tick(&state, NULL));
    TEST_ASSERT_EQUAL(0, state.phase);
}



int main(void)
{
    RUN_TEST(test_init_is_off);
    RUN_TEST(test_a_cycle_goes_up_to_the_top_back_down_to_the_bottom_and_back);
    RUN_TEST(test_between_the_table_s_points_it_is_a_straight_line);
    RUN_TEST(test_the_first_quarter_only_rises);
    RUN_TEST(test_the_rate_is_on_a_curve);
    RUN_TEST(test_at_the_top_rate_it_is_1354_cycles_in_65536_ticks);
    RUN_TEST(test_a_stall_moves_the_cycle_on_by_all_the_ticks_it_took);
    RUN_TEST(test_off_it_is_the_signal_from_outside);
    RUN_TEST(test_its_swing_is_added_to_the_signal_from_outside);
    RUN_TEST(test_a_rate_that_cannot_be_read_leaves_the_last_one);
    RUN_TEST(test_turned_off_it_starts_again_from_the_centre_going_up);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
