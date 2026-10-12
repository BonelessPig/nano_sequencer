/**
 * @file   test_profile.c
 * @brief  Host unit tests for the profile button (core/profile.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include "profile.h"
#include "test_harness.h"

#define SETTLE_TICKS    (20U)    // The debounce time
#define SAVE_TICKS      (1000U)  // A hold this long saves
#define CLEAR_TICKS     (5000U)  // This long clears
#define CLEAR_ALL_TICKS (10000U) // And this long clears every profile

/**
 * @brief Runs one tick with a button that could be read.
 */
static profile_action_t tick(profile_state_t *p_state, bool b_button, uint8_t elapsed_ticks)
{
    profile_inputs_t in;

    in.b_button       = b_button;
    in.b_button_valid = true;
    in.elapsed_ticks  = elapsed_ticks;
    return profile_tick(p_state, &in);
}

/**
 * @brief Runs a number of ticks of one each with the button as given, and
 *        checks that nothing is asked for on any of them.
 */
static void quiet_ticks(profile_state_t *p_state, bool b_button, unsigned int count)
{
    for (unsigned int i = 0U; i < count; i++)
    {
        TEST_ASSERT_EQUAL(PROFILE_NONE, tick(p_state, b_button, 1U));
    }
}

/**
 * @brief A fresh state with the power-on recall already asked, the button up.
 */
static void start(profile_state_t *p_state)
{
    profile_init(p_state);
    TEST_ASSERT_EQUAL(PROFILE_RECALL, tick(p_state, false, 1U));
}

/**
 * @brief  Presses the button and lets it go. The debounce finds it down
 *         SETTLE_TICKS - 1 late and up as late again, the tick it is found
 *         up not being counted, so the hold comes to held_ticks exactly.
 * @return What the button asked for on the tick it was found let go.
 */
static profile_action_t hold(profile_state_t *p_state, unsigned int held_ticks)
{
    quiet_ticks(p_state, true, held_ticks);
    quiet_ticks(p_state, false, SETTLE_TICKS - 1U);
    return tick(p_state, false, 1U);
}



static void test_the_first_valid_tick_asks_for_the_power_on_recall_once(void)
{
    profile_state_t state;

    profile_init(&state);
    TEST_ASSERT_EQUAL(PROFILE_RECALL, tick(&state, false, 1U));
    quiet_ticks(&state, false, 3000U);
}



static void test_the_power_on_recall_waits_for_a_button_that_can_be_read(void)
{
    profile_state_t  state;
    profile_inputs_t in;

    in.b_button       = false;
    in.b_button_valid = false;
    in.elapsed_ticks  = 1U;

    profile_init(&state);
    for (unsigned int i = 0U; i < 100U; i++)
    {
        TEST_ASSERT_EQUAL(PROFILE_NONE, profile_tick(&state, &in));
    }

    in.b_button_valid = true;
    TEST_ASSERT_EQUAL(PROFILE_RECALL, profile_tick(&state, &in));
    TEST_ASSERT_EQUAL(PROFILE_NONE, profile_tick(&state, &in));

    // After that a button that cannot be read is taken as it stands
    in.b_button_valid = false;
    in.b_button       = true;
    for (unsigned int i = 0U; i < 1500U; i++)
    {
        TEST_ASSERT_EQUAL(PROFILE_NONE, profile_tick(&state, &in));
    }
    in.b_button      = false;
    in.elapsed_ticks = SETTLE_TICKS;
    TEST_ASSERT_EQUAL(PROFILE_SAVE, profile_tick(&state, &in));
}



static void test_a_button_down_at_power_on_is_held_from_then(void)
{
    profile_state_t state;

    // The first tick is the power-on recall; the hold is counted from the next
    profile_init(&state);
    TEST_ASSERT_EQUAL(PROFILE_RECALL, tick(&state, true, 1U));
    quiet_ticks(&state, true, SAVE_TICKS - SETTLE_TICKS);
    quiet_ticks(&state, false, SETTLE_TICKS - 1U);
    TEST_ASSERT_EQUAL(PROFILE_RECALL, tick(&state, false, 1U)); // One short of a second

    profile_init(&state);
    TEST_ASSERT_EQUAL(PROFILE_RECALL, tick(&state, true, 1U));
    quiet_ticks(&state, true, (SAVE_TICKS - SETTLE_TICKS) + 1U);
    quiet_ticks(&state, false, SETTLE_TICKS - 1U);
    TEST_ASSERT_EQUAL(PROFILE_SAVE, tick(&state, false, 1U));
}



static void test_a_click_recalls_once_it_is_let_go(void)
{
    profile_state_t state;

    start(&state);
    quiet_ticks(&state, false, 5U);
    quiet_ticks(&state, true, 100U);

    // Let go: the debounce waits out its time, and then it is a click
    quiet_ticks(&state, false, SETTLE_TICKS - 1U);
    TEST_ASSERT_EQUAL(PROFILE_RECALL, tick(&state, false, 1U));
    quiet_ticks(&state, false, 3000U); // And only the once
}



static void test_the_hold_decides_what_is_asked(void)
{
    static const struct
    {
        unsigned int     held_ticks;
        profile_action_t action;
    } cases[] =
    {
        { SETTLE_TICKS,          PROFILE_RECALL },
        { SAVE_TICKS - 1U,       PROFILE_RECALL },
        { SAVE_TICKS,            PROFILE_SAVE },
        { CLEAR_TICKS - 1U,      PROFILE_SAVE },
        { CLEAR_TICKS,           PROFILE_CLEAR },
        { CLEAR_ALL_TICKS - 1U,  PROFILE_CLEAR },
        { CLEAR_ALL_TICKS,       PROFILE_CLEAR_ALL },
        { 70000U,                PROFILE_CLEAR_ALL } // Past where 16 bits come round
    };

    for (size_t i = 0U; i < (sizeof(cases) / sizeof(cases[0])); i++)
    {
        profile_state_t state;

        start(&state);
        TEST_ASSERT_EQUAL(cases[i].action, hold(&state, cases[i].held_ticks));
    }
}



static void test_nothing_is_asked_while_the_button_is_down(void)
{
    profile_state_t state;

    start(&state);
    quiet_ticks(&state, true, 12000U);
}



static void test_a_bounce_shorter_than_the_debounce_is_no_press(void)
{
    profile_state_t state;

    start(&state);
    quiet_ticks(&state, true, SETTLE_TICKS - 1U);
    quiet_ticks(&state, false, 3000U);
}



static void test_a_bounce_as_it_is_let_go_does_not_ask_twice(void)
{
    profile_state_t state;

    start(&state);
    quiet_ticks(&state, true, 2000U);

    // Up, down and up again inside the debounce time: one save, no click
    quiet_ticks(&state, false, 5U);
    quiet_ticks(&state, true, 5U);
    quiet_ticks(&state, false, SETTLE_TICKS - 1U);
    TEST_ASSERT_EQUAL(PROFILE_SAVE, tick(&state, false, 1U));
    quiet_ticks(&state, false, 3000U);
}



static void test_each_press_is_counted_from_nothing(void)
{
    profile_state_t state;

    // A long hold, and then a click that is still a click
    start(&state);
    TEST_ASSERT_EQUAL(PROFILE_CLEAR, hold(&state, 6000U));
    TEST_ASSERT_EQUAL(PROFILE_RECALL, hold(&state, 100U));
}



static void test_the_hold_is_counted_in_elapsed_ticks(void)
{
    profile_state_t state;

    // Four passes of 250 ticks are a second
    start(&state);
    for (unsigned int pass = 0U; pass < 4U; pass++)
    {
        TEST_ASSERT_EQUAL(PROFILE_NONE, tick(&state, true, 250U));
    }
    TEST_ASSERT_EQUAL(PROFILE_SAVE, tick(&state, false, SETTLE_TICKS));

    // And a pass of none counts for nothing: a button found down with no
    // time gone by, and then up, was never held
    start(&state);
    TEST_ASSERT_EQUAL(PROFILE_NONE, tick(&state, true, SETTLE_TICKS));
    TEST_ASSERT_EQUAL(PROFILE_NONE, tick(&state, true, 0U));
    TEST_ASSERT_EQUAL(PROFILE_RECALL, tick(&state, false, SETTLE_TICKS));

    profile_init(&state);
    TEST_ASSERT_EQUAL(PROFILE_RECALL, tick(&state, true, 0U)); // Power-on
    TEST_ASSERT_EQUAL(PROFILE_NONE, tick(&state, true, 0U));
    TEST_ASSERT_EQUAL(PROFILE_NONE, tick(&state, false, SETTLE_TICKS));
}



static void test_init_forgets_a_hold_and_asks_for_the_recall_again(void)
{
    profile_state_t state;

    start(&state);
    quiet_ticks(&state, true, 2000U);
    profile_init(&state);
    TEST_ASSERT_EQUAL(PROFILE_RECALL, tick(&state, false, 1U));
    quiet_ticks(&state, false, 100U);
}



static void test_null_pointers_do_nothing(void)
{
    profile_state_t  state;
    profile_inputs_t in;

    in.b_button       = false;
    in.b_button_valid = true;
    in.elapsed_ticks  = 1U;

    profile_init(NULL);
    TEST_ASSERT_EQUAL(PROFILE_NONE, profile_tick(NULL, &in));

    // And the recall is still to be asked afterwards
    profile_init(&state);
    TEST_ASSERT_EQUAL(PROFILE_NONE, profile_tick(&state, NULL));
    TEST_ASSERT_EQUAL(PROFILE_RECALL, profile_tick(&state, &in));
}



int main(void)
{
    RUN_TEST(test_the_first_valid_tick_asks_for_the_power_on_recall_once);
    RUN_TEST(test_the_power_on_recall_waits_for_a_button_that_can_be_read);
    RUN_TEST(test_a_button_down_at_power_on_is_held_from_then);
    RUN_TEST(test_a_click_recalls_once_it_is_let_go);
    RUN_TEST(test_the_hold_decides_what_is_asked);
    RUN_TEST(test_nothing_is_asked_while_the_button_is_down);
    RUN_TEST(test_a_bounce_shorter_than_the_debounce_is_no_press);
    RUN_TEST(test_a_bounce_as_it_is_let_go_does_not_ask_twice);
    RUN_TEST(test_each_press_is_counted_from_nothing);
    RUN_TEST(test_the_hold_is_counted_in_elapsed_ticks);
    RUN_TEST(test_init_forgets_a_hold_and_asks_for_the_recall_again);
    RUN_TEST(test_null_pointers_do_nothing);
    return TEST_RESULT();
}
