/**
 * @file   test_app.c
 * @brief  Host tests for the application loop (app/app.c) running against the
 *         fake ports in adapters/host. These check the wiring: which ports
 *         are called, with what, in what order and on which tick.
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

#define TEMPO_RAW_150_BPM (480U)  // A step every 100 ticks
#define TEMPO_RAW_MAX     (1023U) // 285 BPM: 19 steps every 1000 ticks
#define TICKS_PER_STEP    (100U)  // At TEMPO_RAW_150_BPM
#define SCAN_PERIOD_TICKS (8U)    // How often the app re-reads its inputs

// Step n holds note 15 - n, so the step and the note are never the same number
static const uint8_t g_descending_notes[HOST_RAW_STEP_BYTES] =
{
    0xFEU, 0xDCU, 0xBAU, 0x98U, 0x76U, 0x54U, 0x32U, 0x10U
};

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
 *        in the event record. The panel holds the descending notes and the
 *        tempo control is at 150 BPM.
 */
static void start_app(void)
{
    host_reset();
    (void)app_init();
    host_set_steps(g_descending_notes, STATUS_OK);
    host_set_tempo(TEMPO_RAW_150_BPM, STATUS_OK);
}



/**
 * @brief Runs the main loop for a number of passes, with one tick elapsed
 *        before each.
 */
static void run_ticks(unsigned int count)
{
    host_set_elapsed_ticks(1U);
    for (unsigned int i = 0U; i < count; i++)
    {
        app_run_once();
    }
}



static void test_init_success_is_silent(void)
{
    host_reset();
    TEST_ASSERT_EQUAL(STATUS_OK, app_init());
    TEST_ASSERT_EQUAL(1, host_platform_init_count());
    TEST_ASSERT_EQUAL(0, host_event_count());
}



static void test_init_failure_is_logged_sent_and_returned(void)
{
    host_reset();
    host_set_platform_status(ERR_TIMEOUT);

    // The main loop will not run after this, so the log is flushed here
    TEST_ASSERT_EQUAL(ERR_TIMEOUT, app_init());
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(0U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_INIT, (uint16_t)ERR_TIMEOUT);
    expect_event(1U, HOST_EVENT_FLUSH, 0U, 0U);
}



static void test_pass_with_no_tick_only_polls_the_log(void)
{
    start_app();
    host_set_elapsed_ticks(0U);
    for (unsigned int pass = 0U; pass < 10U; pass++)
    {
        app_run_once();
    }

    TEST_ASSERT_EQUAL(0, host_step_read_count());
    TEST_ASSERT_EQUAL(0, host_tempo_read_count());
    TEST_ASSERT_EQUAL(0, host_event_count());
    TEST_ASSERT_EQUAL(10, host_log_poll_count());
}



static void test_log_is_polled_on_every_pass(void)
{
    start_app();
    run_ticks(5U);
    TEST_ASSERT_EQUAL(5, host_log_poll_count());
}



static void test_step_is_logged_one_tick_after_it_begins(void)
{
    start_app();

    // The first tick works out that step 0 begins; nothing is applied yet
    run_ticks(1U);
    TEST_ASSERT_EQUAL(0, host_event_count());

    // The second tick starts by applying it
    run_ticks(1U);
    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 15U);

    // And it is applied once, not on every tick after
    run_ticks(10U);
    TEST_ASSERT_EQUAL(1, host_event_count());
}



static void test_steps_are_logged_at_the_tempo_and_wrap(void)
{
    const uint16_t step_count = (uint16_t)(SEQ_STEP_COUNT + 2U); // Once round and two more

    start_app();
    run_ticks(2U); // Step 0 begins on tick 1 and is logged on tick 2
    TEST_ASSERT_EQUAL(1, host_event_count());

    // Step n begins on tick 100 * n and is logged on the tick after
    run_ticks(TICKS_PER_STEP - 2U);
    for (uint16_t step = 1U; step < step_count; step++)
    {
        TEST_ASSERT_EQUAL(step, host_event_count()); // Not yet, on the tick it begins
        run_ticks(1U);
        TEST_ASSERT_EQUAL(step + 1U, host_event_count());
        run_ticks(TICKS_PER_STEP - 1U);
    }

    for (uint16_t count = 0U; count < step_count; count++)
    {
        const uint16_t step = (uint16_t)(count % SEQ_STEP_COUNT);

        expect_event(count, HOST_EVENT_STEP_NOTE, step, (uint16_t)(15U - step));
    }
}



static void test_fastest_tempo_logs_19_steps_a_second(void)
{
    start_app();
    host_set_tempo(TEMPO_RAW_MAX, STATUS_OK);

    // Step 0 on tick 1, step 19 on tick 1000, each logged a tick later
    run_ticks(1000U);
    TEST_ASSERT_EQUAL(19, host_event_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(20, host_event_count());
    expect_event(19U, HOST_EVENT_STEP_NOTE, 3U, 12U);
}



static void test_inputs_are_read_every_eighth_tick(void)
{
    start_app();

    run_ticks(1U); // The first tick always reads
    TEST_ASSERT_EQUAL(1, host_step_read_count());
    TEST_ASSERT_EQUAL(1, host_tempo_read_count());

    run_ticks(SCAN_PERIOD_TICKS - 1U);
    TEST_ASSERT_EQUAL(1, host_step_read_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(2, host_step_read_count());
    TEST_ASSERT_EQUAL(2, host_tempo_read_count());

    run_ticks(10U * SCAN_PERIOD_TICKS);
    TEST_ASSERT_EQUAL(12, host_step_read_count());
    TEST_ASSERT_EQUAL(12, host_tempo_read_count());
}



static void test_scan_timing_counts_elapsed_ticks_not_passes(void)
{
    start_app();
    host_set_elapsed_ticks(5U);

    app_run_once(); // Reads: the first tick
    TEST_ASSERT_EQUAL(1, host_step_read_count());
    app_run_once(); // 5 of the 8 ticks gone
    TEST_ASSERT_EQUAL(1, host_step_read_count());
    app_run_once(); // 10 gone: reads
    TEST_ASSERT_EQUAL(2, host_step_read_count());

    // A pass that is very late reads once, not once per period missed
    host_set_elapsed_ticks(200U);
    app_run_once();
    TEST_ASSERT_EQUAL(3, host_step_read_count());
}



static void test_tick_reads_the_whole_chain(void)
{
    start_app();
    run_ticks(1U);
    TEST_ASSERT_EQUAL(SEQ_RAW_BYTE_COUNT, host_last_step_byte_count());
}



static void test_elapsed_ticks_reach_the_core(void)
{
    // Ten ticks at a time, a 100-tick step begins on every tenth pass
    start_app();
    host_set_elapsed_ticks(10U);

    app_run_once(); // Step 0 begins
    app_run_once(); // And is logged
    TEST_ASSERT_EQUAL(1, host_event_count());
    for (unsigned int pass = 2U; pass < 10U; pass++)
    {
        app_run_once();
    }
    TEST_ASSERT_EQUAL(1, host_event_count()); // Step 1 begins on the tenth pass
    app_run_once();
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(1U, HOST_EVENT_STEP_NOTE, 1U, 14U);
}



static void test_step_read_failure_logs_error_instead_of_the_note(void)
{
    start_app();
    host_set_steps(NULL, ERR_INVALID_PARAM);
    run_ticks(2U);

    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_STEP_READ, (uint16_t)ERR_INVALID_PARAM);
}



static void test_tempo_read_failure_logs_error_and_keeps_the_tempo(void)
{
    start_app();
    run_ticks(2U);
    TEST_ASSERT_EQUAL(1, host_event_count());

    // From the next scan on the tempo cannot be read
    host_set_tempo(0U, ERR_TIMEOUT);
    run_ticks(TICKS_PER_STEP - 2U);
    TEST_ASSERT_EQUAL(1, host_event_count());

    // Step 1 is still 100 ticks after step 0. Its note first, then the error
    run_ticks(1U);
    TEST_ASSERT_EQUAL(3, host_event_count());
    expect_event(1U, HOST_EVENT_STEP_NOTE, 1U, 14U);
    expect_event(2U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_TEMPO_READ, (uint16_t)ERR_TIMEOUT);
}



static void test_both_reads_failing_logs_both_errors_in_order(void)
{
    start_app();
    host_set_steps(NULL, ERR_GENERAL);
    host_set_tempo(0U, ERR_TIMEOUT);
    run_ticks(2U);

    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(0U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_STEP_READ, (uint16_t)ERR_GENERAL);
    expect_event(1U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_TEMPO_READ, (uint16_t)ERR_TIMEOUT);
}



static void test_failed_reads_are_reported_once_per_step_not_per_scan(void)
{
    start_app();
    host_set_steps(NULL, ERR_GENERAL);
    host_set_tempo(0U, ERR_TIMEOUT);

    // With no tempo reading the clock runs at its 30 BPM floor: 500 ticks a step
    run_ticks(400U);
    TEST_ASSERT_EQUAL(50, host_step_read_count()); // Every one of them failed
    TEST_ASSERT_EQUAL(2, host_event_count());      // Reported with step 0 only
}



static void test_init_resets_the_step_and_discards_pending_outputs(void)
{
    start_app();
    run_ticks(TICKS_PER_STEP); // Step 0 logged; step 1 begun but not yet logged
    TEST_ASSERT_EQUAL(1, host_event_count());

    start_app(); // Re-initialize: the position and the unlogged step are forgotten
    run_ticks(1U);
    TEST_ASSERT_EQUAL(0, host_event_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 15U);
}



static void test_init_forgets_earlier_read_failures(void)
{
    start_app();
    host_set_steps(NULL, ERR_GENERAL);
    host_set_tempo(0U, ERR_TIMEOUT);
    run_ticks(2U);

    start_app();
    run_ticks(2U);
    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 15U);
}



int main(void)
{
    RUN_TEST(test_init_success_is_silent);
    RUN_TEST(test_init_failure_is_logged_sent_and_returned);
    RUN_TEST(test_pass_with_no_tick_only_polls_the_log);
    RUN_TEST(test_log_is_polled_on_every_pass);
    RUN_TEST(test_step_is_logged_one_tick_after_it_begins);
    RUN_TEST(test_steps_are_logged_at_the_tempo_and_wrap);
    RUN_TEST(test_fastest_tempo_logs_19_steps_a_second);
    RUN_TEST(test_inputs_are_read_every_eighth_tick);
    RUN_TEST(test_scan_timing_counts_elapsed_ticks_not_passes);
    RUN_TEST(test_tick_reads_the_whole_chain);
    RUN_TEST(test_elapsed_ticks_reach_the_core);
    RUN_TEST(test_step_read_failure_logs_error_instead_of_the_note);
    RUN_TEST(test_tempo_read_failure_logs_error_and_keeps_the_tempo);
    RUN_TEST(test_both_reads_failing_logs_both_errors_in_order);
    RUN_TEST(test_failed_reads_are_reported_once_per_step_not_per_scan);
    RUN_TEST(test_init_resets_the_step_and_discards_pending_outputs);
    RUN_TEST(test_init_forgets_earlier_read_failures);
    return TEST_RESULT();
}
