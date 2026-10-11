/**
 * @file   test_mod.c
 * @brief  Host unit tests for the modulation (core/mod.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "mod.h"
#include "test_harness.h"

#define POISON_BYTE (0xA5)
#define CENTRE      (512U)  // Where the signal does nothing, as mod.h says
#define DEAD_ZONE   (16U)   // How far from it still does nothing
#define TOP         (1023U) // The top of an amount and of the signal

// The generator's first bytes from this seed, as test_prng.c has them:
// 135, 35, 70, 220, 176, 221, 238, 248
#define SEED (0xACE1U)

/**
 * @brief A depth, for the tests of what is done with one.
 */
static mod_depth_t depth_of(uint8_t depth, bool b_negative)
{
    mod_depth_t made;

    made.depth      = depth;
    made.b_negative = b_negative;
    return made;
}



static void test_the_centre_and_an_amount_of_0_are_no_depth(void)
{
    mod_depth_t depth;

    (void)memset(&depth, POISON_BYTE, sizeof(depth));
    mod_depth(TOP, CENTRE, &depth);
    TEST_ASSERT_EQUAL(0, depth.depth);
    TEST_ASSERT(!depth.b_negative);

    (void)memset(&depth, POISON_BYTE, sizeof(depth));
    mod_depth(0U, TOP, &depth);
    TEST_ASSERT_EQUAL(0, depth.depth);
    TEST_ASSERT(!depth.b_negative);

    mod_depth(0U, 0U, &depth);
    TEST_ASSERT_EQUAL(0, depth.depth);
    TEST_ASSERT(depth.b_negative);
}



static void test_a_signal_near_the_centre_is_no_depth(void)
{
    mod_depth_t depth;

    // An input with nothing plugged in, and its noise
    mod_depth(TOP, CENTRE + DEAD_ZONE - 1U, &depth);
    TEST_ASSERT_EQUAL(0, depth.depth);
    mod_depth(TOP, CENTRE - DEAD_ZONE + 1U, &depth);
    TEST_ASSERT_EQUAL(0, depth.depth);
    TEST_ASSERT(depth.b_negative);

    // The first reading outside it counts: 16 of 511 at full amount
    mod_depth(TOP, CENTRE + DEAD_ZONE, &depth);
    TEST_ASSERT_EQUAL(8, depth.depth);
    TEST_ASSERT(!depth.b_negative);
    mod_depth(TOP, CENTRE - DEAD_ZONE, &depth);
    TEST_ASSERT_EQUAL(8, depth.depth);
    TEST_ASSERT(depth.b_negative);
}



static void test_the_depth_is_the_swing_times_the_amount(void)
{
    mod_depth_t depth;

    // Either end of the swing at full amount is the most there is
    mod_depth(TOP, TOP, &depth);
    TEST_ASSERT_EQUAL(255, depth.depth);
    TEST_ASSERT(!depth.b_negative);
    mod_depth(TOP, 0U, &depth);
    TEST_ASSERT_EQUAL(255, depth.depth);
    TEST_ASSERT(depth.b_negative);

    // Half the swing, or half the amount, is half of it
    mod_depth(TOP, CENTRE + 256U, &depth);
    TEST_ASSERT_EQUAL(128, depth.depth);
    mod_depth(TOP, CENTRE - 256U, &depth);
    TEST_ASSERT_EQUAL(128, depth.depth);
    TEST_ASSERT(depth.b_negative);
    mod_depth(511U, TOP, &depth);
    TEST_ASSERT_EQUAL(127, depth.depth);

    // And half of each a quarter
    mod_depth(511U, CENTRE + 256U, &depth);
    TEST_ASSERT_EQUAL(63, depth.depth);
}



static void test_readings_above_the_top_are_the_top(void)
{
    mod_depth_t depth;

    mod_depth(5000U, 5000U, &depth);
    TEST_ASSERT_EQUAL(255, depth.depth);
    TEST_ASSERT(!depth.b_negative);
}



static void test_a_shift_moves_in_proportion_to_the_depth(void)
{
    mod_depth_t depth;

    // The whole range at the most depth, up or down
    depth = depth_of(255U, false);
    TEST_ASSERT_EQUAL(107, mod_shift(100U, &depth, 7U, 0U, 255U));
    depth = depth_of(255U, true);
    TEST_ASSERT_EQUAL(93, mod_shift(100U, &depth, 7U, 0U, 255U));

    // Half of it at half the depth, rounded to the nearest: 3.5 is 4
    depth = depth_of(128U, false);
    TEST_ASSERT_EQUAL(104, mod_shift(100U, &depth, 7U, 0U, 255U));
    depth = depth_of(128U, true);
    TEST_ASSERT_EQUAL(96, mod_shift(100U, &depth, 7U, 0U, 255U));

    // The least depth that comes to a whole step, and the one below it
    depth = depth_of(19U, false);
    TEST_ASSERT_EQUAL(101, mod_shift(100U, &depth, 7U, 0U, 255U));
    depth = depth_of(18U, false);
    TEST_ASSERT_EQUAL(100, mod_shift(100U, &depth, 7U, 0U, 255U));
}



static void test_a_shift_stays_within_its_limits(void)
{
    mod_depth_t depth;

    depth = depth_of(255U, false);
    TEST_ASSERT_EQUAL(15, mod_shift(12U, &depth, 7U, 1U, 15U));
    TEST_ASSERT_EQUAL(255, mod_shift(250U, &depth, 255U, 0U, 255U));
    depth = depth_of(255U, true);
    TEST_ASSERT_EQUAL(1, mod_shift(5U, &depth, 7U, 1U, 15U));
    TEST_ASSERT_EQUAL(0, mod_shift(5U, &depth, 7U, 0U, 15U));
    TEST_ASSERT_EQUAL(1, mod_shift(7U, &depth, 7U, 1U, 15U)); // Exactly to 0

    // A value outside them is brought in when it is moved
    depth = depth_of(255U, false);
    TEST_ASSERT_EQUAL(15, mod_shift(200U, &depth, 1U, 1U, 15U));

    // Limits the wrong way round: the upper one is used
    TEST_ASSERT_EQUAL(4, mod_shift(10U, &depth, 1U, 9U, 4U));
}



static void test_a_shift_of_nothing_leaves_the_value_as_given(void)
{
    mod_depth_t depth = depth_of(0U, false);

    // Limits or no limits
    TEST_ASSERT_EQUAL(200, mod_shift(200U, &depth, 7U, 1U, 15U));
    TEST_ASSERT_EQUAL(0, mod_shift(0U, &depth, 7U, 1U, 15U));

    // A range of 0 is nothing too
    depth = depth_of(255U, false);
    TEST_ASSERT_EQUAL(200, mod_shift(200U, &depth, 0U, 1U, 15U));

    TEST_ASSERT_EQUAL(200, mod_shift(200U, NULL, 7U, 1U, 15U));
}



static void test_a_chance_comes_up_as_often_as_the_depth_says(void)
{
    static const uint8_t depths[] = { 1U, 64U, 128U, 255U };
    prng_state_t         prng;

    // Over the generator's whole run every byte value but one comes 256
    // times and 0 comes 255, so the count is exact
    for (size_t i = 0U; i < (sizeof(depths) / sizeof(depths[0])); i++)
    {
        const mod_depth_t depth = depth_of(depths[i], false);
        unsigned long     hits  = 0UL;

        prng_init(&prng, SEED);
        for (unsigned long draw = 0UL; draw < 65535UL; draw++)
        {
            hits += mod_chance(&depth, &prng) ? 1UL : 0UL;
        }
        TEST_ASSERT_EQUAL(256UL * depths[i], hits);
    }
}



static void test_a_chance_takes_the_top_draws(void)
{
    // Of 135, 35, 70, 220, 176, 221, 238, 248: those above 255 - 128
    static const bool  expected[] = { true, false, false, true, true, true, true, true };
    const mod_depth_t  depth      = depth_of(128U, false);
    prng_state_t       prng;

    prng_init(&prng, SEED);
    for (size_t i = 0U; i < (sizeof(expected) / sizeof(expected[0])); i++)
    {
        TEST_ASSERT_EQUAL(expected[i], mod_chance(&depth, &prng));
    }
}



static void test_no_chance_draws_nothing(void)
{
    mod_depth_t  depth;
    prng_state_t prng;
    prng_state_t untouched;

    prng_init(&prng, SEED);
    untouched = prng;

    // No depth, or a signal below its centre
    depth = depth_of(0U, false);
    TEST_ASSERT(!mod_chance(&depth, &prng));
    depth = depth_of(255U, true);
    TEST_ASSERT(!mod_chance(&depth, &prng));
    TEST_ASSERT(!mod_chance(NULL, &prng));
    TEST_ASSERT_EQUAL(untouched.value, prng.value);
}



static void test_a_chance_with_no_generator_never_comes_up(void)
{
    const mod_depth_t depth = depth_of(255U, false);

    TEST_ASSERT(!mod_chance(&depth, NULL));
}



static void test_a_null_depth_is_ignored(void)
{
    mod_depth(TOP, TOP, NULL); // Must not crash
}



int main(void)
{
    RUN_TEST(test_the_centre_and_an_amount_of_0_are_no_depth);
    RUN_TEST(test_a_signal_near_the_centre_is_no_depth);
    RUN_TEST(test_the_depth_is_the_swing_times_the_amount);
    RUN_TEST(test_readings_above_the_top_are_the_top);
    RUN_TEST(test_a_shift_moves_in_proportion_to_the_depth);
    RUN_TEST(test_a_shift_stays_within_its_limits);
    RUN_TEST(test_a_shift_of_nothing_leaves_the_value_as_given);
    RUN_TEST(test_a_chance_comes_up_as_often_as_the_depth_says);
    RUN_TEST(test_a_chance_takes_the_top_draws);
    RUN_TEST(test_no_chance_draws_nothing);
    RUN_TEST(test_a_chance_with_no_generator_never_comes_up);
    RUN_TEST(test_a_null_depth_is_ignored);
    return TEST_RESULT();
}
