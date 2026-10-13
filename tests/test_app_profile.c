/**
 * @file   test_app_profile.c
 * @brief  Host tests for the application loop (app/app.c): the profiles of
 *         the amounts, their button and what is stored. The real app and
 *         core run against the fake ports in adapters/host/.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "app.h"
#include "host_ports.h"
#include "log_port.h"
#include "profile_record.h"
#include "test_harness.h"
#include "app_fixture.h"
#include "profile_fixture.h"



static void test_a_hold_of_a_second_saves_the_amounts_to_the_selected_profile(void)
{
    start_stopped();
    set_amount(2U, 600U);
    set_amount(5U, 300U);
    TEST_ASSERT_EQUAL(0, host_nv_write_count());

    press_button(5U, 300U, SAVE_TICKS);
    TEST_ASSERT_EQUAL(1, host_nv_write_count());
    TEST_ASSERT_EQUAL(1, count_events(HOST_EVENT_PROFILE, 5U, ACTION_SAVE));
    TEST_ASSERT_EQUAL(1, count_kind(HOST_EVENT_PROFILE));

    // The record of profile 5: the version, then positions 1 to 7, low byte
    // first. The tempo, position 0, is not in it
    TEST_ASSERT_EQUAL(STORED_VERSION, host_nv_byte(address_of(5U)));
    TEST_ASSERT_EQUAL(0x00, host_nv_byte((uint16_t)(address_of(5U) + 1U)));
    TEST_ASSERT_EQUAL(0x58, host_nv_byte((uint16_t)(address_of(5U) + 3U))); // 600
    TEST_ASSERT_EQUAL(0x02, host_nv_byte((uint16_t)(address_of(5U) + 4U)));
    TEST_ASSERT_EQUAL(0x2C, host_nv_byte((uint16_t)(address_of(5U) + 9U))); // 300
    TEST_ASSERT_EQUAL(0x01, host_nv_byte((uint16_t)(address_of(5U) + 10U)));

    // No other profile was written, nor anything below them
    TEST_ASSERT_EQUAL(ERASED, host_nv_byte(address_of(4U)));
    TEST_ASSERT_EQUAL(ERASED, host_nv_byte(address_of(6U)));
    TEST_ASSERT_EQUAL(ERASED, host_nv_byte((uint16_t)(PROFILES_ADDRESS - 1U)));
}



static void test_a_click_recalls_the_selected_profile_and_detaches_the_pot(void)
{
    start_stopped();
    store_profile(5U);
    set_amount(5U, 900U);
    TEST_ASSERT_EQUAL(225, host_level());

    // Position 5 holds the fifth of the stored amounts
    press_button(5U, 900U, CLICK_TICKS);
    TEST_ASSERT_EQUAL(1, count_events(HOST_EVENT_PROFILE, 5U, ACTION_RECALL));
    TEST_ASSERT_EQUAL(125, host_level());
    TEST_ASSERT_EQUAL(0, host_nv_write_count());

    // The pot is where it was, and sets nothing until it meets the amount
    set_selector_and_pot(5U, 880U);
    run_ticks(2U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(125, host_level());
    set_selector_and_pot(5U, 500U);
    run_ticks(2U * SCAN_PERIOD_TICKS);
    set_selector_and_pot(5U, 520U);
    run_ticks(2U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(130, host_level());

    // Every position's amount came with it
    set_selector_and_pot(1U, 520U);
    run_ticks(2U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(25, host_level());
    set_selector_and_pot(7U, 520U);
    run_ticks(2U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(175, host_level());
}



static void test_a_recall_leaves_the_tempo_where_the_pot_put_it(void)
{
    // Profile 0 is the one chosen with the selector on the tempo. It is
    // stored once the power-on recall has been and gone, on the tenth tick
    start_app();
    run_ticks(12U);
    store_profile(0U);
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 14U);

    press_button(0U, TEMPO_RAW_150_BPM, CLICK_TICKS);
    TEST_ASSERT_EQUAL(1, count_events(HOST_EVENT_PROFILE, 0U, ACTION_RECALL));
    TEST_ASSERT_EQUAL(TEMPO_RAW_150_BPM / 4U, host_level());

    // Still a step every 100 ticks, the fourth logged on tick 302, and
    // the pot still has the tempo
    run_ticks((3U * TICKS_PER_STEP) -
              (10U + CLICK_TICKS + SETTLE_TICKS + (2U * SCAN_PERIOD_TICKS)));
    TEST_ASSERT_EQUAL(4, count_kind(HOST_EVENT_STEP_NOTE));
    set_selector_and_pot(0U, 800U);
    run_ticks(2U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(200, host_level());
}



static void test_a_click_on_a_profile_that_holds_nothing_changes_nothing(void)
{
    start_stopped();
    set_amount(3U, 400U);

    press_button(3U, 400U, CLICK_TICKS);
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_PROFILE));
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_ERROR));
    TEST_ASSERT_EQUAL(100, host_level());

    // The pot still has it
    set_selector_and_pot(3U, 440U);
    run_ticks(2U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(110, host_level());
}



static void test_the_selected_profile_is_loaded_at_power_on(void)
{
    // The selector at 3 and the pot at the tempo before the power comes on
    host_reset();
    store_profile(3U);
    set_selector_and_pot(3U, TEMPO_RAW_150_BPM);
    TEST_ASSERT_EQUAL(STATUS_OK, app_init());
    TEST_ASSERT_EQUAL(2, host_nv_read_count()); // The calibration, the crash log
    TEST_ASSERT_EQUAL(0, host_event_count());

    // The first tick reads the panel. The eight ticks after it each read
    // a byte of one profile, for the status light, the first of them the
    // tick that logs the first step; the profile the selector is at is
    // read on the tick after those, the tenth. The amounts are in use
    // from then; the pot gave the tempo and has not met position 3's amount
    run_ticks(1U);
    TEST_ASSERT_EQUAL(2, host_nv_read_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(3, host_nv_read_count());
    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 14U);
    run_ticks(7U);
    TEST_ASSERT_EQUAL(10, host_nv_read_count());
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_PROFILE));
    TEST_ASSERT_EQUAL(0, host_level());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(11, host_nv_read_count());
    expect_event(1U, HOST_EVENT_PROFILE, 3U, ACTION_RECALL);
    run_ticks(1U);
    TEST_ASSERT_EQUAL(75, host_level());
    run_ticks(TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(2, count_kind(HOST_EVENT_STEP_NOTE));
    TEST_ASSERT_EQUAL(1, count_kind(HOST_EVENT_PROFILE)); // Loaded the once
    TEST_ASSERT_EQUAL(11, host_nv_read_count());
    TEST_ASSERT_EQUAL(75, host_level());
}



static void test_a_profile_that_holds_nothing_loads_nothing_at_power_on(void)
{
    host_reset();
    store_profile(4U); // Not the one selected
    set_selector_and_pot(3U, TEMPO_RAW_150_BPM);
    (void)app_init();

    run_ticks(20U);
    TEST_ASSERT_EQUAL(11, host_nv_read_count()); // Looked at, and not a profile
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_PROFILE));
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_ERROR));
    TEST_ASSERT_EQUAL(0, host_level());
}



static void test_the_profile_is_loaded_once_the_panel_can_be_read(void)
{
    uint8_t raw[HOST_RAW_STEP_BYTES];

    host_reset();
    store_profile(3U);
    (void)memcpy(raw, g_descending_notes, sizeof(raw));
    raw[HOST_RAW_STEP_BYTES - 1U] = 3U;
    host_set_steps(raw, ERR_TIMEOUT);
    host_set_tempo(TEMPO_RAW_150_BPM, STATUS_OK);
    (void)app_init();

    // With no panel there is no selector to say which
    run_ticks(3U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(2, host_nv_read_count()); // The calibration, the crash log
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_PROFILE));

    host_set_steps(raw, STATUS_OK);
    run_ticks(2U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(11, host_nv_read_count()); // Each of the eight, and then it
    TEST_ASSERT_EQUAL(1, count_events(HOST_EVENT_PROFILE, 3U, ACTION_RECALL));
    run_ticks(10U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(1, count_kind(HOST_EVENT_PROFILE));
}



static void test_a_recall_that_cannot_be_read_is_logged_and_given_up(void)
{
    start_stopped();
    store_profile(5U);
    host_set_nv_read_status(ERR_TIMEOUT);

    press_button(5U, TEMPO_RAW_150_BPM, CLICK_TICKS);
    TEST_ASSERT_EQUAL(1, count_events(HOST_EVENT_ERROR, LOG_ERROR_PROFILE_READ,
                                      ERR_TIMEOUT));
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_PROFILE));
    TEST_ASSERT_EQUAL(0, host_level());

    // It is not tried again when the storage can be read
    host_set_nv_read_status(STATUS_OK);
    run_ticks(2U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_PROFILE));
}



static void test_a_recall_waits_while_the_storage_is_busy(void)
{
    // As it is at power-on while a crash log record is being written
    host_reset();
    store_profile(3U);
    set_selector_and_pot(3U, TEMPO_RAW_150_BPM);
    (void)app_init();
    host_set_nv_read_status(ERR_BUSY);
    run_ticks(20U);
    TEST_ASSERT(host_nv_read_count() > 10U); // Asked on every tick
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_ERROR));
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_PROFILE));
    TEST_ASSERT_EQUAL(0, host_level());

    // Each of the eight is looked at, then the recall, and no more is read
    host_set_nv_read_status(STATUS_OK);
    run_ticks(10U);
    TEST_ASSERT_EQUAL(1, count_events(HOST_EVENT_PROFILE, 3U, ACTION_RECALL));
    TEST_ASSERT_EQUAL(75, host_level());
    {
        const uint16_t reads = host_nv_read_count();

        run_ticks(20U);
        TEST_ASSERT_EQUAL(reads, host_nv_read_count());
    }
}



static void test_a_recall_by_the_button_waits_while_the_storage_is_busy(void)
{
    start_stopped();
    store_profile(5U);
    host_set_nv_read_status(ERR_BUSY);

    press_button(5U, TEMPO_RAW_150_BPM, CLICK_TICKS);
    run_ticks(20U);
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_ERROR));
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_PROFILE));

    host_set_nv_read_status(STATUS_OK);
    run_ticks(2U);
    TEST_ASSERT_EQUAL(1, count_events(HOST_EVENT_PROFILE, 5U, ACTION_RECALL));
}



static void test_a_save_that_cannot_be_started_is_logged(void)
{
    start_stopped();
    host_set_nv_write_status(ERR_BUSY);

    press_button(5U, TEMPO_RAW_150_BPM, SAVE_TICKS);
    TEST_ASSERT_EQUAL(1, count_events(HOST_EVENT_ERROR, LOG_ERROR_PROFILE_WRITE, ERR_BUSY));
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_PROFILE));
    TEST_ASSERT_EQUAL(1, host_nv_write_count()); // Not tried again
    TEST_ASSERT_EQUAL(ERASED, host_nv_byte(address_of(5U)));
}



static void test_a_hold_of_five_seconds_clears_the_selected_profile_only(void)
{
    start_stopped();
    store_profile(4U);
    store_profile(5U);
    set_amount(5U, 900U);

    press_button(5U, 900U, CLEAR_TICKS);
    TEST_ASSERT_EQUAL(1, count_events(HOST_EVENT_PROFILE, 5U, ACTION_CLEAR));
    TEST_ASSERT_EQUAL(1, count_kind(HOST_EVENT_PROFILE));
    TEST_ASSERT_EQUAL(1, host_nv_write_count());

    // One byte makes it no profile; the one beside it is as it was
    TEST_ASSERT_EQUAL(ERASED, host_nv_byte(address_of(5U)));
    TEST_ASSERT_EQUAL(100, host_nv_byte((uint16_t)(address_of(5U) + 1U)));
    TEST_ASSERT_EQUAL(STORED_VERSION, host_nv_byte(address_of(4U)));

    // The amounts in use are not touched, and a click now recalls nothing
    TEST_ASSERT_EQUAL(225, host_level());
    press_button(5U, 900U, CLICK_TICKS);
    TEST_ASSERT_EQUAL(1, count_kind(HOST_EVENT_PROFILE));
    TEST_ASSERT_EQUAL(225, host_level());
}



static void test_a_hold_of_ten_seconds_clears_every_profile_and_nothing_else(void)
{
    static const uint8_t other[4] = { 0x11U, 0x22U, 0x33U, 0x44U };

    start_stopped();
    for (uint8_t profile = 0U; profile < 8U; profile++)
    {
        store_profile(profile);
    }
    host_set_nv_bytes(0U, other, (uint8_t)sizeof(other));  // Where the calibration is
    host_set_nv_bytes(16U, other, (uint8_t)sizeof(other)); // And the crash log
    host_set_nv_bytes((uint16_t)(PROFILES_ADDRESS - 4U), other, (uint8_t)sizeof(other));
    set_amount(2U, 600U);

    press_button(2U, 600U, CLEAR_ALL_TICKS);
    TEST_ASSERT_EQUAL(1, count_events(HOST_EVENT_PROFILE, 2U, ACTION_CLEAR_ALL));
    TEST_ASSERT_EQUAL(1, count_kind(HOST_EVENT_PROFILE));
    TEST_ASSERT_EQUAL(8, host_nv_write_count()); // One byte a profile

    for (uint8_t profile = 0U; profile < 8U; profile++)
    {
        TEST_ASSERT_EQUAL(ERASED, host_nv_byte(address_of(profile)));
    }
    for (uint16_t i = 0U; i < sizeof(other); i++)
    {
        TEST_ASSERT_EQUAL(other[i], host_nv_byte(i));
        TEST_ASSERT_EQUAL(other[i], host_nv_byte((uint16_t)(16U + i)));
        TEST_ASSERT_EQUAL(other[i], host_nv_byte((uint16_t)((PROFILES_ADDRESS - 4U) + i)));
    }
    TEST_ASSERT_EQUAL(150, host_level()); // The amounts in use are not touched
}



static void test_a_clear_waits_while_the_storage_is_busy(void)
{
    start_stopped();
    for (uint8_t profile = 0U; profile < 8U; profile++)
    {
        store_profile(profile);
    }

    // Busy from before the button is let go: the clear is asked for on
    // every tick and none of it is done, and that is no error
    set_selector_and_pot(BUTTON_BIT, TEMPO_RAW_150_BPM);
    run_ticks(CLEAR_ALL_TICKS);
    host_set_nv_write_status(ERR_BUSY);
    set_selector_and_pot(0U, TEMPO_RAW_150_BPM);
    run_ticks(SETTLE_TICKS + (4U * SCAN_PERIOD_TICKS));
    TEST_ASSERT(host_nv_write_count() > 8U);
    TEST_ASSERT_EQUAL(0, count_kind(HOST_EVENT_ERROR));
    TEST_ASSERT_EQUAL(STORED_VERSION, host_nv_byte(address_of(0U)));

    // Free again: one profile a tick, lowest first
    host_set_nv_write_status(STATUS_OK);
    run_ticks(1U);
    TEST_ASSERT_EQUAL(ERASED, host_nv_byte(address_of(0U)));
    TEST_ASSERT_EQUAL(STORED_VERSION, host_nv_byte(address_of(1U)));
    run_ticks(7U);
    TEST_ASSERT_EQUAL(ERASED, host_nv_byte(address_of(7U)));

    // And then no more is written
    {
        const uint16_t writes = host_nv_write_count();

        run_ticks(20U);
        TEST_ASSERT_EQUAL(writes, host_nv_write_count());
    }
}



static void test_a_clear_that_fails_is_logged_and_given_up(void)
{
    start_stopped();
    store_profile(6U);
    host_set_nv_write_status(ERR_INVALID_PARAM);

    press_button(6U, TEMPO_RAW_150_BPM, CLEAR_TICKS);
    TEST_ASSERT_EQUAL(1, count_events(HOST_EVENT_ERROR, LOG_ERROR_PROFILE_WRITE,
                                      ERR_INVALID_PARAM));
    TEST_ASSERT_EQUAL(1, host_nv_write_count());
    TEST_ASSERT_EQUAL(STORED_VERSION, host_nv_byte(address_of(6U)));
}



static void test_the_button_does_nothing_in_calibration_mode(void)
{
    uint8_t raw[HOST_RAW_STEP_BYTES] =
    {
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xA5U, 0xA5U, 0x00U, BUTTON_BIT
    };

    // The entry pattern, its store switch left as it is, and the button held
    host_reset();
    (void)app_init();
    host_set_steps(raw, STATUS_OK);
    host_set_tempo(CAL_TRIM_CENTRE, STATUS_OK);
    run_ticks(SAVE_TICKS);
    raw[HOST_RAW_STEP_BYTES - 1U] = 0x00U;
    host_set_steps(raw, STATUS_OK);
    run_ticks(SETTLE_TICKS + (2U * SCAN_PERIOD_TICKS));

    TEST_ASSERT_EQUAL(0, host_nv_write_count());
    TEST_ASSERT_EQUAL(0, host_event_count());
}



int main(void)
{
    RUN_TEST(test_a_hold_of_a_second_saves_the_amounts_to_the_selected_profile);
    RUN_TEST(test_a_click_recalls_the_selected_profile_and_detaches_the_pot);
    RUN_TEST(test_a_recall_leaves_the_tempo_where_the_pot_put_it);
    RUN_TEST(test_a_click_on_a_profile_that_holds_nothing_changes_nothing);
    RUN_TEST(test_the_selected_profile_is_loaded_at_power_on);
    RUN_TEST(test_a_profile_that_holds_nothing_loads_nothing_at_power_on);
    RUN_TEST(test_the_profile_is_loaded_once_the_panel_can_be_read);
    RUN_TEST(test_a_recall_that_cannot_be_read_is_logged_and_given_up);
    RUN_TEST(test_a_recall_waits_while_the_storage_is_busy);
    RUN_TEST(test_a_recall_by_the_button_waits_while_the_storage_is_busy);
    RUN_TEST(test_a_save_that_cannot_be_started_is_logged);
    RUN_TEST(test_a_hold_of_five_seconds_clears_the_selected_profile_only);
    RUN_TEST(test_a_hold_of_ten_seconds_clears_every_profile_and_nothing_else);
    RUN_TEST(test_a_clear_waits_while_the_storage_is_busy);
    RUN_TEST(test_a_clear_that_fails_is_logged_and_given_up);
    RUN_TEST(test_the_button_does_nothing_in_calibration_mode);
    return TEST_RESULT();
}
