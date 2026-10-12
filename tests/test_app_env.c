/**
 * @file   test_app_env.c
 * @brief  Host tests for the application loop (app/app.c): the envelope
 *         output, the amplitude and time the pot sets for it, and the MIDI
 *         velocity that goes with the amplitude.
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
#include "test_harness.h"
#include "app_fixture.h"

#define ENV_TOP_MILLIVOLTS (4095U)
#define GATE_TICKS         (50U)  // Half a step at TEMPO_RAW_150_BPM
#define POSITION_AMPLITUDE (5U)   // Selector positions
#define POSITION_ENV_TIME  (6U)
#define AMOUNT_16_MS       (128U) // 32 squared over 64
#define MIDI_NOTE_ON       (0x90U)

/**
 * @brief The velocity of the last Note On written, or 0 if there is none.
 *        Only whole messages are written, so the byte two on is its own.
 */
static uint8_t last_note_on_velocity(void)
{
    uint8_t velocity = 0U;

    for (uint16_t i = 0U; i < host_midi_byte_count(); i++)
    {
        if (MIDI_NOTE_ON == host_midi_byte(i))
        {
            velocity = host_midi_byte((uint16_t)(i + 2U));
        }
    }
    return velocity;
}



static void test_the_envelope_is_written_on_every_tick_and_follows_the_gate_at_power_on(void)
{
    // Nothing has been worked out when the first tick applies
    start_app();
    run_ticks(1U);
    TEST_ASSERT_EQUAL(1, host_env_write_count());
    TEST_ASSERT_EQUAL(0, host_env_millivolts());

    // Step 0's gate, applied a tick after it was worked out, and the
    // envelope with it: no time is set, so it is the gate at full height
    run_ticks(1U);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT_EQUAL(ENV_TOP_MILLIVOLTS, host_env_millivolts());
    run_ticks(GATE_TICKS - 1U);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT_EQUAL(ENV_TOP_MILLIVOLTS, host_env_millivolts());

    run_ticks(1U);
    TEST_ASSERT(!host_gate_level());
    TEST_ASSERT_EQUAL(0, host_env_millivolts());
    TEST_ASSERT_EQUAL(GATE_TICKS + 2U, host_env_write_count());
    TEST_ASSERT_EQUAL(host_cv_write_count(), host_env_write_count());
}



static void test_position_6_is_the_time_of_a_one_shot_envelope(void)
{
    // A slope of 16 ms: step 1 begins on tick 101 and is applied on 102
    start_app();
    run_ticks(2U);
    set_amount(POSITION_ENV_TIME, AMOUNT_16_MS);
    run_ticks(TICKS_PER_STEP - 20U);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT_EQUAL(255, host_env_millivolts()); // 4095 in 16 ms, one tick of it

    run_ticks(15U);
    TEST_ASSERT_EQUAL(ENV_TOP_MILLIVOLTS, host_env_millivolts());

    // And down again at once, with the gate still open
    run_ticks(1U);
    TEST_ASSERT_EQUAL(ENV_TOP_MILLIVOLTS - 255U, host_env_millivolts());
    run_ticks(15U);
    TEST_ASSERT_EQUAL(0, host_env_millivolts());
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT_EQUAL(2, host_event_count()); // Steps 0 and 1: the tempo is as it was
}



static void test_position_5_lets_the_lfo_bring_the_peak_and_the_velocity_down(void)
{
    // Step 0, with no amount: full height, and velocity 100
    start_app();
    host_set_mod(256U, STATUS_OK);
    run_ticks(2U);
    TEST_ASSERT_EQUAL(ENV_TOP_MILLIVOLTS, host_env_millivolts());
    TEST_ASSERT_EQUAL(100, last_note_on_velocity());

    // Full amount, with the LFO half way down: a depth of 128 off 255 is
    // an amplitude of 127, which is 2039 mV and velocity 50
    set_amount(POSITION_AMPLITUDE, 1023U);
    run_ticks(TICKS_PER_STEP - 20U);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT_EQUAL(2039, host_env_millivolts());
    TEST_ASSERT_EQUAL(50, last_note_on_velocity());

    // Above its centre the LFO takes nothing off
    host_set_mod(1023U, STATUS_OK);
    run_ticks(TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(ENV_TOP_MILLIVOLTS, host_env_millivolts());
    TEST_ASSERT_EQUAL(100, last_note_on_velocity());
}



static void test_a_failed_envelope_write_is_logged_with_the_step_as_the_pitch_is(void)
{
    start_app();
    host_set_env_status(ERR_TIMEOUT);

    run_ticks(2U);
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 14U);
    expect_event(1U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_CV_WRITE, (uint16_t)ERR_TIMEOUT);
    TEST_ASSERT_EQUAL(0, host_env_millivolts());
    TEST_ASSERT_EQUAL(1167, host_cv_millivolts()); // The pitch went all the same

    // With both failing it is the pitch's status that is logged
    host_set_cv_status(ERR_BUSY);
    run_ticks(TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(4, host_event_count());
    expect_event(3U, HOST_EVENT_ERROR, (uint16_t)LOG_ERROR_CV_WRITE, (uint16_t)ERR_BUSY);
}



static void test_the_envelope_stays_at_zero_in_calibration_mode(void)
{
    start_calibration(2U, CAL_TRIM_CENTRE);
    run_ticks(50U);
    TEST_ASSERT(host_gate_level());
    TEST_ASSERT_EQUAL(50, host_env_write_count());
    TEST_ASSERT_EQUAL(0, host_env_millivolts());
}



static void test_init_puts_the_envelope_back_to_zero(void)
{
    start_app();
    run_ticks(2U);
    TEST_ASSERT_EQUAL(ENV_TOP_MILLIVOLTS, host_env_millivolts());

    // A reset with a note sounding: the first tick writes 0
    start_calibration(2U, CAL_TRIM_CENTRE);
    run_ticks(1U);
    TEST_ASSERT_EQUAL(0, host_env_millivolts());
    TEST_ASSERT_EQUAL(1, host_env_write_count());
}



int main(void)
{
    RUN_TEST(test_the_envelope_is_written_on_every_tick_and_follows_the_gate_at_power_on);
    RUN_TEST(test_position_6_is_the_time_of_a_one_shot_envelope);
    RUN_TEST(test_position_5_lets_the_lfo_bring_the_peak_and_the_velocity_down);
    RUN_TEST(test_a_failed_envelope_write_is_logged_with_the_step_as_the_pitch_is);
    RUN_TEST(test_the_envelope_stays_at_zero_in_calibration_mode);
    RUN_TEST(test_init_puts_the_envelope_back_to_zero);
    return TEST_RESULT();
}
