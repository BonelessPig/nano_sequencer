/**
 * @file   test_address.c
 * @brief  Host unit tests for step addressing (core/address.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "address.h"
#include "panel.h"
#include "test_harness.h"

#define POISON_BYTE (0xA5)
#define NO_STEP     (0xEEU) // What p_step holds if address_next() did not write it

/**
 * @brief The controls for a looping pattern over a range.
 */
static address_config_t make_config(address_direction_t direction, uint8_t first_step,
                                    uint8_t last_step)
{
    address_config_t config;

    (void)memset(&config, 0, sizeof(config));
    config.direction  = direction;
    config.first_step = first_step;
    config.last_step  = last_step;
    config.b_one_shot = false;
    return config;
}



/**
 * @brief Checks that the next steps played are exactly the expected ones.
 */
static void expect_steps(address_state_t *p_state, const address_config_t *p_config,
                         const uint8_t *p_expected, size_t count)
{
    for (size_t i = 0U; i < count; i++)
    {
        uint8_t step = NO_STEP;

        TEST_ASSERT(address_next(p_state, p_config, &step));
        TEST_ASSERT_EQUAL(p_expected[i], step);
    }
}

#define EXPECT_STEPS(p_state, p_config, expected) \
    expect_steps((p_state), (p_config), (expected), sizeof(expected) / sizeof((expected)[0]))



/**
 * @brief Checks that no step is played, and that p_step is left alone.
 */
static void expect_no_step(address_state_t *p_state, const address_config_t *p_config)
{
    uint8_t step = NO_STEP;

    TEST_ASSERT(!address_next(p_state, p_config, &step));
    TEST_ASSERT_EQUAL(NO_STEP, step);
}



static void test_init_goes_to_the_start_of_the_cycle(void)
{
    address_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    address_init(&state);
    TEST_ASSERT_EQUAL(0, state.position);
    TEST_ASSERT(!state.b_finished);
}



static void test_forward_plays_all_sixteen_steps_and_wraps(void)
{
    const address_config_t config = make_config(ADDRESS_FORWARD, 0U, 15U);
    address_state_t        state;

    address_init(&state);
    for (unsigned int count = 0U; count < (3U * PANEL_STEP_COUNT); count++)
    {
        uint8_t step = NO_STEP;

        TEST_ASSERT(address_next(&state, &config, &step));
        TEST_ASSERT_EQUAL(count % PANEL_STEP_COUNT, step);
    }
}



static void test_forward_stays_within_the_range(void)
{
    static const uint8_t   expected[] = { 4U, 5U, 6U, 7U, 4U, 5U, 6U, 7U, 4U };
    const address_config_t config = make_config(ADDRESS_FORWARD, 4U, 7U);
    address_state_t        state;

    address_init(&state);
    EXPECT_STEPS(&state, &config, expected);
}



static void test_reverse_plays_last_to_first(void)
{
    static const uint8_t   expected[] = { 7U, 6U, 5U, 4U, 7U, 6U, 5U, 4U, 7U };
    const address_config_t config = make_config(ADDRESS_REVERSE, 4U, 7U);
    address_state_t        state;

    address_init(&state);
    EXPECT_STEPS(&state, &config, expected);
}



static void test_pendulum_plays_each_end_once_per_cycle(void)
{
    static const uint8_t   expected[] = { 4U, 5U, 6U, 7U, 6U, 5U, 4U, 5U, 6U, 7U, 6U, 5U, 4U };
    const address_config_t config = make_config(ADDRESS_PENDULUM, 4U, 7U);
    address_state_t        state;

    address_init(&state);
    EXPECT_STEPS(&state, &config, expected);
}



static void test_pendulum_over_the_whole_panel_has_a_thirty_step_cycle(void)
{
    const address_config_t config = make_config(ADDRESS_PENDULUM, 0U, 15U);
    address_state_t        state;

    address_init(&state);
    for (unsigned int count = 0U; count < 61U; count++)
    {
        const unsigned int in_cycle = count % 30U;
        const unsigned int expected = (in_cycle <= 15U) ? in_cycle : (30U - in_cycle);
        uint8_t            step     = NO_STEP;

        TEST_ASSERT(address_next(&state, &config, &step));
        TEST_ASSERT_EQUAL(expected, step);
    }
}



static void test_pendulum_over_two_steps_alternates(void)
{
    static const uint8_t   expected[] = { 9U, 10U, 9U, 10U, 9U };
    const address_config_t config = make_config(ADDRESS_PENDULUM, 9U, 10U);
    address_state_t        state;

    address_init(&state);
    EXPECT_STEPS(&state, &config, expected);
}



static void test_one_step_range_repeats_that_step_in_every_direction(void)
{
    static const address_direction_t directions[] =
    {
        ADDRESS_FORWARD, ADDRESS_REVERSE, ADDRESS_PENDULUM
    };
    static const uint8_t expected[] = { 6U, 6U, 6U, 6U };

    for (size_t i = 0U; i < (sizeof(directions) / sizeof(directions[0])); i++)
    {
        const address_config_t config = make_config(directions[i], 6U, 6U);
        address_state_t        state;

        address_init(&state);
        EXPECT_STEPS(&state, &config, expected);
    }
}



static void test_first_above_last_runs_round_through_step_zero(void)
{
    static const uint8_t forward[]  = { 14U, 15U, 0U, 1U, 14U, 15U };
    static const uint8_t reverse[]  = { 1U, 0U, 15U, 14U, 1U, 0U };
    static const uint8_t pendulum[] = { 14U, 15U, 0U, 1U, 0U, 15U, 14U, 15U };
    address_config_t     config = make_config(ADDRESS_FORWARD, 14U, 1U);
    address_state_t      state;

    address_init(&state);
    EXPECT_STEPS(&state, &config, forward);

    config.direction = ADDRESS_REVERSE;
    address_init(&state);
    EXPECT_STEPS(&state, &config, reverse);

    config.direction = ADDRESS_PENDULUM;
    address_init(&state);
    EXPECT_STEPS(&state, &config, pendulum);
}



static void test_first_one_above_last_is_the_whole_panel(void)
{
    // First 5, last 4: sixteen steps starting at 5
    const address_config_t config = make_config(ADDRESS_FORWARD, 5U, 4U);
    address_state_t        state;

    address_init(&state);
    for (unsigned int count = 0U; count < (2U * PANEL_STEP_COUNT); count++)
    {
        uint8_t step = NO_STEP;

        TEST_ASSERT(address_next(&state, &config, &step));
        TEST_ASSERT_EQUAL((count + 5U) % PANEL_STEP_COUNT, step);
    }
}



static void test_steps_beyond_the_panel_wrap(void)
{
    // 20 is step 4 and 23 is step 7
    static const uint8_t   expected[] = { 4U, 5U, 6U, 7U, 4U };
    const address_config_t config = make_config(ADDRESS_FORWARD, 20U, 23U);
    address_state_t        state;

    address_init(&state);
    EXPECT_STEPS(&state, &config, expected);
}



static void test_unknown_direction_plays_forward(void)
{
    static const uint8_t   expected[] = { 4U, 5U, 6U, 7U, 4U };
    const address_config_t config = make_config((address_direction_t)99, 4U, 7U);
    address_state_t        state;

    address_init(&state);
    EXPECT_STEPS(&state, &config, expected);
}



static void test_one_shot_stops_after_one_cycle(void)
{
    static const uint8_t forward[]  = { 4U, 5U, 6U, 7U };
    static const uint8_t reverse[]  = { 7U, 6U, 5U, 4U };
    static const uint8_t pendulum[] = { 4U, 5U, 6U, 7U, 6U, 5U };
    address_config_t     config = make_config(ADDRESS_FORWARD, 4U, 7U);
    address_state_t      state;

    config.b_one_shot = true;

    address_init(&state);
    EXPECT_STEPS(&state, &config, forward);
    expect_no_step(&state, &config);
    expect_no_step(&state, &config); // And it stays stopped

    config.direction = ADDRESS_REVERSE;
    address_init(&state);
    EXPECT_STEPS(&state, &config, reverse);
    expect_no_step(&state, &config);

    config.direction = ADDRESS_PENDULUM;
    address_init(&state);
    EXPECT_STEPS(&state, &config, pendulum);
    expect_no_step(&state, &config);
}



static void test_one_shot_over_one_step_plays_it_once(void)
{
    static const uint8_t expected[] = { 6U };
    address_config_t     config = make_config(ADDRESS_PENDULUM, 6U, 6U);
    address_state_t      state;

    config.b_one_shot = true;
    address_init(&state);
    EXPECT_STEPS(&state, &config, expected);
    expect_no_step(&state, &config);
}



static void test_init_plays_a_finished_one_shot_again(void)
{
    static const uint8_t expected[] = { 4U, 5U, 6U, 7U };
    address_config_t     config = make_config(ADDRESS_FORWARD, 4U, 7U);
    address_state_t      state;

    config.b_one_shot = true;
    address_init(&state);
    EXPECT_STEPS(&state, &config, expected);
    expect_no_step(&state, &config);

    address_init(&state);
    EXPECT_STEPS(&state, &config, expected);
    expect_no_step(&state, &config);
}



static void test_init_mid_cycle_goes_back_to_the_start(void)
{
    static const uint8_t   part[]     = { 4U, 5U };
    static const uint8_t   expected[] = { 4U, 5U, 6U, 7U, 4U };
    const address_config_t config = make_config(ADDRESS_FORWARD, 4U, 7U);
    address_state_t        state;

    address_init(&state);
    EXPECT_STEPS(&state, &config, part);
    address_init(&state);
    EXPECT_STEPS(&state, &config, expected);
}



static void test_clearing_one_shot_starts_the_next_cycle(void)
{
    static const uint8_t expected[] = { 4U, 5U, 6U, 7U };
    address_config_t     config = make_config(ADDRESS_FORWARD, 4U, 7U);
    address_state_t      state;

    config.b_one_shot = true;
    address_init(&state);
    EXPECT_STEPS(&state, &config, expected);
    expect_no_step(&state, &config);

    // Back to looping: it carries on from the start of the range
    config.b_one_shot = false;
    EXPECT_STEPS(&state, &config, expected);
    EXPECT_STEPS(&state, &config, expected);
}



static void test_setting_one_shot_mid_cycle_stops_at_the_end_of_it(void)
{
    static const uint8_t part[] = { 4U, 5U, 6U, 7U, 4U, 5U };
    static const uint8_t rest[] = { 6U, 7U };
    address_config_t     config = make_config(ADDRESS_FORWARD, 4U, 7U);
    address_state_t      state;

    address_init(&state);
    EXPECT_STEPS(&state, &config, part);
    config.b_one_shot = true;
    EXPECT_STEPS(&state, &config, rest);
    expect_no_step(&state, &config);
}



static void test_changing_direction_mid_cycle_keeps_the_count(void)
{
    static const uint8_t forward[] = { 0U, 1U, 2U };
    static const uint8_t reverse[] = { 12U, 11U };
    address_config_t     config = make_config(ADDRESS_FORWARD, 0U, 15U);
    address_state_t      state;

    // Three steps played; the fourth of a reverse cycle is step 12
    address_init(&state);
    EXPECT_STEPS(&state, &config, forward);
    config.direction = ADDRESS_REVERSE;
    EXPECT_STEPS(&state, &config, reverse);
}



static void test_shortening_the_range_past_the_count_starts_the_cycle_again(void)
{
    static const uint8_t before[] = { 0U, 1U, 2U, 3U, 4U, 5U };
    static const uint8_t after[]  = { 0U, 1U, 2U, 3U, 0U };
    address_config_t     config = make_config(ADDRESS_FORWARD, 0U, 15U);
    address_state_t      state;

    address_init(&state);
    EXPECT_STEPS(&state, &config, before);
    config.last_step = 3U;
    EXPECT_STEPS(&state, &config, after);
}



static void test_shortening_a_one_shot_range_does_not_finish_it_early(void)
{
    static const uint8_t before[] = { 0U, 1U, 2U, 3U, 4U, 5U };
    static const uint8_t after[]  = { 0U, 1U, 2U, 3U };
    address_config_t     config = make_config(ADDRESS_FORWARD, 0U, 15U);
    address_state_t      state;

    config.b_one_shot = true;
    address_init(&state);
    EXPECT_STEPS(&state, &config, before);
    config.last_step = 3U;
    EXPECT_STEPS(&state, &config, after); // The new range is played in full
    expect_no_step(&state, &config);
}



static void test_next_does_not_change_the_config(void)
{
    const address_config_t before = make_config(ADDRESS_PENDULUM, 3U, 9U);
    address_config_t       config = before;
    address_state_t        state;
    uint8_t                step;

    address_init(&state);
    (void)address_next(&state, &config, &step);
    TEST_ASSERT_EQUAL(0, memcmp(&before, &config, sizeof(config)));
}



static void test_null_pointers_are_ignored(void)
{
    const address_config_t config = make_config(ADDRESS_FORWARD, 0U, 15U);
    address_state_t        state;
    uint8_t                step = NO_STEP;

    address_init(NULL); // Must not crash

    address_init(&state);
    TEST_ASSERT(!address_next(NULL, &config, &step));
    TEST_ASSERT(!address_next(&state, NULL, &step));
    TEST_ASSERT(!address_next(&state, &config, NULL));
    TEST_ASSERT_EQUAL(NO_STEP, step);
    TEST_ASSERT_EQUAL(0, state.position); // State untouched
}



int main(void)
{
    RUN_TEST(test_init_goes_to_the_start_of_the_cycle);
    RUN_TEST(test_forward_plays_all_sixteen_steps_and_wraps);
    RUN_TEST(test_forward_stays_within_the_range);
    RUN_TEST(test_reverse_plays_last_to_first);
    RUN_TEST(test_pendulum_plays_each_end_once_per_cycle);
    RUN_TEST(test_pendulum_over_the_whole_panel_has_a_thirty_step_cycle);
    RUN_TEST(test_pendulum_over_two_steps_alternates);
    RUN_TEST(test_one_step_range_repeats_that_step_in_every_direction);
    RUN_TEST(test_first_above_last_runs_round_through_step_zero);
    RUN_TEST(test_first_one_above_last_is_the_whole_panel);
    RUN_TEST(test_steps_beyond_the_panel_wrap);
    RUN_TEST(test_unknown_direction_plays_forward);
    RUN_TEST(test_one_shot_stops_after_one_cycle);
    RUN_TEST(test_one_shot_over_one_step_plays_it_once);
    RUN_TEST(test_init_plays_a_finished_one_shot_again);
    RUN_TEST(test_init_mid_cycle_goes_back_to_the_start);
    RUN_TEST(test_clearing_one_shot_starts_the_next_cycle);
    RUN_TEST(test_setting_one_shot_mid_cycle_stops_at_the_end_of_it);
    RUN_TEST(test_changing_direction_mid_cycle_keeps_the_count);
    RUN_TEST(test_shortening_the_range_past_the_count_starts_the_cycle_again);
    RUN_TEST(test_shortening_a_one_shot_range_does_not_finish_it_early);
    RUN_TEST(test_next_does_not_change_the_config);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
