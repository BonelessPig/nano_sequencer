/**
 * @file   test_app_cal.c
 * @brief  Host tests for the application loop (app/app.c) against the fake
 *         ports in adapters/host: the stored calibration and
 *         calibration mode.
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

static void test_init_reports_the_reset_cause_and_no_calibration(void)
{
    host_reset();
    host_set_reset_cause(0x02U);
    TEST_ASSERT_EQUAL(STATUS_OK, app_init());

    TEST_ASSERT_EQUAL(1, host_startup_count());
    TEST_ASSERT_EQUAL(0x02, host_startup_reset_cause());
    TEST_ASSERT(!host_startup_calibrated()); // The storage is erased
    TEST_ASSERT_EQUAL(2, host_nv_read_count()); // The calibration, the crash log
    TEST_ASSERT_EQUAL(0, host_nv_write_count());
    TEST_ASSERT_EQUAL(0, host_event_count());

    // The cause was taken, so the next start sees only what happens after
    (void)app_init();
    TEST_ASSERT_EQUAL(2, host_startup_count());
    TEST_ASSERT_EQUAL(0, host_startup_reset_cause());
}



static void test_init_failure_reads_no_calibration_and_reports_no_start(void)
{
    host_reset();
    host_set_platform_status(ERR_TIMEOUT);
    host_set_reset_cause(0x01U);
    (void)app_init();

    TEST_ASSERT_EQUAL(0, host_startup_count());
    TEST_ASSERT_EQUAL(0, host_nv_read_count());
    TEST_ASSERT_EQUAL(0, host_crash_count());
    TEST_ASSERT_EQUAL(0, host_breadcrumb_write_count());
}



static void test_a_stored_calibration_is_loaded_and_played(void)
{
    // Every octave a little out, each by a different amount
    static const pitch_cal_config_t stored = { { 10U, 1100U, 2000U, 3050U, 4000U } };
    uint8_t                         record[CAL_RECORD_SIZE];

    cal_record_encode(&stored, record);
    host_reset();
    host_set_nv_bytes(0U, record, (uint8_t)sizeof(record));
    (void)app_init();
    host_set_steps(g_descending_notes, STATUS_OK);
    host_set_tempo(TEMPO_RAW_150_BPM, STATUS_OK);

    TEST_ASSERT(host_startup_calibrated());

    // Step 0 plays pitch 14, two semitones into the second octave:
    // 1100 + (2000 - 1100) * 2 / 12; step 1 plays pitch 13
    run_ticks(2U);
    TEST_ASSERT_EQUAL(1250, host_cv_millivolts());
    run_ticks(TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(1175, host_cv_millivolts());
    TEST_ASSERT_EQUAL(0, host_nv_write_count()); // Playing never writes
}



static void test_a_record_that_is_not_valid_leaves_the_ideal_table(void)
{
    static const pitch_cal_config_t stored = { { 10U, 1100U, 2000U, 3050U, 4000U } };
    uint8_t                         record[CAL_RECORD_SIZE];

    cal_record_encode(&stored, record);
    record[3] = (uint8_t)(record[3] ^ 0x01U); // One bit of one entry
    host_reset();
    host_set_nv_bytes(0U, record, (uint8_t)sizeof(record));
    start_app_keeping_storage();

    TEST_ASSERT(!host_startup_calibrated());
    TEST_ASSERT_EQUAL(0, host_event_count()); // Not an error: nothing is logged
    run_ticks(2U);
    TEST_ASSERT_EQUAL(1167, host_cv_millivolts());
}



static void test_a_failed_calibration_read_is_logged_and_leaves_the_ideal_table(void)
{
    static const pitch_cal_config_t stored = { { 10U, 1100U, 2000U, 3050U, 4000U } };
    uint8_t                         record[CAL_RECORD_SIZE];

    cal_record_encode(&stored, record);
    host_reset();
    host_set_nv_bytes(0U, record, (uint8_t)sizeof(record));
    host_set_nv_read_status(ERR_BUSY);
    start_app_keeping_storage();

    // The storage refuses every read, so the crash log's fails as well
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(0U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_NV_READ, (uint16_t)ERR_BUSY);
    expect_event(1U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_CRASH_READ, (uint16_t)ERR_BUSY);
    TEST_ASSERT_EQUAL(1, host_startup_count());
    TEST_ASSERT(!host_startup_calibrated());
    run_ticks(2U);
    TEST_ASSERT_EQUAL(1167, host_cv_millivolts());
}



static void test_the_entry_pattern_at_start_up_gives_calibration_mode(void)
{
    start_calibration(2U, CAL_TRIM_CENTRE);

    // The first tick reads the panel and chooses the mode; the second
    // applies it: the point's nominal voltage, the gate high, no clock
    run_ticks(1U);
    TEST_ASSERT(!host_gate_level());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(2000, host_cv_millivolts());
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT(!host_clock_output_level());

    // Nothing is sequenced: no step is logged and the clock stays low
    run_ticks(3U * TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(0, host_event_count());
    TEST_ASSERT_EQUAL(0, host_clock_output_rise_count());
    TEST_ASSERT_EQUAL(1, host_gate_rise_count());
    TEST_ASSERT_EQUAL(2000, host_cv_millivolts());
    TEST_ASSERT_EQUAL(0, host_nv_write_count());

    // The outputs are still written on every tick
    TEST_ASSERT_EQUAL((3U * TICKS_PER_STEP) + 2U, host_cv_write_count());
    TEST_ASSERT_EQUAL((3U * TICKS_PER_STEP) + 2U, host_gate_write_count());
}



static void test_calibration_follows_the_point_switches_and_the_tempo_control(void)
{
    start_calibration(2U, CAL_TRIM_CENTRE);
    run_ticks(2U);

    // Both are read at the next scan and applied the tick after
    set_calibration_panel(3U, true);
    host_set_tempo(616U, STATUS_OK); // 13 values above the middle
    run_ticks(SCAN_PERIOD_TICKS - 2U);
    TEST_ASSERT_EQUAL(2000, host_cv_millivolts());
    run_ticks(2U);
    TEST_ASSERT_EQUAL(3013, host_cv_millivolts());

    // With the control unreadable the trim holds, and the point still moves
    set_calibration_panel(1U, true);
    host_set_tempo(0U, ERR_TIMEOUT);
    run_ticks(SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(1013, host_cv_millivolts());
    TEST_ASSERT_EQUAL(0, host_event_count()); // Nothing is logged without a step
}



static void test_storing_a_point_saves_the_record_and_logs_it(void)
{
    static const pitch_cal_config_t expected = { { 0U, 1000U, 2013U, 3000U, 4000U } };
    uint8_t                         record[CAL_RECORD_SIZE];

    start_calibration(2U, 616U);
    run_ticks(2U);

    set_calibration_panel(2U, false);
    run_ticks(SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(0, host_nv_write_count());

    // Switched on: seen at the next scan, on tick 17, and saved on tick 18
    set_calibration_panel(2U, true);
    run_ticks(SCAN_PERIOD_TICKS - 1U);
    TEST_ASSERT_EQUAL(0, host_nv_write_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(1, host_nv_write_count());
    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_CAL_POINT, 2U, 2013U);

    // The whole table as a record: the stored point, the rest still ideal
    cal_record_encode(&expected, record);
    for (uint16_t i = 0U; i < CAL_RECORD_SIZE; i++)
    {
        TEST_ASSERT_EQUAL(record[i], host_nv_byte(i));
    }
    TEST_ASSERT_EQUAL(HOST_NV_ERASED, host_nv_byte((uint16_t)CAL_RECORD_SIZE));

    // Once: the switch staying on saves nothing more
    run_ticks(10U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(1, host_nv_write_count());
    TEST_ASSERT_EQUAL(1, host_event_count());
}



static void test_the_gate_drops_for_a_quarter_second_after_a_store(void)
{
    start_calibration(1U, CAL_TRIM_CENTRE);
    run_ticks(2U);
    set_calibration_panel(1U, false);
    run_ticks(SCAN_PERIOD_TICKS);
    set_calibration_panel(1U, true);
    run_ticks(SCAN_PERIOD_TICKS - 1U);
    TEST_ASSERT(host_gate_level());

    // Low from the tick the save is made, for 250 ticks
    run_ticks(1U);
    TEST_ASSERT(!host_gate_level());
    run_ticks(CAL_CONFIRM_TICKS - 1U);
    TEST_ASSERT(!host_gate_level());
    run_ticks(1U);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT_EQUAL(2, host_gate_rise_count());
    TEST_ASSERT_EQUAL(1000, host_cv_millivolts()); // The pitch never moved
}



static void test_a_stored_point_is_played_after_the_next_start(void)
{
    start_calibration(1U, 672U); // Point 1, 20 above nominal
    run_ticks(2U);
    set_calibration_panel(1U, false);
    run_ticks(SCAN_PERIOD_TICKS);
    set_calibration_panel(1U, true);
    run_ticks(SCAN_PERIOD_TICKS + 1U);
    TEST_ASSERT_EQUAL(1, host_nv_write_count());

    // Power off and on with an ordinary panel: step 0 plays pitch 14, which
    // is 1020 + (2000 - 1020) * 2 / 12
    start_app_keeping_storage();
    TEST_ASSERT(host_startup_calibrated());
    run_ticks(2U);
    TEST_ASSERT_EQUAL(1, host_event_count() - 1U); // The store's line, then step 0
    expect_event(1U, HOST_EVENT_STEP_NOTE, 0U, 14U);
    TEST_ASSERT_EQUAL(1183, host_cv_millivolts());
}



static void test_storing_keeps_the_other_points_of_a_loaded_table(void)
{
    static const pitch_cal_config_t stored   = { { 10U, 1100U, 2000U, 3050U, 4000U } };
    static const pitch_cal_config_t expected = { { 10U, 1100U, 2000U, 3050U, 4063U } };
    uint8_t                         record[CAL_RECORD_SIZE];

    cal_record_encode(&stored, record);
    host_reset();
    host_set_nv_bytes(0U, record, (uint8_t)sizeof(record));
    (void)app_init();
    set_calibration_panel(4U, true);
    host_set_tempo(TEMPO_RAW_MAX, STATUS_OK);
    run_ticks(2U);
    TEST_ASSERT_EQUAL(4063, host_cv_millivolts());

    set_calibration_panel(4U, false);
    run_ticks(SCAN_PERIOD_TICKS);
    set_calibration_panel(4U, true);
    run_ticks(SCAN_PERIOD_TICKS + 1U);

    cal_record_encode(&expected, record);
    for (uint16_t i = 0U; i < CAL_RECORD_SIZE; i++)
    {
        TEST_ASSERT_EQUAL(record[i], host_nv_byte(i));
    }
}



static void test_a_save_that_cannot_start_is_logged_as_an_error(void)
{
    start_calibration(2U, CAL_TRIM_CENTRE);
    host_set_nv_write_status(ERR_BUSY);
    run_ticks(2U);
    set_calibration_panel(2U, false);
    run_ticks(SCAN_PERIOD_TICKS);
    set_calibration_panel(2U, true);
    run_ticks(SCAN_PERIOD_TICKS + 1U);

    TEST_ASSERT_EQUAL(1, host_nv_write_count());
    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_NV_WRITE, (uint16_t)ERR_BUSY);
    TEST_ASSERT_EQUAL(HOST_NV_ERASED, host_nv_byte(0U));

    // Storing again once the storage is free saves the point after all
    host_set_nv_write_status(STATUS_OK);
    set_calibration_panel(2U, false);
    run_ticks(SCAN_PERIOD_TICKS);
    set_calibration_panel(2U, true);
    run_ticks(SCAN_PERIOD_TICKS + 1U);
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(1U, HOST_EVENT_CAL_POINT, 2U, 2000U);
    TEST_ASSERT_EQUAL(0x01, host_nv_byte(0U)); // A record, by its version
}



static void test_the_entry_pattern_after_start_up_is_only_a_pattern(void)
{
    start_app();
    run_ticks(2U);
    TEST_ASSERT_EQUAL(1, host_event_count());

    // Set mid-run, with the store switch flipped as well: the sequencer
    // carries on and nothing is saved
    set_calibration_panel(2U, false);
    run_ticks(SCAN_PERIOD_TICKS);
    set_calibration_panel(2U, true);
    run_ticks(TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(1U, HOST_EVENT_STEP_REST, 1U, 0U);
    TEST_ASSERT_EQUAL(0, host_nv_write_count());
    TEST_ASSERT_EQUAL(2, host_clock_output_rise_count());
}



static void test_a_panel_that_cannot_be_read_at_start_up_plays_as_normal(void)
{
    start_calibration(2U, CAL_TRIM_CENTRE);
    host_set_steps(NULL, ERR_TIMEOUT);
    run_ticks(2U);
    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_STEP_READ, (uint16_t)ERR_TIMEOUT);

    // Readable again, with the pattern still set: it is played, not obeyed
    host_set_steps(NULL, STATUS_OK);
    run_ticks(TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(1U, HOST_EVENT_STEP_REST, 1U, 0U);
    TEST_ASSERT_EQUAL(2, host_clock_output_rise_count());
}



static void test_init_leaves_calibration_mode(void)
{
    start_calibration(3U, CAL_TRIM_CENTRE);
    run_ticks(10U);
    TEST_ASSERT_EQUAL(3000, host_cv_millivolts());
    TEST_ASSERT(host_gate_level());

    start_app(); // An ordinary panel this time
    run_ticks(1U);
    TEST_ASSERT_EQUAL(0, host_cv_millivolts());
    TEST_ASSERT(!host_gate_level());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 14U);
}



int main(void)
{
    RUN_TEST(test_init_reports_the_reset_cause_and_no_calibration);
    RUN_TEST(test_init_failure_reads_no_calibration_and_reports_no_start);
    RUN_TEST(test_a_stored_calibration_is_loaded_and_played);
    RUN_TEST(test_a_record_that_is_not_valid_leaves_the_ideal_table);
    RUN_TEST(test_a_failed_calibration_read_is_logged_and_leaves_the_ideal_table);
    RUN_TEST(test_the_entry_pattern_at_start_up_gives_calibration_mode);
    RUN_TEST(test_calibration_follows_the_point_switches_and_the_tempo_control);
    RUN_TEST(test_storing_a_point_saves_the_record_and_logs_it);
    RUN_TEST(test_the_gate_drops_for_a_quarter_second_after_a_store);
    RUN_TEST(test_a_stored_point_is_played_after_the_next_start);
    RUN_TEST(test_storing_keeps_the_other_points_of_a_loaded_table);
    RUN_TEST(test_a_save_that_cannot_start_is_logged_as_an_error);
    RUN_TEST(test_the_entry_pattern_after_start_up_is_only_a_pattern);
    RUN_TEST(test_a_panel_that_cannot_be_read_at_start_up_plays_as_normal);
    RUN_TEST(test_init_leaves_calibration_mode);
    return TEST_RESULT();
}
