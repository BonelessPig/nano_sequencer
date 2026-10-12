/**
 * @file   test_app_crash.c
 * @brief  Host tests for the application loop (app/app.c) against the fake
 *         ports in adapters/host: the crash log and the
 *         breadcrumb.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "app.h"
#include "cal_record.h"
#include "host_ports.h"
#include "midi_output_port.h"
#include "seq.h"
#include "test_harness.h"
#include "app_fixture.h"

/**
 * @brief Checks that the crash log's first record holds these five bytes.
 */
static void expect_first_crash_record(const uint8_t *p_expected)
{
    for (uint16_t i = 0U; i < CRASH_RECORD_BYTES; i++)
    {
        TEST_ASSERT_EQUAL(p_expected[i], host_nv_byte((uint16_t)(CRASH_LOG_ADDRESS + i)));
    }
}



static void test_a_watchdog_reset_is_recorded_with_where_the_loop_had_got_to(void)
{
    // Record 1, the watchdog, place 5 (between ticks), once; the checksum
    // was worked out separately (CRC-8, polynomial 0x07, starting from 1)
    static const uint8_t record[CRASH_RECORD_BYTES] = { 0x01U, 0x08U, 0x05U, 0x01U, 0x17U };

    start_app();
    run_ticks(3U);
    TEST_ASSERT_EQUAL(0, host_crash_count()); // Nothing in the log so far
    const uint16_t events_before = host_event_count();

    // The watchdog restarts the firmware; the breadcrumb is as the loop left it
    host_set_reset_cause(RESET_CAUSE_WATCHDOG);
    start_app_keeping_storage();

    TEST_ASSERT_EQUAL(RESET_CAUSE_WATCHDOG, host_startup_reset_cause());
    TEST_ASSERT_EQUAL(1, host_nv_write_count());
    expect_first_crash_record(record);
    for (uint16_t address = 0U; address < HOST_NV_BYTES; address++)
    {
        if ((address < CRASH_LOG_ADDRESS) ||
            (address >= (CRASH_LOG_ADDRESS + CRASH_RECORD_BYTES)))
        {
            TEST_ASSERT_EQUAL(HOST_NV_ERASED, host_nv_byte(address));
        }
    }
    TEST_ASSERT_EQUAL(1, host_crash_count());
    TEST_ASSERT_EQUAL(1, host_crash_number());
    TEST_ASSERT_EQUAL(PLACE_BETWEEN_TICKS, host_crash_place());
    TEST_ASSERT_EQUAL(1, host_crash_repeats());
    TEST_ASSERT_EQUAL(events_before, host_event_count()); // Not an error
}



static void test_a_watchdog_reset_before_the_first_tick_is_recorded_as_start_up(void)
{
    static const uint8_t record[CRASH_RECORD_BYTES] = { 0x01U, 0x08U, 0x01U, 0x01U, 0x43U };

    start_app();
    host_set_reset_cause(RESET_CAUSE_WATCHDOG);
    start_app_keeping_storage();

    expect_first_crash_record(record);
    TEST_ASSERT_EQUAL(PLACE_START_UP, host_crash_place());
}



static void test_each_part_of_the_loop_a_reset_can_land_in_is_recorded_as_itself(void)
{
    for (uint8_t place = 0U; place <= 6U; place++)
    {
        host_reset();
        host_set_breadcrumb(place);
        host_set_reset_cause(RESET_CAUSE_WATCHDOG);
        (void)app_init();

        TEST_ASSERT_EQUAL(place, host_nv_byte(CRASH_LOG_ADDRESS + 2U));
        TEST_ASSERT_EQUAL(place, host_crash_place());
    }
}



static void test_a_reset_that_was_not_the_watchdog_records_nothing(void)
{
    // Power-on, the reset pin, a brown-out, all three, and none
    static const uint8_t causes[] = { 0x01U, 0x02U, 0x04U, 0x07U, 0x00U };

    for (size_t i = 0U; i < (sizeof(causes) / sizeof(causes[0])); i++)
    {
        host_reset();
        host_set_breadcrumb(PLACE_BETWEEN_TICKS);
        host_set_reset_cause(causes[i]);
        (void)app_init();

        TEST_ASSERT_EQUAL(0, host_nv_write_count());
        TEST_ASSERT_EQUAL(HOST_NV_ERASED, host_nv_byte(CRASH_LOG_ADDRESS));
        TEST_ASSERT_EQUAL(0, host_crash_count());
    }
}



static void test_the_same_crash_again_raises_the_count_of_its_record(void)
{
    static const uint8_t twice[CRASH_RECORD_BYTES] = { 0x01U, 0x08U, 0x05U, 0x02U, 0x1EU };

    start_app();
    for (unsigned int crash = 0U; crash < 2U; crash++)
    {
        run_ticks(3U);
        host_set_reset_cause(RESET_CAUSE_WATCHDOG);
        start_app_keeping_storage();
    }

    TEST_ASSERT_EQUAL(2, host_nv_write_count());
    expect_first_crash_record(twice);
    TEST_ASSERT_EQUAL(HOST_NV_ERASED, host_nv_byte(CRASH_LOG_ADDRESS + CRASH_RECORD_BYTES));
    TEST_ASSERT_EQUAL(1, host_crash_number());
    TEST_ASSERT_EQUAL(2, host_crash_repeats());
}



static void test_a_crash_somewhere_else_is_a_second_record(void)
{
    start_app();
    run_ticks(3U);
    host_set_reset_cause(RESET_CAUSE_WATCHDOG);
    start_app_keeping_storage();

    // And again at once, before a tick has run
    host_set_reset_cause(RESET_CAUSE_WATCHDOG);
    start_app_keeping_storage();

    TEST_ASSERT_EQUAL(1, host_nv_byte(CRASH_LOG_ADDRESS)); // The first is kept
    TEST_ASSERT_EQUAL(2, host_nv_byte(CRASH_LOG_ADDRESS + CRASH_RECORD_BYTES));
    TEST_ASSERT_EQUAL(2, host_crash_number());
    TEST_ASSERT_EQUAL(PLACE_START_UP, host_crash_place());
    TEST_ASSERT_EQUAL(1, host_crash_repeats());
}



static void test_the_newest_crash_is_logged_at_every_start_after_it(void)
{
    start_app();
    run_ticks(3U);
    host_set_reset_cause(RESET_CAUSE_WATCHDOG);
    start_app_keeping_storage();

    // The power is cycled: not a crash, but the log is still reported
    host_set_reset_cause(0x01U);
    start_app_keeping_storage();

    TEST_ASSERT_EQUAL(1, host_nv_write_count()); // Only the crash wrote
    TEST_ASSERT_EQUAL(2, host_crash_count());
    TEST_ASSERT_EQUAL(1, host_crash_number());
    TEST_ASSERT_EQUAL(PLACE_BETWEEN_TICKS, host_crash_place());
    TEST_ASSERT_EQUAL(1, host_crash_repeats());
}



static void test_a_crash_counted_255_times_is_not_written_again(void)
{
    static const uint8_t full[CRASH_RECORD_BYTES] = { 0x01U, 0x08U, 0x05U, 0xFFU, 0xE3U };

    host_reset();
    host_set_nv_bytes(CRASH_LOG_ADDRESS, full, (uint8_t)sizeof(full));
    host_set_breadcrumb(PLACE_BETWEEN_TICKS);
    host_set_reset_cause(RESET_CAUSE_WATCHDOG);
    (void)app_init();

    TEST_ASSERT_EQUAL(0, host_nv_write_count());
    expect_first_crash_record(full);
    TEST_ASSERT_EQUAL(1, host_crash_count());
    TEST_ASSERT_EQUAL(255, host_crash_repeats());
    TEST_ASSERT_EQUAL(0, host_event_count());
}



static void test_a_crash_record_that_cannot_be_written_is_logged_as_an_error(void)
{
    host_reset();
    host_set_breadcrumb(PLACE_BETWEEN_TICKS);
    host_set_reset_cause(RESET_CAUSE_WATCHDOG);
    host_set_nv_write_status(ERR_BUSY);
    (void)app_init();

    TEST_ASSERT_EQUAL(1, host_nv_write_count());
    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_CRASH_WRITE, (uint16_t)ERR_BUSY);
    TEST_ASSERT_EQUAL(HOST_NV_ERASED, host_nv_byte(CRASH_LOG_ADDRESS));
    TEST_ASSERT_EQUAL(0, host_crash_count()); // Nothing is reported as kept
    TEST_ASSERT_EQUAL(1, host_startup_count());
}



static void test_a_crash_log_that_cannot_be_read_is_left_alone(void)
{
    host_reset();
    host_set_breadcrumb(PLACE_BETWEEN_TICKS);
    host_set_reset_cause(RESET_CAUSE_WATCHDOG);
    host_set_nv_read_status(ERR_BUSY);
    (void)app_init();

    TEST_ASSERT_EQUAL(0, host_nv_write_count());
    TEST_ASSERT_EQUAL(0, host_crash_count());
    expect_event(1U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_CRASH_READ, (uint16_t)ERR_BUSY);
}



static void test_a_stored_calibration_and_the_crash_log_keep_apart(void)
{
    static const pitch_cal_config_t stored = { { 10U, 1100U, 2000U, 3050U, 4000U } };
    uint8_t                         record[CAL_RECORD_SIZE];

    cal_record_encode(&stored, record);
    host_reset();
    host_set_nv_bytes(0U, record, (uint8_t)sizeof(record));
    host_set_breadcrumb(PLACE_BETWEEN_TICKS);
    host_set_reset_cause(RESET_CAUSE_WATCHDOG);
    (void)app_init();

    TEST_ASSERT(host_startup_calibrated());
    TEST_ASSERT_EQUAL(1, host_crash_count());
    for (uint16_t i = 0U; i < CAL_RECORD_SIZE; i++)
    {
        TEST_ASSERT_EQUAL(record[i], host_nv_byte(i));
    }
    for (uint16_t address = CAL_RECORD_SIZE; address < CRASH_LOG_ADDRESS; address++)
    {
        TEST_ASSERT_EQUAL(HOST_NV_ERASED, host_nv_byte(address));
    }
}



static void test_the_breadcrumb_marks_each_part_of_a_tick_and_nothing_between(void)
{
    start_app();
    TEST_ASSERT_EQUAL(1, host_breadcrumb_write_count());
    TEST_ASSERT_EQUAL(PLACE_START_UP, host_breadcrumb());

    // A pass with no tick leaves it where it was
    host_set_elapsed_ticks(0U);
    for (unsigned int pass = 0U; pass < 10U; pass++)
    {
        app_run_once();
    }
    TEST_ASSERT_EQUAL(1, host_breadcrumb_write_count());

    for (unsigned int tick = 1U; tick <= 3U; tick++)
    {
        run_ticks(1U);
        TEST_ASSERT_EQUAL(1U + (4U * tick), host_breadcrumb_write_count());
        TEST_ASSERT_EQUAL(2, host_breadcrumb_before(3U)); // Applying the outputs
        TEST_ASSERT_EQUAL(3, host_breadcrumb_before(2U)); // Gathering the inputs
        TEST_ASSERT_EQUAL(4, host_breadcrumb_before(1U)); // The core
        TEST_ASSERT_EQUAL(PLACE_BETWEEN_TICKS, host_breadcrumb_before(0U));
        TEST_ASSERT_EQUAL(2, host_breadcrumb_at_cv_write());
        TEST_ASSERT_EQUAL(3, host_breadcrumb_at_run_read());
        TEST_ASSERT_EQUAL(PLACE_BETWEEN_TICKS, host_breadcrumb());
    }
    TEST_ASSERT_EQUAL(0, host_breadcrumb_before(HOST_BREADCRUMB_HISTORY)); // Out of range
}



static void test_storage_is_polled_on_every_pass(void)
{
    start_app();
    run_ticks(5U);
    host_set_elapsed_ticks(0U);
    app_run_once();
    app_run_once();
    TEST_ASSERT_EQUAL(7, host_nv_poll_count());
}



int main(void)
{
    RUN_TEST(test_a_watchdog_reset_is_recorded_with_where_the_loop_had_got_to);
    RUN_TEST(test_a_watchdog_reset_before_the_first_tick_is_recorded_as_start_up);
    RUN_TEST(test_each_part_of_the_loop_a_reset_can_land_in_is_recorded_as_itself);
    RUN_TEST(test_a_reset_that_was_not_the_watchdog_records_nothing);
    RUN_TEST(test_the_same_crash_again_raises_the_count_of_its_record);
    RUN_TEST(test_a_crash_somewhere_else_is_a_second_record);
    RUN_TEST(test_the_newest_crash_is_logged_at_every_start_after_it);
    RUN_TEST(test_a_crash_counted_255_times_is_not_written_again);
    RUN_TEST(test_a_crash_record_that_cannot_be_written_is_logged_as_an_error);
    RUN_TEST(test_a_crash_log_that_cannot_be_read_is_left_alone);
    RUN_TEST(test_a_stored_calibration_and_the_crash_log_keep_apart);
    RUN_TEST(test_the_breadcrumb_marks_each_part_of_a_tick_and_nothing_between);
    RUN_TEST(test_storage_is_polled_on_every_pass);
    return TEST_RESULT();
}
