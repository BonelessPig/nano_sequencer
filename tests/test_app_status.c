/**
 * @file   test_app_status.c
 * @brief  Host tests for the application loop (app/app_profile.c): the
 *         status light. The real app and core run against the fake ports in
 *         adapters/host/.
 * @author BonelessPig
 *
 * The light is worked out on one tick and written on the next, and the
 * button is seen at a scan and debounced, so these tests run on well past
 * each thing and count the times the light changed: two for each flicker
 * over a level that stays, and one fewer where what was done changed the
 * level under it.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "app.h"
#include "host_ports.h"
#include "log_port.h"
#include "test_harness.h"
#include "app_fixture.h"
#include "profile_fixture.h"

#define START_TICKS  (52U)   // What start_with() runs
#define HOLD_SAVE    (2000U) // Holds that end well clear of a wink
#define HOLD_CLEAR   (6000U)
#define HOLD_ALL     (10500U)
#define RUN_ON_TICKS (900U)  // Longer than any run of flickers

/**
 * @brief The power coming on with some profiles stored, a bit each, and the
 *        selector at a position; then the pattern stopped, so that a long
 *        hold does not fill the event record with steps. By the end every
 *        profile has been looked at.
 */
static void start_with(uint8_t stored, uint8_t position)
{
    host_reset();
    for (uint8_t profile = 0U; profile < 8U; profile++)
    {
        if (0U != (stored & (1U << profile)))
        {
            store_profile(profile);
        }
    }
    (void)app_init();
    set_selector_and_pot(position, TEMPO_RAW_150_BPM);
    run_ticks(2U);
    host_set_run_button(true);
    run_ticks(SETTLE_TICKS + 5U);
    host_set_run_button(false);
    run_ticks(SETTLE_TICKS + 5U);
}

/**
 * @brief Holds the profile button down with the selector at a position.
 */
static void hold_button(uint8_t position, unsigned int hold_ticks)
{
    set_selector_and_pot((uint8_t)(position | BUTTON_BIT), TEMPO_RAW_150_BPM);
    run_ticks(hold_ticks);
}

/**
 * @brief Lets the button go and runs on until the light has said its piece.
 */
static void let_go(uint8_t position)
{
    set_selector_and_pot(position, TEMPO_RAW_150_BPM);
    run_ticks(RUN_ON_TICKS);
}



static void test_the_light_is_dark_with_nothing_stored_and_written_on_every_tick(void)
{
    start_with(0x00U, 3U);
    TEST_ASSERT_EQUAL(false, host_status_lit());
    TEST_ASSERT_EQUAL(0, host_status_change_count());
    TEST_ASSERT_EQUAL(START_TICKS, host_status_write_count());

    run_ticks(100U);
    TEST_ASSERT_EQUAL(START_TICKS + 100U, host_status_write_count());
    TEST_ASSERT_EQUAL(0, host_status_change_count());
}



static void test_the_light_is_lit_while_the_selected_profile_holds_something(void)
{
    start_with(0x48U, 3U); // Profiles 3 and 6
    TEST_ASSERT_EQUAL(true, host_status_lit());

    set_selector_and_pot(4U, TEMPO_RAW_150_BPM);
    run_ticks(2U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(false, host_status_lit());

    set_selector_and_pot(6U, TEMPO_RAW_150_BPM);
    run_ticks(2U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(true, host_status_lit());
    TEST_ASSERT_EQUAL(3, host_status_change_count());
}



static void test_the_power_on_recall_says_nothing(void)
{
    // Whether it found a profile
    start_with(0x08U, 3U);
    run_ticks(RUN_ON_TICKS);
    TEST_ASSERT_EQUAL(1, count_events(HOST_EVENT_PROFILE, 3U, ACTION_RECALL));
    TEST_ASSERT_EQUAL(true, host_status_lit());
    TEST_ASSERT_EQUAL(1, host_status_change_count()); // Lit, and no more

    // Or not
    start_with(0x10U, 3U);
    run_ticks(RUN_ON_TICKS);
    TEST_ASSERT_EQUAL(0, host_status_change_count());
}



static void test_a_recall_flickers_once(void)
{
    start_with(0x20U, 5U);
    TEST_ASSERT_EQUAL(1, host_status_change_count());

    hold_button(5U, CLICK_TICKS);
    let_go(5U);
    TEST_ASSERT_EQUAL(2, count_events(HOST_EVENT_PROFILE, 5U, ACTION_RECALL));
    TEST_ASSERT_EQUAL(3, host_status_change_count());
    TEST_ASSERT_EQUAL(true, host_status_lit());
}



static void test_a_recall_that_finds_nothing_is_one_long_blink(void)
{
    start_with(0x00U, 5U);

    hold_button(5U, CLICK_TICKS);
    set_selector_and_pot(5U, TEMPO_RAW_150_BPM);
    run_ticks(SETTLE_TICKS + (2U * SCAN_PERIOD_TICKS));
    TEST_ASSERT_EQUAL(true, host_status_lit());
    run_ticks(400U); // Still, where a flicker would long be over
    TEST_ASSERT_EQUAL(true, host_status_lit());
    TEST_ASSERT_EQUAL(1, host_status_change_count());

    run_ticks(200U);
    TEST_ASSERT_EQUAL(false, host_status_lit());
    run_ticks(RUN_ON_TICKS);
    TEST_ASSERT_EQUAL(2, host_status_change_count());
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_PROFILE));
}



static void test_a_recall_that_cannot_be_read_is_one_long_blink(void)
{
    start_with(0x20U, 5U);
    host_set_nv_read_status(ERR_TIMEOUT);

    hold_button(5U, CLICK_TICKS);
    set_selector_and_pot(5U, TEMPO_RAW_150_BPM);
    run_ticks(SETTLE_TICKS + (2U * SCAN_PERIOD_TICKS) + 400U);
    TEST_ASSERT_EQUAL(false, host_status_lit());
    TEST_ASSERT_EQUAL(2, host_status_change_count());

    run_ticks(RUN_ON_TICKS);
    TEST_ASSERT_EQUAL(true, host_status_lit());
    TEST_ASSERT_EQUAL(3, host_status_change_count());
}



static void test_a_save_winks_at_a_second_and_flickers_twice(void)
{
    start_with(0x00U, 5U);

    // The wink: lit and dark again, while the button is still down
    hold_button(5U, HOLD_SAVE);
    TEST_ASSERT_EQUAL(2, host_status_change_count());
    TEST_ASSERT_EQUAL(false, host_status_lit());
    TEST_ASSERT_EQUAL(0, host_nv_write_count());

    // The profile holds something from here, so the first flicker begins
    // dark, as the light was, and the second ends lit
    let_go(5U);
    TEST_ASSERT_EQUAL(1, host_nv_write_count());
    TEST_ASSERT_EQUAL(5, host_status_change_count());
    TEST_ASSERT_EQUAL(true, host_status_lit());

    // Lit for that profile and no other
    set_selector_and_pot(4U, TEMPO_RAW_150_BPM);
    run_ticks(2U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(false, host_status_lit());
}



static void test_a_save_that_cannot_be_started_is_one_long_blink(void)
{
    start_with(0x00U, 5U);
    host_set_nv_write_status(ERR_BUSY);

    hold_button(5U, HOLD_SAVE);
    TEST_ASSERT_EQUAL(2, host_status_change_count());

    set_selector_and_pot(5U, TEMPO_RAW_150_BPM);
    run_ticks(SETTLE_TICKS + (2U * SCAN_PERIOD_TICKS) + 400U);
    TEST_ASSERT_EQUAL(true, host_status_lit());
    TEST_ASSERT_EQUAL(3, host_status_change_count());

    run_ticks(RUN_ON_TICKS);
    TEST_ASSERT_EQUAL(false, host_status_lit()); // And the profile holds nothing
    TEST_ASSERT_EQUAL(4, host_status_change_count());
}



static void test_a_clear_winks_twice_and_flickers_three_times(void)
{
    start_with(0x40U, 6U);
    TEST_ASSERT_EQUAL(1, host_status_change_count());

    hold_button(6U, HOLD_CLEAR);
    TEST_ASSERT_EQUAL(5, host_status_change_count()); // At 1 s and at 5 s
    TEST_ASSERT_EQUAL(true, host_status_lit());

    // The profile holds nothing from here, so the first flicker begins lit,
    // as the light was, and the third ends dark
    let_go(6U);
    TEST_ASSERT_EQUAL(1, count_events(HOST_EVENT_PROFILE, 6U, ACTION_CLEAR));
    TEST_ASSERT_EQUAL(10, host_status_change_count());
    TEST_ASSERT_EQUAL(false, host_status_lit());
}



static void test_a_clear_of_all_winks_three_times_and_flickers_four(void)
{
    start_with(0xFFU, 2U);
    TEST_ASSERT_EQUAL(1, host_status_change_count());

    hold_button(2U, HOLD_ALL);
    TEST_ASSERT_EQUAL(7, host_status_change_count()); // At 1 s, 5 s and 10 s
    TEST_ASSERT_EQUAL(true, host_status_lit());

    let_go(2U);
    TEST_ASSERT_EQUAL(1, count_events(HOST_EVENT_PROFILE, 2U, ACTION_CLEAR_ALL));
    TEST_ASSERT_EQUAL(14, host_status_change_count());
    TEST_ASSERT_EQUAL(false, host_status_lit());

    // And no other profile holds anything either
    for (uint8_t position = 0U; position < 8U; position++)
    {
        set_selector_and_pot(position, TEMPO_RAW_150_BPM);
        run_ticks(2U * SCAN_PERIOD_TICKS);
        TEST_ASSERT_EQUAL(false, host_status_lit());
    }
}



static void test_a_profile_that_cannot_be_read_at_power_on_is_logged_and_taken_as_empty(void)
{
    host_reset();
    store_profile(3U);
    (void)app_init();
    host_set_nv_read_status(ERR_TIMEOUT);
    set_selector_and_pot(3U, TEMPO_RAW_150_BPM);

    // Each of the eight looked at, and then the recall
    run_ticks(20U);
    TEST_ASSERT_EQUAL(9, count_events(HOST_EVENT_ERROR, LOG_ERROR_PROFILE_READ, ERR_TIMEOUT));
    TEST_ASSERT_EQUAL(11, host_nv_read_count());

    // None is looked at again, and the failed recall says nothing
    host_set_nv_read_status(STATUS_OK);
    run_ticks(RUN_ON_TICKS);
    TEST_ASSERT_EQUAL(11, host_nv_read_count());
    TEST_ASSERT_EQUAL(false, host_status_lit());
    TEST_ASSERT_EQUAL(0, host_status_change_count());
}



static void test_the_look_at_the_profiles_waits_while_the_storage_is_busy(void)
{
    // Busy from the start, as it is at power-on while a crash log record
    // is being written
    host_reset();
    store_profile(3U);
    (void)app_init();
    host_set_nv_read_status(ERR_BUSY);
    set_selector_and_pot(3U, TEMPO_RAW_150_BPM);
    run_ticks(30U);
    TEST_ASSERT_EQUAL(false, host_status_lit());
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_ERROR));

    host_set_nv_read_status(STATUS_OK);
    run_ticks(12U);
    TEST_ASSERT_EQUAL(true, host_status_lit());

    // And busy from part of the way through the eight
    host_reset();
    store_profile(3U);
    (void)app_init();
    set_selector_and_pot(3U, TEMPO_RAW_150_BPM);
    run_ticks(3U);
    host_set_nv_read_status(ERR_BUSY);
    run_ticks(30U);
    TEST_ASSERT_EQUAL(false, host_status_lit());
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_ERROR));

    host_set_nv_read_status(STATUS_OK);
    run_ticks(12U);
    TEST_ASSERT_EQUAL(true, host_status_lit());
}



static void test_the_light_stays_dark_in_calibration_mode(void)
{
    host_reset();
    for (uint8_t profile = 0U; profile < 8U; profile++)
    {
        store_profile(profile);
    }
    (void)app_init();
    set_calibration_panel(0U, true); // The entry pattern; the selector at 0
    host_set_tempo(CAL_TRIM_CENTRE, STATUS_OK);

    run_ticks(100U);
    TEST_ASSERT_EQUAL(false, host_status_lit());
    TEST_ASSERT_EQUAL(0, host_status_change_count());
    TEST_ASSERT_EQUAL(100, host_status_write_count());
    TEST_ASSERT_EQUAL(2, host_nv_read_count()); // No profile is looked at
}



static void test_init_puts_the_light_out(void)
{
    start_with(0x08U, 3U);
    hold_button(3U, CLICK_TICKS);
    TEST_ASSERT_EQUAL(true, host_status_lit());

    // The next tick writes what init left, and the button, still down, is
    // held from then
    (void)app_init();
    run_ticks(1U);
    TEST_ASSERT_EQUAL(false, host_status_lit());
    run_ticks(20U);
    TEST_ASSERT_EQUAL(true, host_status_lit());
}



int main(void)
{
    RUN_TEST(test_the_light_is_dark_with_nothing_stored_and_written_on_every_tick);
    RUN_TEST(test_the_light_is_lit_while_the_selected_profile_holds_something);
    RUN_TEST(test_the_power_on_recall_says_nothing);
    RUN_TEST(test_a_recall_flickers_once);
    RUN_TEST(test_a_recall_that_finds_nothing_is_one_long_blink);
    RUN_TEST(test_a_recall_that_cannot_be_read_is_one_long_blink);
    RUN_TEST(test_a_save_winks_at_a_second_and_flickers_twice);
    RUN_TEST(test_a_save_that_cannot_be_started_is_one_long_blink);
    RUN_TEST(test_a_clear_winks_twice_and_flickers_three_times);
    RUN_TEST(test_a_clear_of_all_winks_three_times_and_flickers_four);
    RUN_TEST(test_a_profile_that_cannot_be_read_at_power_on_is_logged_and_taken_as_empty);
    RUN_TEST(test_the_look_at_the_profiles_waits_while_the_storage_is_busy);
    RUN_TEST(test_the_light_stays_dark_in_calibration_mode);
    RUN_TEST(test_init_puts_the_light_out);
    return TEST_RESULT();
}
