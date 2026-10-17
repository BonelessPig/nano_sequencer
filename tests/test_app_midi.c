/**
 * @file   test_app_midi.c
 * @brief  Host tests for the application loop (app/app.c) against the fake
 *         ports in adapters/host: MIDI out.
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

    // And twice on a pass with a tick: once between its steps as well, so
    // that a byte can leave while the tick's work is still being done
    run_ticks(5U);
    TEST_ASSERT_EQUAL(20, host_midi_poll_count());
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
    static const uint8_t after_start[] = { MIDI_CLOCK, 0x90U, 38U, 100U };
    uint16_t             bytes;

    // Stopped, and then an edge: the next start is on the external clock,
    // and from the edge no clock is sent
    start_app();
    host_set_tempo(SYNC_RAW_ONE_TO_ONE, STATUS_OK);
    run_ticks(10U);
    host_set_run_button(true);
    run_ticks(SETTLE_TICKS + 1U);
    host_set_run_button(false);
    host_pulse_clock_input();
    run_ticks(3U * TICKS_PER_STEP);
    bytes = host_midi_byte_count();
    run_ticks(3U * TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(bytes, host_midi_byte_count());

    // Started, with no edge yet: Start and nothing after it
    host_set_run_button(true);
    run_ticks(SETTLE_TICKS + 1U);
    host_set_run_button(false);
    run_ticks(3U * TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(bytes + 1U, host_midi_byte_count());
    TEST_ASSERT_EQUAL(MIDI_START, host_midi_byte(bytes));

    // The edge that begins step 0 brings the first clock with it
    host_pulse_clock_input();
    run_ticks(2U);
    TEST_ASSERT_EQUAL(bytes + 1U + sizeof(after_start), host_midi_byte_count());
    for (uint16_t i = 0U; i < sizeof(after_start); i++)
    {
        TEST_ASSERT_EQUAL(after_start[i], host_midi_byte((uint16_t)(bytes + 1U + i)));
    }
}



static void test_calibration_mode_sends_no_midi(void)
{
    start_calibration(2U, CAL_TRIM_CENTRE);
    run_ticks(3U * TICKS_PER_STEP);
    TEST_ASSERT_EQUAL(0, host_midi_byte_count());
    TEST_ASSERT_EQUAL(2U * 3U * TICKS_PER_STEP, host_midi_poll_count()); // Twice a tick
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
    RUN_TEST(test_the_midi_fake_records_what_is_written_and_scripts_the_room);
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
    return TEST_RESULT();
}
