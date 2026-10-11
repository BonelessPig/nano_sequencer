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
#include "cal_record.h"
#include "host_ports.h"
#include "midi_output_port.h"
#include "seq.h"
#include "test_harness.h"

#define TEMPO_RAW_150_BPM (480U)  // A step every 100 ticks
#define TEMPO_RAW_MAX     (1023U) // 285 BPM: 19 steps every 1000 ticks
#define TICKS_PER_STEP    (100U)  // At TEMPO_RAW_150_BPM
#define SCAN_PERIOD_TICKS (8U)    // How often the app re-reads its inputs
#define CAL_TRIM_CENTRE   (512U)  // Tempo reading that trims nothing in calibration mode
#define CAL_CONFIRM_TICKS (250U)  // The gate is low this long after a store
#define SETTLE_TICKS      (20U)   // Debounce time of the run button and sync switch
#define SYNC_RAW_ONE_TO_ONE (500U) // Tempo reading that is a step to an external edge
#define CRASH_LOG_ADDRESS    (16U)   // Where the app keeps the crash log
#define CRASH_RECORD_BYTES   (5U)
#define RESET_CAUSE_WATCHDOG (0x08U)
#define PLACE_START_UP       (1U)    // The breadcrumb before the first tick
#define PLACE_BETWEEN_TICKS  (5U)    // And once a tick has finished
#define MIDI_CLOCK           (0xF8U)
#define MIDI_START           (0xFAU)
#define MIDI_STOP            (0xFCU)

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
 * @brief The same without clearing the fakes first: the app starts again as
 *        after a power cycle, with the non-volatile storage as it was left.
 */
static void start_app_keeping_storage(void)
{
    (void)app_init();
    host_set_steps(g_descending_notes, STATUS_OK);
    host_set_tempo(TEMPO_RAW_150_BPM, STATUS_OK);
}



/**
 * @brief Sets the panel as calibration mode uses it: the entry pattern on
 *        the last four steps, with the store switch (the lowest of step 15)
 *        on or off, and the point to set on step 0.
 */
static void set_calibration_panel(uint8_t point, bool b_store_on)
{
    uint8_t raw[HOST_RAW_STEP_BYTES] =
    {
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xA5U, 0xA4U, 0x00U
    };

    raw[0] = (uint8_t)(point << 4U);
    if (b_store_on)
    {
        raw[7] = 0xA5U;
    }
    host_set_steps(raw, STATUS_OK);
}



/**
 * @brief Fresh fakes and a freshly initialized app whose panel asks for
 *        calibration mode, at the given point and tempo control reading.
 */
static void start_calibration(uint8_t point, uint16_t tempo_raw)
{
    host_reset();
    (void)app_init();
    set_calibration_panel(point, true);
    host_set_tempo(tempo_raw, STATUS_OK);
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

    // Step n begins on tick 100 * n + 1 and is logged on the tick after
    run_ticks(TICKS_PER_STEP - 1U);
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

    // Step 0 on tick 1, step 19 on tick 1001, each logged a tick later
    run_ticks(1001U);
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



static void test_mode_switches_reach_the_core(void)
{
    // The descending notes with the mode switches choosing the stage
    // engine, and stage 0 set to repeat its note over two steps
    static const uint8_t stages[HOST_RAW_STEP_BYTES] =
    {
        0xFEU, 0xDCU, 0xBAU, 0x98U, 0x56U, 0x54U, 0x32U, 0x10U, 0x01U
    };

    start_app();
    host_set_steps(stages, STATUS_OK);
    run_ticks(2U + (2U * TICKS_PER_STEP));

    TEST_ASSERT_EQUAL(3, host_event_count());
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 14U);
    expect_event(1U, HOST_EVENT_STEP_NOTE, 0U, 14U);
    expect_event(2U, HOST_EVENT_STEP_NOTE, 1U, 13U);
}



static void test_step_lights_are_given_on_every_tick_and_follow_the_step(void)
{
    start_app();
    host_set_elapsed_ticks(0U);
    app_run_once();
    TEST_ASSERT_EQUAL(0, host_led_write_count()); // Not between ticks

    // Step 0 begins on the first tick and its light is given on the second
    run_ticks(1U);
    TEST_ASSERT_EQUAL(1, host_led_write_count());
    TEST_ASSERT_EQUAL(0, host_leds());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(0x0001, host_leds());

    // Lit for the first half of the step, then step 1's
    run_ticks(TICKS_PER_STEP / 2U);
    TEST_ASSERT_EQUAL(0, host_leds());
    run_ticks(TICKS_PER_STEP / 2U);
    TEST_ASSERT_EQUAL(0x0002, host_leds());
    TEST_ASSERT_EQUAL(2U + TICKS_PER_STEP, host_led_write_count());

    (void)app_init();
    run_ticks(1U);
    TEST_ASSERT_EQUAL(0, host_leds()); // Nothing left over from before
}



static void test_watchdog_is_kicked_on_every_tick_and_only_then(void)
{
    start_app();
    TEST_ASSERT_EQUAL(0, host_watchdog_kick_count()); // Not by init

    // A loop going round with no tick must not keep the watchdog quiet
    host_set_elapsed_ticks(0U);
    for (unsigned int pass = 0U; pass < 50U; pass++)
    {
        app_run_once();
    }
    TEST_ASSERT_EQUAL(0, host_watchdog_kick_count());

    run_ticks(3U);
    TEST_ASSERT_EQUAL(3, host_watchdog_kick_count());

    // Once for a late pass, however many ticks it brings
    host_set_elapsed_ticks(40U);
    app_run_once();
    TEST_ASSERT_EQUAL(4, host_watchdog_kick_count());
}



static void test_elapsed_ticks_reach_the_core(void)
{
    // Ten ticks at a time, a 100-tick step begins on every tenth pass. The
    // time on the pass that starts the pattern is not counted
    start_app();
    host_set_elapsed_ticks(10U);

    app_run_once(); // Step 0 begins
    app_run_once(); // And is logged
    TEST_ASSERT_EQUAL(1, host_event_count());
    for (unsigned int pass = 2U; pass < 11U; pass++)
    {
        app_run_once();
    }
    TEST_ASSERT_EQUAL(1, host_event_count()); // Step 1 begins on the eleventh pass
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

    // The same again for step 1, which begins on tick 101
    run_ticks(TICKS_PER_STEP - 1U);
    TEST_ASSERT_EQUAL(1, host_gate_rise_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(2, host_gate_rise_count());
    TEST_ASSERT_EQUAL(2, host_event_count());
    TEST_ASSERT_EQUAL(1, host_event_count_at_gate_rise());
}



static void test_pitch_cv_is_in_place_before_the_gate_opens(void)
{
    start_app(); // Step 0 plays pitch 14, step 1 pitch 13

    // The first tick works out step 0; the pitch written is still the 0 V
    // the app starts from
    run_ticks(1U);
    TEST_ASSERT_EQUAL(1, host_cv_write_count());
    TEST_ASSERT_EQUAL(0, host_cv_millivolts());

    // The second tick applies it: 14 semitones at 1 V to the octave, and
    // already there when the gate went high
    run_ticks(1U);
    TEST_ASSERT_EQUAL(1167, host_cv_millivolts());
    TEST_ASSERT_EQUAL(1, host_gate_rise_count());
    TEST_ASSERT_EQUAL(1167, host_cv_millivolts_at_gate_rise());

    // It holds after the gate closes, until step 1 begins on tick 101
    run_ticks(TICKS_PER_STEP - 1U);
    TEST_ASSERT(!host_gate_level());
    TEST_ASSERT_EQUAL(1167, host_cv_millivolts());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(1083, host_cv_millivolts());
    TEST_ASSERT_EQUAL(1083, host_cv_millivolts_at_gate_rise());
}



static void test_pitch_cv_is_written_on_every_tick_only(void)
{
    start_app();
    run_ticks(25U);
    TEST_ASSERT_EQUAL(25, host_cv_write_count());

    // A pass with no tick leaves it alone
    host_set_elapsed_ticks(0U);
    app_run_once();
    TEST_ASSERT_EQUAL(25, host_cv_write_count());
}



static void test_pitch_cv_follows_the_ideal_table_and_holds_through_a_rest(void)
{
    // Step 1 an octave up, step 2 the highest chromatic pitch, step 3 a rest
    static const uint8_t notes[HOST_RAW_STEP_BYTES] =
    {
        0x1DU, 0xF0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
    };

    start_app();
    host_set_steps(notes, STATUS_OK);

    run_ticks(2U);
    TEST_ASSERT_EQUAL(0, host_cv_millivolts()); // Note value 1: the lowest pitch
    run_ticks(TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(1000, host_cv_millivolts());
    run_ticks(TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(1167, host_cv_millivolts());
    run_ticks(TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(4, host_event_count()); // The rest has begun
    TEST_ASSERT_EQUAL(1167, host_cv_millivolts());
}



static void test_init_puts_the_pitch_cv_back_to_zero(void)
{
    start_app();
    run_ticks(10U);
    TEST_ASSERT_EQUAL(1167, host_cv_millivolts());

    (void)app_init();
    run_ticks(1U);
    TEST_ASSERT_EQUAL(0, host_cv_millivolts());
}



static void test_cv_write_failure_is_logged_once_per_step_after_the_note(void)
{
    start_app();
    host_set_cv_status(ERR_TIMEOUT);

    // Every tick's write fails; it is reported with the step, not per tick
    run_ticks(2U);
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 14U);
    expect_event(1U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_CV_WRITE, (uint16_t)ERR_TIMEOUT);
    TEST_ASSERT_EQUAL(0, host_cv_millivolts());
    TEST_ASSERT(host_gate_level()); // The note still plays, at the old pitch

    run_ticks(TICKS_PER_STEP - 1U);
    TEST_ASSERT_EQUAL(2, host_event_count());

    // Once the writes work again, the next tick puts the pitch right
    host_set_cv_status(STATUS_OK);
    run_ticks(1U);
    TEST_ASSERT_EQUAL(3, host_event_count());
    expect_event(2U, HOST_EVENT_STEP_NOTE, 1U, 13U);
    TEST_ASSERT_EQUAL(1083, host_cv_millivolts());
}



static void test_every_failure_at_once_is_logged_in_order(void)
{
    start_app();
    host_set_steps(NULL, ERR_GENERAL);
    host_set_tempo(0U, ERR_TIMEOUT);
    host_set_cv_status(ERR_BUSY);
    run_ticks(2U);

    TEST_ASSERT_EQUAL(3, host_event_count());
    expect_event(0U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_STEP_READ, (uint16_t)ERR_GENERAL);
    expect_event(1U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_TEMPO_READ, (uint16_t)ERR_TIMEOUT);
    expect_event(2U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_CV_WRITE, (uint16_t)ERR_BUSY);
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

    // Step 0 begins on tick 1: its gate is worked out as open on ticks 1 to
    // 50 and applied on ticks 2 to 51
    run_ticks(51U);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT(host_clock_output_level());
    run_ticks(1U);
    TEST_ASSERT(!host_gate_level());
    TEST_ASSERT(!host_clock_output_level());

    // Step 1 begins on tick 101: high from tick 102 to tick 151
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
    run_ticks(TICKS_PER_STEP - 1U);
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



static void test_the_run_button_stops_and_starts_the_pattern_and_is_logged(void)
{
    start_app();
    run_ticks(10U);
    TEST_ASSERT_EQUAL(1, host_event_count());
    TEST_ASSERT(host_gate_level());

    // The press counts on the twentieth tick and is applied on the next
    host_set_run_button(true);
    run_ticks(SETTLE_TICKS);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT_EQUAL(1, host_event_count());
    run_ticks(1U);
    TEST_ASSERT(!host_gate_level());
    TEST_ASSERT(!host_clock_output_level());
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(1U, HOST_EVENT_TRANSPORT, (uint16_t)TRANSPORT_STOPPED, 0U);

    // Stopped: nothing plays and nothing more is logged
    host_set_run_button(false);
    run_ticks(5U * TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(2, host_event_count());
    TEST_ASSERT_EQUAL(1, host_gate_rise_count());
    TEST_ASSERT_EQUAL(1, host_clock_output_rise_count());

    // The next press starts the pattern from step 0: the transport is logged
    // first, then the step
    host_set_run_button(true);
    run_ticks(SETTLE_TICKS + 1U);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT_EQUAL(4, host_event_count());
    expect_event(2U, HOST_EVENT_TRANSPORT, (uint16_t)TRANSPORT_INTERNAL, 0U);
    expect_event(3U, HOST_EVENT_STEP_NOTE, 0U, 14U);

    // And step 1 follows a whole step later
    host_set_run_button(false);
    run_ticks(TICKS_PER_STEP - 1U);
    TEST_ASSERT_EQUAL(4, host_event_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(5, host_event_count());
    expect_event(4U, HOST_EVENT_STEP_NOTE, 1U, 13U);
}



static void test_the_sync_switch_steps_the_pattern_from_the_clock_input(void)
{
    start_app();
    host_set_tempo(SYNC_RAW_ONE_TO_ONE, STATUS_OK);
    host_set_sync_switch(true);

    // On from the first tick: the change is logged and no step begins
    run_ticks(2U);
    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_TRANSPORT, (uint16_t)TRANSPORT_EXTERNAL, 0U);
    run_ticks(3U * TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(1, host_event_count());
    TEST_ASSERT_EQUAL(0, host_clock_output_rise_count());

    // An edge is taken on the next tick, whether or not it is a scan tick,
    // and its step is applied on the one after
    for (uint16_t step = 0U; step < 4U; step++)
    {
        host_pulse_clock_input();
        run_ticks(1U);
        TEST_ASSERT_EQUAL(step + 1U, host_event_count());
        run_ticks(1U);
        TEST_ASSERT_EQUAL(step + 2U, host_event_count());
        expect_event((uint16_t)(step + 1U), HOST_EVENT_STEP_NOTE, step, (uint16_t)(14U - step));
        TEST_ASSERT_EQUAL(step + 1U, host_gate_rise_count());

        // One edge is one step, however long the wait for the next
        run_ticks(35U + (step * 50U));
        TEST_ASSERT_EQUAL(step + 2U, host_event_count());
    }
}



static void test_the_reset_input_sends_the_pattern_back_to_its_start(void)
{
    start_app();
    run_ticks(250U); // Steps 0 to 2 have begun
    TEST_ASSERT_EQUAL(3, host_event_count());

    // The pattern goes back, and the first step plays where step 3 would have
    host_pulse_reset_input();
    run_ticks(51U);
    TEST_ASSERT_EQUAL(3, host_event_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(4, host_event_count());
    expect_event(3U, HOST_EVENT_STEP_NOTE, 0U, 14U);

    // The edge is acted on once: step 1 follows
    run_ticks(TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(5, host_event_count());
    expect_event(4U, HOST_EVENT_STEP_NOTE, 1U, 13U);
}



static void test_init_starts_a_stopped_pattern_running_again(void)
{
    start_app();
    run_ticks(1U);
    host_set_run_button(true);
    run_ticks(SETTLE_TICKS + 1U);
    host_set_run_button(false);
    run_ticks(TICKS_PER_STEP);
    TEST_ASSERT(!host_gate_level());

    // A fresh start: running, with no change of transport to report
    start_app();
    run_ticks(2U);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT_EQUAL(1, host_event_count());
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 14U);
}



static void test_calibration_mode_leaves_the_transport_alone(void)
{
    start_calibration(2U, CAL_TRIM_CENTRE);
    host_set_sync_switch(true);
    host_set_run_button(true);
    host_pulse_clock_input();
    host_pulse_reset_input();
    run_ticks(3U * SETTLE_TICKS);

    TEST_ASSERT_EQUAL(0, host_event_count());
    TEST_ASSERT_EQUAL(2000, host_cv_millivolts());
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT(!host_clock_output_level());
}



static void test_the_midi_fake_records_what_is_written_and_scripts_the_room(void)
{
    static const uint8_t note_on[]  = { 0x90U, 0x18U, 0x64U };
    static const uint8_t note_off[] = { 0x80U, 0x18U, 0x00U };

    // The port is called here directly: this is the fake's own behaviour
    host_reset();
    TEST_ASSERT_EQUAL(HOST_MIDI_ROOM_DEFAULT, midi_output_room());
    TEST_ASSERT_EQUAL(0, host_midi_byte_count());
    TEST_ASSERT_EQUAL(0, host_midi_byte(0U));

    // The room is whatever was scripted, and is not held to
    host_set_midi_room(2U);
    TEST_ASSERT_EQUAL(2, midi_output_room());
    TEST_ASSERT_EQUAL(2, host_midi_room_read_count());
    midi_output_write(note_on, (uint8_t)sizeof(note_on));
    midi_output_write(note_off, (uint8_t)sizeof(note_off));
    midi_output_write(NULL, 3U);
    midi_output_write(note_on, 0U);

    TEST_ASSERT_EQUAL(4, host_midi_write_count());
    TEST_ASSERT_EQUAL(6, host_midi_byte_count());
    for (uint16_t i = 0U; i < 3U; i++)
    {
        TEST_ASSERT_EQUAL(note_on[i], host_midi_byte(i));
        TEST_ASSERT_EQUAL(note_off[i], host_midi_byte((uint16_t)(i + 3U)));
    }
    TEST_ASSERT_EQUAL(0, host_midi_byte(6U)); // Past the end of the record

    // A poll sends nothing and takes nothing away; it is only counted
    midi_output_poll();
    midi_output_poll();
    TEST_ASSERT_EQUAL(2, host_midi_poll_count());
    TEST_ASSERT_EQUAL(6, host_midi_byte_count());

    // The record is bounded: bytes past its end are dropped
    for (unsigned int i = 0U; i < HOST_MIDI_BYTES_MAX; i++)
    {
        midi_output_write(note_off, 1U);
    }
    TEST_ASSERT_EQUAL(HOST_MIDI_BYTES_MAX, host_midi_byte_count());
    TEST_ASSERT_EQUAL(0x80, host_midi_byte((uint16_t)(HOST_MIDI_BYTES_MAX - 1U)));
    TEST_ASSERT_EQUAL(0, host_midi_byte((uint16_t)HOST_MIDI_BYTES_MAX));

    host_reset();
    TEST_ASSERT_EQUAL(0, host_midi_byte_count());
    TEST_ASSERT_EQUAL(0, host_midi_write_count());
    TEST_ASSERT_EQUAL(0, host_midi_poll_count());
    TEST_ASSERT_EQUAL(0, host_midi_room_read_count());
    TEST_ASSERT_EQUAL(HOST_MIDI_ROOM_DEFAULT, midi_output_room());
}



/**
 * @brief Counts the MIDI clock bytes among those written so far.
 */
static unsigned int midi_clock_count(void)
{
    unsigned int clocks = 0U;

    for (uint16_t i = 0U; i < host_midi_byte_count(); i++)
    {
        if (MIDI_CLOCK == host_midi_byte(i))
        {
            clocks++;
        }
    }
    return clocks;
}



/**
 * @brief Checks that the MIDI bytes written so far, with the clocks left
 *        out, are exactly the expected ones in order.
 */
static void expect_midi_without_clocks(const uint8_t *p_expected, unsigned int count)
{
    unsigned int found = 0U;

    for (uint16_t i = 0U; i < host_midi_byte_count(); i++)
    {
        const uint8_t byte = host_midi_byte(i);

        if (MIDI_CLOCK != byte)
        {
            if (found < count)
            {
                TEST_ASSERT_EQUAL(p_expected[found], byte);
            }
            found++;
        }
    }
    TEST_ASSERT_EQUAL(count, found);
}



static void test_midi_start_clock_and_note_are_queued_one_tick_after_the_step_begins(void)
{
    // Step 0 is pitch 14, which is note 24 + 14
    static const uint8_t expected[] = { MIDI_START, MIDI_CLOCK, 0x90U, 38U, 100U };

    start_app();

    // The first tick works the bytes out; the write it makes is of none
    run_ticks(1U);
    TEST_ASSERT_EQUAL(1, host_midi_write_count());
    TEST_ASSERT_EQUAL(0, host_midi_byte_count());

    run_ticks(1U);
    TEST_ASSERT_EQUAL(sizeof(expected), host_midi_byte_count());
    for (uint16_t i = 0U; i < sizeof(expected); i++)
    {
        TEST_ASSERT_EQUAL(expected[i], host_midi_byte(i));
    }

    // Written, and the room asked for, once on every tick
    run_ticks(8U);
    TEST_ASSERT_EQUAL(10, host_midi_write_count());
    TEST_ASSERT_EQUAL(10, host_midi_room_read_count());
}



static void test_midi_is_queued_after_the_gate_and_before_the_log(void)
{
    start_app();
    run_ticks(2U);
    TEST_ASSERT_EQUAL(0, host_midi_byte_count_at_gate_rise());
    TEST_ASSERT_EQUAL(5, host_midi_byte_count_at_first_event());
}



static void test_midi_note_ends_with_the_gate_and_clocks_are_six_a_step(void)
{
    static const uint8_t expected[] =
    {
        MIDI_START, 0x90U, 38U, 100U, 0x80U, 38U, 0U, 0x90U, 37U, 100U
    };

    // The gate falls on tick 51 and step 1 begins on tick 101; each is
    // queued a tick later. A clock falls on both
    start_app();
    run_ticks(51U);
    TEST_ASSERT_EQUAL(4, host_midi_byte_count() - midi_clock_count());
    TEST_ASSERT_EQUAL(3, midi_clock_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(7, host_midi_byte_count() - midi_clock_count());
    TEST_ASSERT_EQUAL(4, midi_clock_count());

    run_ticks(TICKS_PER_STEP / 2U);
    expect_midi_without_clocks(expected, sizeof(expected));
    TEST_ASSERT_EQUAL(7, midi_clock_count());
}



static void test_midi_plays_the_pattern_and_a_rest_sends_no_note(void)
{
    unsigned int note_ons  = 0U;
    unsigned int note_offs = 0U;

    // Once round, and step 0 again: fifteen notes and a rest, then a note
    start_app();
    run_ticks((PANEL_STEP_COUNT * TICKS_PER_STEP) + 2U);

    for (uint16_t i = 0U; i < host_midi_byte_count(); i++)
    {
        if (0x90U == host_midi_byte(i))
        {
            note_ons++;
        }
        if (0x80U == host_midi_byte(i))
        {
            note_offs++;
        }
    }
    TEST_ASSERT_EQUAL(16, note_ons);
    TEST_ASSERT_EQUAL(15, note_offs);
    TEST_ASSERT_EQUAL(1U + (6U * PANEL_STEP_COUNT), midi_clock_count());
}



static void test_midi_takes_no_more_than_the_output_has_room_for(void)
{
    static const uint8_t expected[] = { MIDI_START, MIDI_CLOCK, 0x90U, 38U, 100U };

    // Room for one byte a tick: the Start, then the clock, and the note waits
    start_app();
    host_set_midi_room(1U);
    run_ticks(2U);
    TEST_ASSERT_EQUAL(1, host_midi_byte_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(2, host_midi_byte_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(2, host_midi_byte_count());

    // The room is read on one tick, used by the core on it, and what was
    // worked out is written on the next
    host_set_midi_room(HOST_MIDI_ROOM_DEFAULT);
    run_ticks(1U);
    TEST_ASSERT_EQUAL(2, host_midi_byte_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(sizeof(expected), host_midi_byte_count());
    for (uint16_t i = 0U; i < sizeof(expected); i++)
    {
        TEST_ASSERT_EQUAL(expected[i], host_midi_byte(i));
    }
}



static void test_midi_is_polled_on_every_pass_tick_or_not(void)
{
    start_app();
    host_set_elapsed_ticks(0U);
    for (unsigned int pass = 0U; pass < 10U; pass++)
    {
        app_run_once();
    }
    TEST_ASSERT_EQUAL(10, host_midi_poll_count());
    TEST_ASSERT_EQUAL(0, host_midi_write_count());
    TEST_ASSERT_EQUAL(0, host_midi_room_read_count());

    run_ticks(5U);
    TEST_ASSERT_EQUAL(15, host_midi_poll_count());
}



static void test_midi_stop_and_start_follow_the_run_button_and_the_clock_carries_on(void)
{
    static const uint8_t expected[] =
    {
        MIDI_START, 0x90U, 38U, 100U, MIDI_STOP, 0x80U, 38U, 0U,
        MIDI_START, 0x90U, 38U, 100U
    };
    unsigned int clocks;
    uint16_t     bytes;

    start_app();
    run_ticks(10U);
    host_set_run_button(true);
    run_ticks(SETTLE_TICKS + 1U);
    host_set_run_button(false);
    expect_midi_without_clocks(expected, 8U);

    // Stopped: the clock goes on at the tempo, six to a step's time
    clocks = midi_clock_count();
    run_ticks(4U * TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(clocks + 24U, midi_clock_count());
    expect_midi_without_clocks(expected, 8U);

    // A start sends Start and its clock at once, then the first note. The
    // clocks of the stopped pattern go on until the press has settled
    host_set_run_button(true);
    run_ticks(SETTLE_TICKS + 1U);
    host_set_run_button(false);
    expect_midi_without_clocks(expected, sizeof(expected));
    bytes = (uint16_t)(host_midi_byte_count() - 5U);
    TEST_ASSERT_EQUAL(MIDI_START, host_midi_byte(bytes));
    TEST_ASSERT_EQUAL(MIDI_CLOCK, host_midi_byte((uint16_t)(bytes + 1U)));
    TEST_ASSERT_EQUAL(0x90, host_midi_byte((uint16_t)(bytes + 2U)));
}



static void test_midi_clock_on_the_external_clock_waits_for_the_first_edge(void)
{
    static const uint8_t first[] = { MIDI_START };
    static const uint8_t both[]  = { MIDI_START, MIDI_CLOCK, 0x90U, 38U, 100U };

    start_app();
    host_set_tempo(SYNC_RAW_ONE_TO_ONE, STATUS_OK);
    host_set_sync_switch(true);

    // Started, with no edge yet: Start and nothing after it
    run_ticks(3U * TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(sizeof(first), host_midi_byte_count());
    TEST_ASSERT_EQUAL(first[0], host_midi_byte(0U));

    // The edge that begins step 0 brings the first clock with it
    host_pulse_clock_input();
    run_ticks(2U);
    TEST_ASSERT_EQUAL(sizeof(both), host_midi_byte_count());
    for (uint16_t i = 0U; i < sizeof(both); i++)
    {
        TEST_ASSERT_EQUAL(both[i], host_midi_byte(i));
    }
}



static void test_calibration_mode_sends_no_midi(void)
{
    start_calibration(2U, CAL_TRIM_CENTRE);
    run_ticks(3U * TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(0, host_midi_byte_count());
    TEST_ASSERT_EQUAL(3U * TICKS_PER_STEP, host_midi_poll_count());
}



static void test_init_discards_midi_bytes_not_yet_queued(void)
{
    // The first tick has worked out a Start, a clock and a note
    start_app();
    run_ticks(1U);

    // Starting again forgets them, and plays its own start from the top
    start_app_keeping_storage();
    run_ticks(1U);
    TEST_ASSERT_EQUAL(0, host_midi_byte_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(5, host_midi_byte_count());
    TEST_ASSERT_EQUAL(MIDI_START, host_midi_byte(0U));
}



int main(void)
{
    RUN_TEST(test_midi_start_clock_and_note_are_queued_one_tick_after_the_step_begins);
    RUN_TEST(test_midi_is_queued_after_the_gate_and_before_the_log);
    RUN_TEST(test_midi_note_ends_with_the_gate_and_clocks_are_six_a_step);
    RUN_TEST(test_midi_plays_the_pattern_and_a_rest_sends_no_note);
    RUN_TEST(test_midi_takes_no_more_than_the_output_has_room_for);
    RUN_TEST(test_midi_is_polled_on_every_pass_tick_or_not);
    RUN_TEST(test_midi_stop_and_start_follow_the_run_button_and_the_clock_carries_on);
    RUN_TEST(test_midi_clock_on_the_external_clock_waits_for_the_first_edge);
    RUN_TEST(test_calibration_mode_sends_no_midi);
    RUN_TEST(test_init_discards_midi_bytes_not_yet_queued);
    RUN_TEST(test_init_reports_the_reset_cause_and_no_calibration);
    RUN_TEST(test_init_failure_reads_no_calibration_and_reports_no_start);
    RUN_TEST(test_a_stored_calibration_is_loaded_and_played);
    RUN_TEST(test_a_record_that_is_not_valid_leaves_the_ideal_table);
    RUN_TEST(test_a_failed_calibration_read_is_logged_and_leaves_the_ideal_table);
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
    RUN_TEST(test_mode_switches_reach_the_core);
    RUN_TEST(test_step_lights_are_given_on_every_tick_and_follow_the_step);
    RUN_TEST(test_watchdog_is_kicked_on_every_tick_and_only_then);
    RUN_TEST(test_elapsed_ticks_reach_the_core);
    RUN_TEST(test_gate_opens_one_tick_after_its_step_begins_and_before_the_log);
    RUN_TEST(test_pitch_cv_is_in_place_before_the_gate_opens);
    RUN_TEST(test_pitch_cv_is_written_on_every_tick_only);
    RUN_TEST(test_pitch_cv_follows_the_ideal_table_and_holds_through_a_rest);
    RUN_TEST(test_init_puts_the_pitch_cv_back_to_zero);
    RUN_TEST(test_cv_write_failure_is_logged_once_per_step_after_the_note);
    RUN_TEST(test_every_failure_at_once_is_logged_in_order);
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
    RUN_TEST(test_the_run_button_stops_and_starts_the_pattern_and_is_logged);
    RUN_TEST(test_the_sync_switch_steps_the_pattern_from_the_clock_input);
    RUN_TEST(test_the_reset_input_sends_the_pattern_back_to_its_start);
    RUN_TEST(test_init_starts_a_stopped_pattern_running_again);
    RUN_TEST(test_calibration_mode_leaves_the_transport_alone);
    RUN_TEST(test_the_midi_fake_records_what_is_written_and_scripts_the_room);
    return TEST_RESULT();
}
