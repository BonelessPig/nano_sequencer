/**
 * @file   test_app.c
 * @brief  Host tests for the application loop (app/app.c) running against the
 *         fake ports in adapters/host. These check the wiring: which ports
 *         are called, with what, and in what order.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include "app.h"
#include "host_ports.h"
#include "seq.h"
#include "test_harness.h"

/**
 * @brief Checks that the recorded event at index is the expected output-port call.
 */
static void expect_event(uint16_t index, host_event_kind_t kind, uint16_t a, uint16_t b)
{
    const host_event_t *p_event = host_event_get(index);

    TEST_ASSERT(NULL != p_event);
    if (NULL != p_event)
    {
        TEST_ASSERT_EQUAL(kind, p_event->kind);
        TEST_ASSERT_EQUAL(a, p_event->a);
        TEST_ASSERT_EQUAL(b, p_event->b);
    }
}



/**
 * @brief Fresh fakes and a freshly initialized app, with the init not counted
 *        in the event record.
 */
static void start_app(void)
{
    host_reset();
    (void)app_init();
}



static void test_init_success_is_silent(void)
{
    host_reset();
    TEST_ASSERT_EQUAL(STATUS_OK, app_init());
    TEST_ASSERT_EQUAL(1, host_platform_init_count());
    TEST_ASSERT_EQUAL(0, host_event_count());
}



static void test_init_failure_is_logged_and_returned(void)
{
    host_reset();
    host_set_platform_status(ERR_TIMEOUT);

    TEST_ASSERT_EQUAL(ERR_TIMEOUT, app_init());
    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_INIT, (uint16_t)ERR_TIMEOUT);
}



// Step n holds note 15 - n, so the step and the note are never the same number
static const uint8_t g_descending_notes[HOST_RAW_STEP_BYTES] =
{
    0xFEU, 0xDCU, 0xBAU, 0x98U, 0x76U, 0x54U, 0x32U, 0x10U
};

#define EVENTS_PER_TICK (2U) // A good tick records one step note and one delay



static void test_tick_logs_the_step_played_then_delays(void)
{
    start_app();
    host_set_steps(g_descending_notes, STATUS_OK);
    host_set_tempo(400U, STATUS_OK);
    app_run_once();

    // The first step and its note, then exactly one delay, and nothing else
    TEST_ASSERT_EQUAL(EVENTS_PER_TICK, host_event_count());
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 15U);
    expect_event(1U, HOST_EVENT_DELAY, 100U, 0U);
}



static void test_successive_ticks_log_successive_steps(void)
{
    const uint16_t tick_count = (uint16_t)(SEQ_STEP_COUNT + 2U); // Once round and two more

    start_app();
    host_set_steps(g_descending_notes, STATUS_OK);
    host_set_tempo(40U, STATUS_OK);
    for (uint16_t tick = 0U; tick < tick_count; tick++)
    {
        app_run_once();
    }

    TEST_ASSERT_EQUAL(tick_count * EVENTS_PER_TICK, host_event_count());
    for (uint16_t tick = 0U; tick < tick_count; tick++)
    {
        const uint16_t step = (uint16_t)(tick % SEQ_STEP_COUNT);

        expect_event((uint16_t)(tick * EVENTS_PER_TICK), HOST_EVENT_STEP_NOTE, step, (uint16_t)(15U - step));
        expect_event((uint16_t)((tick * EVENTS_PER_TICK) + 1U), HOST_EVENT_DELAY, 10U, 0U);
    }
}



static void test_tick_reads_the_whole_chain(void)
{
    start_app();
    app_run_once();
    TEST_ASSERT_EQUAL(SEQ_RAW_BYTE_COUNT, host_last_step_byte_count());
}



static void test_step_read_failure_logs_error_instead_of_the_note(void)
{
    start_app();
    host_set_steps(NULL, ERR_INVALID_PARAM);
    host_set_tempo(200U, STATUS_OK);
    app_run_once();

    // One error in place of the step note; the delay still happens
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(0U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_STEP_READ, (uint16_t)ERR_INVALID_PARAM);
    expect_event(1U, HOST_EVENT_DELAY, 50U, 0U);
}



static void test_tempo_read_failure_logs_error_and_keeps_last_delay(void)
{
    start_app();
    host_set_tempo(400U, STATUS_OK);
    app_run_once();

    host_reset(); // Clears the record and the script, but not the app's state
    host_set_tempo(0U, ERR_TIMEOUT);
    app_run_once();

    // The step note first (the second step: this is the app's second tick),
    // then the error, then a delay from the last good reading
    TEST_ASSERT_EQUAL(3, host_event_count());
    expect_event(0U, HOST_EVENT_STEP_NOTE, 1U, 0U);
    expect_event(1U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_TEMPO_READ, (uint16_t)ERR_TIMEOUT);
    expect_event(2U, HOST_EVENT_DELAY, 100U, 0U);
}



static void test_both_reads_failing_logs_both_errors_in_order(void)
{
    start_app();
    host_set_steps(NULL, ERR_GENERAL);
    host_set_tempo(0U, ERR_TIMEOUT);
    app_run_once();

    TEST_ASSERT_EQUAL(3, host_event_count());
    expect_event(0U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_STEP_READ, (uint16_t)ERR_GENERAL);
    expect_event(1U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_TEMPO_READ, (uint16_t)ERR_TIMEOUT);
    expect_event(2U, HOST_EVENT_DELAY, 0U, 0U);
}



static void test_init_resets_the_remembered_tempo_and_step(void)
{
    start_app();
    host_set_tempo(1023U, STATUS_OK);
    app_run_once();

    start_app(); // Re-initialize: the earlier reading and position must be forgotten
    host_set_tempo(0U, ERR_TIMEOUT);
    app_run_once();

    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 0U);
    expect_event(2U, HOST_EVENT_DELAY, 0U, 0U);
}



static void test_maximum_tempo_gives_255_ms(void)
{
    start_app();
    host_set_tempo(1023U, STATUS_OK);
    app_run_once();

    expect_event(1U, HOST_EVENT_DELAY, 255U, 0U);
}



int main(void)
{
    RUN_TEST(test_init_success_is_silent);
    RUN_TEST(test_init_failure_is_logged_and_returned);
    RUN_TEST(test_tick_logs_the_step_played_then_delays);
    RUN_TEST(test_successive_ticks_log_successive_steps);
    RUN_TEST(test_tick_reads_the_whole_chain);
    RUN_TEST(test_step_read_failure_logs_error_instead_of_the_note);
    RUN_TEST(test_tempo_read_failure_logs_error_and_keeps_last_delay);
    RUN_TEST(test_both_reads_failing_logs_both_errors_in_order);
    RUN_TEST(test_init_resets_the_remembered_tempo_and_step);
    RUN_TEST(test_maximum_tempo_gives_255_ms);
    return TEST_RESULT();
}
