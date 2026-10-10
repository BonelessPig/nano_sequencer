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

// Step n holds note value 15 - n, so the step and the note are never the same
// number. In the chromatic scale the app plays, that is pitch 14 - n, and
// step 15 (note value 0) is a rest
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
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 14U);

    // And it is applied once, not on every tick after
    run_ticks(10U);
    TEST_ASSERT_EQUAL(1, host_event_count());
}



static void test_steps_are_logged_at_the_tempo_and_wrap(void)
{
    const uint16_t step_count = (uint16_t)(PANEL_STEP_COUNT + 2U); // Once round and two more

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
        const uint16_t step = (uint16_t)(count % PANEL_STEP_COUNT);

        if (step < 15U)
        {
            expect_event(count, HOST_EVENT_STEP_NOTE, step, (uint16_t)(14U - step));
        }
        else
        {
            expect_event(count, HOST_EVENT_STEP_REST, step, 0U);
        }
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
    expect_event(19U, HOST_EVENT_STEP_NOTE, 3U, 11U);
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
    TEST_ASSERT_EQUAL(PANEL_RAW_BYTE_COUNT, host_last_step_byte_count());
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
    expect_event(1U, HOST_EVENT_STEP_NOTE, 1U, 13U);
}



static void test_gate_opens_one_tick_after_its_step_begins_and_before_the_log(void)
{
    start_app();

    // The first tick works out that step 0 begins; the gate is written, low
    run_ticks(1U);
    TEST_ASSERT_EQUAL(1, host_gate_write_count());
    TEST_ASSERT(!host_gate_level());
    TEST_ASSERT(!host_clock_output_level());

    // The second tick applies it: gate and clock output high, then the log
    run_ticks(1U);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT(host_clock_output_level());
    TEST_ASSERT_EQUAL(1, host_gate_rise_count());
    TEST_ASSERT_EQUAL(1, host_event_count());
    TEST_ASSERT_EQUAL(0, host_event_count_at_gate_rise());

    // The same again for step 1, which begins on tick 100
    run_ticks(TICKS_PER_STEP - 2U);
    TEST_ASSERT_EQUAL(1, host_gate_rise_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(2, host_gate_rise_count());
    TEST_ASSERT_EQUAL(2, host_event_count());
    TEST_ASSERT_EQUAL(1, host_event_count_at_gate_rise());
}



static void test_gate_and_clock_output_are_written_on_every_tick_only(void)
{
    start_app();
    run_ticks(25U);
    TEST_ASSERT_EQUAL(25, host_gate_write_count());

    // A pass with no tick leaves them alone
    host_set_elapsed_ticks(0U);
    app_run_once();
    TEST_ASSERT_EQUAL(25, host_gate_write_count());
}



static void test_gate_is_held_for_half_a_step(void)
{
    start_app();

    // Step 0 begins on tick 1, a tick short: its gate is worked out as open
    // on ticks 1 to 49 and applied on ticks 2 to 50
    run_ticks(50U);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT(host_clock_output_level());
    run_ticks(1U);
    TEST_ASSERT(!host_gate_level());
    TEST_ASSERT(!host_clock_output_level());

    // Step 1 begins on tick 100: high from tick 101 to tick 150
    run_ticks(49U);
    TEST_ASSERT(!host_gate_level());
    run_ticks(1U);
    TEST_ASSERT(host_gate_level());
    run_ticks(49U);
    TEST_ASSERT(host_gate_level());
    run_ticks(1U);
    TEST_ASSERT(!host_gate_level());
    TEST_ASSERT_EQUAL(2, host_gate_rise_count());
    TEST_ASSERT_EQUAL(2, host_clock_output_rise_count());
}



static void test_a_rest_has_a_clock_pulse_and_no_gate(void)
{
    start_app(); // Step 15 of the descending notes is the only rest
    run_ticks((PANEL_STEP_COUNT * TICKS_PER_STEP) + 2U);

    // Steps 0 to 14 and step 0 again have a gate; all seventeen have a clock
    TEST_ASSERT_EQUAL(16, host_gate_rise_count());
    TEST_ASSERT_EQUAL(17, host_clock_output_rise_count());
    TEST_ASSERT(host_gate_level()); // Step 0, the second time round
}



static void test_a_step_that_cannot_be_read_has_no_gate(void)
{
    start_app();
    host_set_steps(NULL, ERR_TIMEOUT);
    run_ticks(TICKS_PER_STEP);

    TEST_ASSERT_EQUAL(0, host_gate_rise_count());
    TEST_ASSERT_EQUAL(1, host_clock_output_rise_count());
}



static void test_init_puts_the_gate_and_clock_output_low_again(void)
{
    start_app();
    run_ticks(10U);
    TEST_ASSERT(host_gate_level());

    // Re-initialized mid-note: the first tick after it writes both low
    (void)app_init();
    run_ticks(1U);
    TEST_ASSERT(!host_gate_level());
    TEST_ASSERT(!host_clock_output_level());
}



static void test_rest_is_logged_as_a_rest(void)
{
    static const uint8_t rest_then_notes[HOST_RAW_STEP_BYTES] =
    {
        0x01U, 0x23U, 0x45U, 0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU
    };

    start_app();
    host_set_steps(rest_then_notes, STATUS_OK);
    run_ticks(2U);
    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_STEP_REST, 0U, 0U);

    // Note value 1 is the lowest pitch, 0, which is not a rest
    run_ticks(TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(1U, HOST_EVENT_STEP_NOTE, 1U, 0U);
}



static void test_app_plays_all_sixteen_steps_forward_and_chromatic(void)
{
    // Step n holds note value n: the pitches go up a semitone a step
    static const uint8_t ascending[HOST_RAW_STEP_BYTES] =
    {
        0x01U, 0x23U, 0x45U, 0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU
    };

    start_app();
    host_set_steps(ascending, STATUS_OK);
    run_ticks((PANEL_STEP_COUNT * TICKS_PER_STEP) + 2U);

    TEST_ASSERT_EQUAL(PANEL_STEP_COUNT + 1U, host_event_count());
    for (uint16_t step = 1U; step < PANEL_STEP_COUNT; step++)
    {
        expect_event(step, HOST_EVENT_STEP_NOTE, step, (uint16_t)(step - 1U));
    }
    expect_event((uint16_t)PANEL_STEP_COUNT, HOST_EVENT_STEP_REST, 0U, 0U); // Wrapped
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
    expect_event(1U, HOST_EVENT_STEP_NOTE, 1U, 13U);
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
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 14U);
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
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 14U);
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
    RUN_TEST(test_gate_opens_one_tick_after_its_step_begins_and_before_the_log);
    RUN_TEST(test_gate_and_clock_output_are_written_on_every_tick_only);
    RUN_TEST(test_gate_is_held_for_half_a_step);
    RUN_TEST(test_a_rest_has_a_clock_pulse_and_no_gate);
    RUN_TEST(test_a_step_that_cannot_be_read_has_no_gate);
    RUN_TEST(test_init_puts_the_gate_and_clock_output_low_again);
    RUN_TEST(test_rest_is_logged_as_a_rest);
    RUN_TEST(test_app_plays_all_sixteen_steps_forward_and_chromatic);
    RUN_TEST(test_step_read_failure_logs_error_instead_of_the_note);
    RUN_TEST(test_tempo_read_failure_logs_error_and_keeps_the_tempo);
    RUN_TEST(test_both_reads_failing_logs_both_errors_in_order);
    RUN_TEST(test_failed_reads_are_reported_once_per_step_not_per_scan);
    RUN_TEST(test_init_resets_the_step_and_discards_pending_outputs);
    RUN_TEST(test_init_forgets_earlier_read_failures);
    return TEST_RESULT();
}
