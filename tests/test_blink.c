/**
 * @file   test_blink.c
 * @brief  Host tests for the status light (core/blink.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "blink.h"
#include "test_harness.h"

#define HALF_TICKS      (80U)  // Each half of a flicker
#define LONG_HALF_TICKS (500U) // Each half of the long blink

/**
 * @brief Inputs that begin nothing: the steady level as given, one tick gone.
 */
static blink_inputs_t steady(bool b_on)
{
    blink_inputs_t in;

    in.b_on          = b_on;
    in.flickers      = 0U;
    in.b_refused     = false;
    in.hold_stage    = 0U;
    in.elapsed_ticks = 1U;
    return in;
}

/**
 * @brief Runs a number of ticks of one each that begin nothing, and checks
 *        that the light is as expected on every one of them.
 */
static void expect_ticks(blink_state_t *p_state, bool b_on, unsigned int count, bool b_lit)
{
    const blink_inputs_t in = steady(b_on);

    for (unsigned int i = 0U; i < count; i++)
    {
        TEST_ASSERT_EQUAL(b_lit, blink_tick(p_state, &in));
    }
}

/**
 * @brief Runs ticks that begin nothing until the light has been at its
 *        steady level for a whole second, and counts the times it turned
 *        the other way on the way there. It is called with a flicker under
 *        way, which is not one of those counted.
 */
static unsigned int count_flickers(blink_state_t *p_state, bool b_on)
{
    const blink_inputs_t in       = steady(b_on);
    unsigned int         flickers = 0U;
    unsigned int         quiet    = 0U;
    bool                 b_was    = !b_on;

    while (quiet < 1000U)
    {
        const bool b_lit = blink_tick(p_state, &in);

        if ((b_lit != b_on) && (b_was == b_on))
        {
            flickers++;
        }
        quiet = (b_lit == b_on) ? (quiet + 1U) : 0U;
        b_was = b_lit;
    }
    return flickers;
}



static void test_the_light_follows_the_steady_level(void)
{
    blink_state_t state;

    blink_init(&state);
    expect_ticks(&state, false, 10U, false);
    expect_ticks(&state, true, 10U, true);
    expect_ticks(&state, false, 10U, false);
}



static void test_a_flicker_turns_a_lit_light_dark_for_80_ms(void)
{
    blink_state_t  state;
    blink_inputs_t in = steady(true);

    blink_init(&state);
    in.flickers = 1U;
    TEST_ASSERT_EQUAL(false, blink_tick(&state, &in)); // The tick it is begun
    expect_ticks(&state, true, HALF_TICKS - 1U, false);
    expect_ticks(&state, true, 1000U, true);
}



static void test_a_flicker_lights_a_dark_light(void)
{
    blink_state_t  state;
    blink_inputs_t in = steady(false);

    blink_init(&state);
    in.flickers = 1U;
    TEST_ASSERT_EQUAL(true, blink_tick(&state, &in));
    expect_ticks(&state, false, HALF_TICKS - 1U, true);
    expect_ticks(&state, false, 1000U, false);
}



static void test_several_flickers_are_80_ms_apart(void)
{
    blink_state_t  state;
    blink_inputs_t in = steady(true);

    blink_init(&state);
    in.flickers = 3U;
    TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
    expect_ticks(&state, true, HALF_TICKS - 1U, false);
    expect_ticks(&state, true, HALF_TICKS, true);
    expect_ticks(&state, true, HALF_TICKS, false);
    expect_ticks(&state, true, HALF_TICKS, true);
    expect_ticks(&state, true, HALF_TICKS, false);
    expect_ticks(&state, true, 1000U, true);
}



static void test_the_number_of_flickers_is_the_number_asked_up_to_eight(void)
{
    static const uint8_t asked[]    = { 1U, 2U, 3U, 4U, 8U, 9U, 255U };
    static const uint8_t expected[] = { 1U, 2U, 3U, 4U, 8U, 8U, 8U };

    for (unsigned int i = 0U; i < (sizeof(asked) / sizeof(asked[0])); i++)
    {
        blink_state_t  state;
        blink_inputs_t in = steady(true);

        blink_init(&state);
        in.flickers = asked[i];
        TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
        // The first is under way, and is counted as the light comes back
        TEST_ASSERT_EQUAL(expected[i] - 1U, count_flickers(&state, true));
    }
}



static void test_a_refusal_is_one_long_blink(void)
{
    blink_state_t  state;
    blink_inputs_t in = steady(true);

    blink_init(&state);
    in.b_refused = true;
    in.flickers  = 3U; // The long blink is begun in their place
    TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
    expect_ticks(&state, true, LONG_HALF_TICKS - 1U, false);
    expect_ticks(&state, true, 2000U, true);
}



static void test_a_hold_stage_that_rises_is_one_flicker(void)
{
    blink_state_t  state;
    blink_inputs_t in = steady(true);

    blink_init(&state);
    in.hold_stage = 1U;
    TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
    // The stage staying where it is begins nothing more
    for (unsigned int i = 0U; i < (HALF_TICKS - 1U); i++)
    {
        TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
    }
    for (unsigned int i = 0U; i < 1000U; i++)
    {
        TEST_ASSERT_EQUAL(true, blink_tick(&state, &in));
    }

    in.hold_stage = 2U;
    TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
    in.hold_stage = 3U; // One begun while another runs takes its place
    TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
    for (unsigned int i = 0U; i < (HALF_TICKS - 1U); i++)
    {
        TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
    }
    TEST_ASSERT_EQUAL(true, blink_tick(&state, &in));

    // The stage falling begins nothing, and rising again does
    expect_ticks(&state, true, 1000U, true);
    in.hold_stage = 1U;
    TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
}



static void test_flickers_are_begun_in_place_of_a_hold_stage_flicker(void)
{
    blink_state_t  state;
    blink_inputs_t in = steady(true);

    blink_init(&state);
    in.hold_stage = 1U;
    in.flickers   = 2U;
    TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
    TEST_ASSERT_EQUAL(1, count_flickers(&state, true)); // The second of two
}



static void test_the_steady_level_can_change_under_a_flicker(void)
{
    blink_state_t  state;
    blink_inputs_t in = steady(false);

    blink_init(&state);
    in.flickers = 2U;
    TEST_ASSERT_EQUAL(true, blink_tick(&state, &in));
    // Something was saved: the steady level is lit from here
    expect_ticks(&state, true, HALF_TICKS - 1U, false);
    expect_ticks(&state, true, HALF_TICKS, true);
    expect_ticks(&state, true, HALF_TICKS, false);
    expect_ticks(&state, true, 1000U, true);
}



static void test_time_is_counted_in_elapsed_ticks(void)
{
    blink_state_t  state;
    blink_inputs_t in = steady(true);

    blink_init(&state);
    in.flickers = 1U;
    TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));

    in.flickers      = 0U;
    in.elapsed_ticks = 0U; // No time gone by: the flicker stands still
    for (unsigned int i = 0U; i < 1000U; i++)
    {
        TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
    }

    in.elapsed_ticks = 79U;
    TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
    in.elapsed_ticks = 1U;
    TEST_ASSERT_EQUAL(true, blink_tick(&state, &in));

    // A count past the end of a half ends that half and no more
    in.elapsed_ticks = 255U;
    TEST_ASSERT_EQUAL(true, blink_tick(&state, &in));
    in.flickers = 2U;
    TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
    in.flickers = 0U;
    TEST_ASSERT_EQUAL(true, blink_tick(&state, &in));
    TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
    TEST_ASSERT_EQUAL(true, blink_tick(&state, &in));
    TEST_ASSERT_EQUAL(true, blink_tick(&state, &in));
}



static void test_init_ends_a_flicker_and_forgets_the_hold_stage(void)
{
    blink_state_t  state;
    blink_inputs_t in = steady(true);

    blink_init(&state);
    in.flickers   = 4U;
    in.hold_stage = 2U;
    TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));

    blink_init(&state);
    expect_ticks(&state, true, 1000U, true);
    in.flickers = 0U; // The stage is higher than init left it
    TEST_ASSERT_EQUAL(false, blink_tick(&state, &in));
}



static void test_null_pointers_do_nothing(void)
{
    blink_state_t        state;
    const blink_inputs_t in = steady(true);

    blink_init(NULL);
    TEST_ASSERT_EQUAL(false, blink_tick(NULL, &in));

    blink_init(&state);
    TEST_ASSERT_EQUAL(false, blink_tick(&state, NULL));
    TEST_ASSERT_EQUAL(true, blink_tick(&state, &in));
}



int main(void)
{
    RUN_TEST(test_the_light_follows_the_steady_level);
    RUN_TEST(test_a_flicker_turns_a_lit_light_dark_for_80_ms);
    RUN_TEST(test_a_flicker_lights_a_dark_light);
    RUN_TEST(test_several_flickers_are_80_ms_apart);
    RUN_TEST(test_the_number_of_flickers_is_the_number_asked_up_to_eight);
    RUN_TEST(test_a_refusal_is_one_long_blink);
    RUN_TEST(test_a_hold_stage_that_rises_is_one_flicker);
    RUN_TEST(test_flickers_are_begun_in_place_of_a_hold_stage_flicker);
    RUN_TEST(test_the_steady_level_can_change_under_a_flicker);
    RUN_TEST(test_time_is_counted_in_elapsed_ticks);
    RUN_TEST(test_init_ends_a_flicker_and_forgets_the_hold_stage);
    RUN_TEST(test_null_pointers_do_nothing);
    return TEST_RESULT();
}
