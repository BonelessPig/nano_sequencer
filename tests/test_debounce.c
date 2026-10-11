/**
 * @file   test_debounce.c
 * @brief  Host unit tests for the debounce (core/debounce.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "debounce.h"
#include "test_harness.h"

#define POISON_BYTE  (0xA5)
#define SETTLE_TICKS (20U) // Ticks a new level must hold, as debounce.h says

/**
 * @brief Gives the same raw reading for a number of single ticks and returns
 *        the level after the last.
 */
static bool hold(debounce_state_t *p_state, bool b_raw, unsigned int ticks)
{
    bool b_level = false;

    for (unsigned int tick = 0U; tick < ticks; tick++)
    {
        b_level = debounce_update(p_state, b_raw, 1U);
    }
    return b_level;
}



static void test_init_forgets_the_level(void)
{
    debounce_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    debounce_init(&state);
    TEST_ASSERT(!state.b_known);
    TEST_ASSERT(!state.b_level);
    TEST_ASSERT_EQUAL(0, state.ticks);
}



static void test_the_first_reading_is_taken_as_it_stands(void)
{
    debounce_state_t state;

    debounce_init(&state);
    TEST_ASSERT(debounce_update(&state, true, 1U));

    debounce_init(&state);
    TEST_ASSERT(!debounce_update(&state, false, 1U));
}



static void test_a_new_level_must_hold_for_the_settle_time(void)
{
    debounce_state_t state;

    debounce_init(&state);
    (void)debounce_update(&state, false, 1U);

    TEST_ASSERT(!hold(&state, true, SETTLE_TICKS - 1U));
    TEST_ASSERT(debounce_update(&state, true, 1U));

    // And the same going back
    TEST_ASSERT(hold(&state, false, SETTLE_TICKS - 1U));
    TEST_ASSERT(!debounce_update(&state, false, 1U));
}



static void test_a_bounce_starts_the_count_again(void)
{
    debounce_state_t state;

    debounce_init(&state);
    (void)debounce_update(&state, false, 1U);

    TEST_ASSERT(!hold(&state, true, SETTLE_TICKS - 1U));
    TEST_ASSERT(!debounce_update(&state, false, 1U)); // One tick back
    TEST_ASSERT(!hold(&state, true, SETTLE_TICKS - 1U));
    TEST_ASSERT(debounce_update(&state, true, 1U));
}



static void test_a_short_pulse_is_ignored(void)
{
    debounce_state_t state;

    debounce_init(&state);
    (void)debounce_update(&state, true, 1U);

    TEST_ASSERT(hold(&state, false, 5U));
    TEST_ASSERT(hold(&state, true, 100U));
    TEST_ASSERT_EQUAL(0, state.ticks);
}



static void test_elapsed_ticks_count_towards_the_settle_time(void)
{
    debounce_state_t state;

    debounce_init(&state);
    (void)debounce_update(&state, false, 1U);

    TEST_ASSERT(!debounce_update(&state, true, 12U));
    TEST_ASSERT(!debounce_update(&state, true, 7U));
    TEST_ASSERT(debounce_update(&state, true, 1U));

    // A whole stall in one call settles at once, and does not overflow
    TEST_ASSERT(debounce_update(&state, false, 19U));
    TEST_ASSERT(!debounce_update(&state, false, 255U));
}



static void test_no_elapsed_ticks_change_nothing(void)
{
    debounce_state_t state;

    debounce_init(&state);
    (void)debounce_update(&state, false, 1U);
    (void)hold(&state, true, 5U);

    TEST_ASSERT(!debounce_update(&state, true, 0U));
    TEST_ASSERT_EQUAL(5, state.ticks);
}



static void test_null_state_is_ignored(void)
{
    debounce_init(NULL); // Must not crash
    TEST_ASSERT(!debounce_update(NULL, true, 255U));
}



int main(void)
{
    RUN_TEST(test_init_forgets_the_level);
    RUN_TEST(test_the_first_reading_is_taken_as_it_stands);
    RUN_TEST(test_a_new_level_must_hold_for_the_settle_time);
    RUN_TEST(test_a_bounce_starts_the_count_again);
    RUN_TEST(test_a_short_pulse_is_ignored);
    RUN_TEST(test_elapsed_ticks_count_towards_the_settle_time);
    RUN_TEST(test_no_elapsed_ticks_change_nothing);
    RUN_TEST(test_null_state_is_ignored);
    return TEST_RESULT();
}
