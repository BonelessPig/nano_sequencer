/**
 * @file   test_midi_out_notes.c
 * @brief  Host unit tests for MIDI out (core/midi_out.c): notes and ties.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "midi_out.h"
#include "test_harness.h"
#include "midi_out_fixture.h"

static void test_a_note_sends_note_on_and_its_gate_falling_sends_note_off(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = note(&state, PITCH_B, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_B, VELOCITY);

    // Nothing more while the gate is high
    for (unsigned int i = 0U; i < 5U; i++)
    {
        out = hold(&state, true, PLENTY);
        EXPECT_NOTHING(out);
    }

    out = hold(&state, false, PLENTY);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_B, 0U);
    out = hold(&state, false, PLENTY);
    EXPECT_NOTHING(out);

    // The next note is a note of its own, the same pitch or not
    out = note(&state, PITCH_B, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_B, VELOCITY);
    out = hold(&state, false, PLENTY);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_B, 0U);
}



static void test_the_lowest_pitch_is_note_24_and_the_pitch_is_limited(void)
{
    static const uint8_t semitones[] = { 0U, 1U, 12U, 44U, 45U, 46U, 100U, 255U };
    static const uint8_t notes[]     = { 24U, 25U, 36U, 68U, 69U, 69U, 69U, 69U };
    midi_out_state_t     state;
    midi_out_bytes_t     out;

    start_running(&state);
    for (size_t i = 0U; i < (sizeof(semitones) / sizeof(semitones[0])); i++)
    {
        out = note(&state, semitones[i], PLENTY);
        TEST_ASSERT_EQUAL(3, out.count);
        TEST_ASSERT_EQUAL(NOTE_ON, out.bytes[0]);
        TEST_ASSERT_EQUAL(notes[i], out.bytes[1]);
        TEST_ASSERT_EQUAL(VELOCITY, out.bytes[2]);

        out = hold(&state, false, PLENTY);
        TEST_ASSERT_EQUAL(3, out.count);
        TEST_ASSERT_EQUAL(NOTE_OFF, out.bytes[0]);
        TEST_ASSERT_EQUAL(notes[i], out.bytes[1]);
        TEST_ASSERT_EQUAL(0, out.bytes[2]);
    }
}



static void test_the_pitch_is_ignored_when_no_note_starts(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = tick(&state, true, false, PITCH_B, true, 0U, PLENTY);
    EXPECT_NOTHING(out);
    out = tick(&state, true, false, PITCH_B, false, 0U, PLENTY);
    EXPECT_NOTHING(out);
}



static void test_a_tick_sends_start_clocks_note_on_and_note_off_in_that_order(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    // A note starting with the pattern
    midi_out_init(&state);
    out = note(&state, PITCH_A, PLENTY);
    EXPECT_BYTES(out, START, CLOCK, NOTE_ON, NOTE_A, VELOCITY);

    // A tie to another pitch on a tick with a clock: all eight bytes
    out = tick(&state, true, true, PITCH_B, true, 8U, PLENTY);
    EXPECT_BYTES(out, CLOCK, CLOCK, NOTE_ON, NOTE_B, VELOCITY, NOTE_OFF, NOTE_A, 0U);
}



static void test_a_note_on_waits_for_room(void)
{
    // The bytes of a start with a note, and how many of them go on the first
    // tick for each amount of room: only whole messages
    static const uint8_t all[]   = { START, CLOCK, NOTE_ON, NOTE_A, VELOCITY };
    static const uint8_t first[] = { 0U, 1U, 2U, 2U, 2U, 5U, 5U, 5U, 5U, 5U };

    for (uint8_t room = 0U; room < (sizeof(first) / sizeof(first[0])); room++)
    {
        midi_out_state_t state;
        midi_out_bytes_t out;
        uint8_t          sent[2U * MIDI_OUT_MAX_BYTES];
        size_t           count;

        midi_out_init(&state);
        out = note(&state, PITCH_A, room);
        TEST_ASSERT_EQUAL(first[room], out.count);
        (void)memcpy(sent, out.bytes, out.count);
        count = out.count;

        // The rest follows on the next tick, the note still held
        out = hold(&state, true, PLENTY);
        (void)memcpy(&sent[count], out.bytes, out.count);
        count += out.count;

        TEST_ASSERT_EQUAL(sizeof(all), count);
        TEST_ASSERT(0 == memcmp(all, sent, sizeof(all)));

        out = hold(&state, true, PLENTY);
        EXPECT_NOTHING(out);
        out = hold(&state, false, PLENTY);
        EXPECT_BYTES(out, NOTE_OFF, NOTE_A, 0U);
    }
}



static void test_a_note_on_waits_behind_the_clocks(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;
    unsigned int     ticks = 0U;

    // A note starting on a tick with 9 clocks to send: 8 of them go, then
    // the last and the note
    start_running(&state);
    out = tick(&state, true, true, PITCH_A, true, 36U, PLENTY);
    EXPECT_BYTES(out, CLOCK, CLOCK, CLOCK, CLOCK, CLOCK, CLOCK, CLOCK, CLOCK);
    out = hold(&state, true, PLENTY);
    EXPECT_BYTES(out, CLOCK, NOTE_ON, NOTE_A, VELOCITY);

    // One clock and two bytes of room over: the note needs three
    out = tick(&state, true, true, PITCH_B, true, 4U, 3U);
    EXPECT_BYTES(out, CLOCK);
    do
    {
        out = hold(&state, true, 2U);
        EXPECT_NOTHING(out);
        ticks++;
    } while (ticks < 5U);
    out = hold(&state, true, 3U);
    EXPECT_BYTES(out, NOTE_ON, NOTE_B, VELOCITY);
    out = hold(&state, true, 3U);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_A, 0U);
}



static void test_a_note_on_still_waiting_when_its_gate_falls_is_given_up(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = note(&state, PITCH_A, 0U);
    EXPECT_NOTHING(out);
    out = hold(&state, true, 2U);
    EXPECT_NOTHING(out);

    // The gate falls: it never sounded, so no Note On and no Note Off
    out = hold(&state, false, PLENTY);
    EXPECT_NOTHING(out);
    out = hold(&state, true, PLENTY);
    EXPECT_NOTHING(out);
    out = hold(&state, false, PLENTY);
    EXPECT_NOTHING(out);
}



static void test_a_note_whose_gate_never_rose_is_not_sent(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    // The gate is low after the tick the note started on
    start_running(&state);
    out = tick(&state, true, true, PITCH_A, false, 0U, PLENTY);
    EXPECT_NOTHING(out);
    out = hold(&state, false, PLENTY);
    EXPECT_NOTHING(out);

    // With a note sounding, that ends it and starts no other
    out = note(&state, PITCH_A, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_A, VELOCITY);
    out = tick(&state, true, true, PITCH_B, false, 0U, PLENTY);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_A, 0U);
    out = hold(&state, true, PLENTY);
    EXPECT_NOTHING(out);
}



static void test_a_later_note_replaces_one_still_waiting(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = note(&state, PITCH_A, 0U);
    EXPECT_NOTHING(out);
    out = note(&state, PITCH_B, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_B, VELOCITY);
    out = hold(&state, true, PLENTY);
    EXPECT_NOTHING(out);
    out = hold(&state, false, PLENTY);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_B, 0U);
}



static void test_a_note_off_waits_for_room(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = note(&state, PITCH_C, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_C, VELOCITY);

    for (uint8_t room = 0U; room < 3U; room++)
    {
        out = hold(&state, false, room);
        EXPECT_NOTHING(out);
    }
    out = hold(&state, false, 3U);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_C, 0U);
    out = hold(&state, false, PLENTY);
    EXPECT_NOTHING(out);
}



static void test_a_note_off_still_owed_is_sent_though_the_gate_is_high_again(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = note(&state, PITCH_A, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_A, VELOCITY);
    out = hold(&state, false, 0U); // The gate falls with no room
    EXPECT_NOTHING(out);

    // The same pitch starts again. It is a new note and not a tie: the old
    // one is ended first, and the new Note On follows on the next tick
    out = note(&state, PITCH_A, 0U);
    EXPECT_NOTHING(out);
    out = hold(&state, true, PLENTY);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_A, 0U);
    out = hold(&state, true, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_A, VELOCITY);
    out = hold(&state, true, PLENTY);
    EXPECT_NOTHING(out);
    out = hold(&state, false, PLENTY);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_A, 0U);

    // A different pitch after a Note Off that waited: the same
    out = note(&state, PITCH_A, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_A, VELOCITY);
    out = hold(&state, false, 0U);
    EXPECT_NOTHING(out);
    out = note(&state, PITCH_B, PLENTY);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_A, 0U);
    out = hold(&state, true, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_B, VELOCITY);
    out = hold(&state, false, PLENTY);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_B, 0U);
}



static void test_a_tie_at_the_same_pitch_holds_the_note(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = note(&state, PITCH_B, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_B, VELOCITY);

    // Three more steps of the same pitch with the gate high throughout
    for (unsigned int i = 0U; i < 3U; i++)
    {
        out = hold(&state, true, PLENTY);
        EXPECT_NOTHING(out);
        out = note(&state, PITCH_B, PLENTY);
        EXPECT_NOTHING(out);
    }

    // One long note, ended once
    out = hold(&state, false, PLENTY);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_B, 0U);
    out = hold(&state, false, PLENTY);
    EXPECT_NOTHING(out);
}



static void test_a_tie_at_another_pitch_sends_the_new_note_on_then_the_old_note_off(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = note(&state, PITCH_A, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_A, VELOCITY);
    out = note(&state, PITCH_B, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_B, VELOCITY, NOTE_OFF, NOTE_A, 0U);
    out = note(&state, PITCH_C, 6U); // Exactly enough room
    EXPECT_BYTES(out, NOTE_ON, NOTE_C, VELOCITY, NOTE_OFF, NOTE_B, 0U);

    // Only the last is sounding
    out = hold(&state, false, PLENTY);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_C, 0U);
    out = hold(&state, false, PLENTY);
    EXPECT_NOTHING(out);
}



static void test_a_tie_with_room_for_the_note_on_only_sends_the_note_off_later(void)
{
    for (uint8_t room = 3U; room < 6U; room++)
    {
        midi_out_state_t state;
        midi_out_bytes_t out;

        start_running(&state);
        out = note(&state, PITCH_A, PLENTY);
        EXPECT_BYTES(out, NOTE_ON, NOTE_A, VELOCITY);

        out = note(&state, PITCH_B, room);
        EXPECT_BYTES(out, NOTE_ON, NOTE_B, VELOCITY);
        out = hold(&state, true, 2U);
        EXPECT_NOTHING(out);
        out = hold(&state, true, PLENTY);
        EXPECT_BYTES(out, NOTE_OFF, NOTE_A, 0U);
        out = hold(&state, true, PLENTY);
        EXPECT_NOTHING(out);

        out = hold(&state, false, PLENTY);
        EXPECT_BYTES(out, NOTE_OFF, NOTE_B, 0U);
    }
}



static void test_a_tie_with_no_room_sends_both_later(void)
{
    for (uint8_t room = 0U; room < 3U; room++)
    {
        midi_out_state_t state;
        midi_out_bytes_t out;

        start_running(&state);
        out = note(&state, PITCH_A, PLENTY);
        EXPECT_BYTES(out, NOTE_ON, NOTE_A, VELOCITY);

        out = note(&state, PITCH_B, room);
        EXPECT_NOTHING(out);
        out = hold(&state, true, PLENTY);
        EXPECT_BYTES(out, NOTE_ON, NOTE_B, VELOCITY, NOTE_OFF, NOTE_A, 0U);
        out = hold(&state, false, PLENTY);
        EXPECT_BYTES(out, NOTE_OFF, NOTE_B, 0U);
    }
}



static void test_a_tie_that_never_went_is_given_up_when_the_gate_falls(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = note(&state, PITCH_A, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_A, VELOCITY);
    out = note(&state, PITCH_B, 0U);
    EXPECT_NOTHING(out);

    // Only the note that sounded is ended
    out = hold(&state, false, PLENTY);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_A, 0U);
    out = hold(&state, false, PLENTY);
    EXPECT_NOTHING(out);
}



static void test_a_tie_back_to_the_sounding_pitch_drops_the_note_that_waited(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = note(&state, PITCH_A, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_A, VELOCITY);
    out = note(&state, PITCH_B, 0U); // Waits
    EXPECT_NOTHING(out);
    out = note(&state, PITCH_A, PLENTY); // And A is wanted again: still held
    EXPECT_NOTHING(out);
    out = hold(&state, true, PLENTY);
    EXPECT_NOTHING(out);
    out = hold(&state, false, PLENTY);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_A, 0U);
}



static void test_a_second_tie_waits_for_the_first_ones_note_off(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = note(&state, PITCH_A, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_A, VELOCITY);
    out = note(&state, PITCH_B, 3U); // A's Note Off does not fit
    EXPECT_BYTES(out, NOTE_ON, NOTE_B, VELOCITY);

    // A third pitch while A is still sounding: A is ended first, so that
    // never more than two notes sound
    out = note(&state, PITCH_C, 3U);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_A, 0U);
    out = hold(&state, true, 3U);
    EXPECT_BYTES(out, NOTE_ON, NOTE_C, VELOCITY);
    out = hold(&state, true, 3U);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_B, 0U);
    out = hold(&state, true, PLENTY);
    EXPECT_NOTHING(out);
    out = hold(&state, false, PLENTY);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_C, 0U);
}



static void test_two_notes_sounding_are_both_ended_when_the_gate_falls(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = note(&state, PITCH_A, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_A, VELOCITY);
    out = note(&state, PITCH_B, 3U);
    EXPECT_BYTES(out, NOTE_ON, NOTE_B, VELOCITY);

    // The gate falls with room for neither, then for one, then the other
    out = hold(&state, false, 2U);
    EXPECT_NOTHING(out);
    out = hold(&state, false, 5U);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_A, 0U);
    out = hold(&state, false, 3U);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_B, 0U);
    out = hold(&state, false, PLENTY);
    EXPECT_NOTHING(out);

    // And with room for both, the older first
    out = note(&state, PITCH_A, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_A, VELOCITY);
    out = note(&state, PITCH_B, 3U);
    EXPECT_BYTES(out, NOTE_ON, NOTE_B, VELOCITY);
    out = hold(&state, false, 6U);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_A, 0U, NOTE_OFF, NOTE_B, 0U);
}



static void test_a_stop_while_a_note_sounds_ends_the_note(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = note(&state, PITCH_A, PLENTY);
    EXPECT_BYTES(out, NOTE_ON, NOTE_A, VELOCITY);

    // The sequencer closes the gate on the tick it stops; a clock falls too
    out = stopped(&state, 4U, PLENTY);
    EXPECT_BYTES(out, STOP, CLOCK, NOTE_OFF, NOTE_A, 0U);
    out = stopped(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);

    // With room for the Stop only, the Note Off follows
    out = note(&state, PITCH_B, PLENTY);
    EXPECT_BYTES(out, START, CLOCK, NOTE_ON, NOTE_B, VELOCITY);
    out = stopped(&state, 0U, 1U);
    EXPECT_BYTES(out, STOP);
    out = stopped(&state, 0U, 0U);
    EXPECT_NOTHING(out);
    out = stopped(&state, 0U, 3U);
    EXPECT_BYTES(out, NOTE_OFF, NOTE_B, 0U);
    out = stopped(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);
}



int main(void)
{
    RUN_TEST(test_a_note_sends_note_on_and_its_gate_falling_sends_note_off);
    RUN_TEST(test_the_lowest_pitch_is_note_24_and_the_pitch_is_limited);
    RUN_TEST(test_the_pitch_is_ignored_when_no_note_starts);
    RUN_TEST(test_a_tick_sends_start_clocks_note_on_and_note_off_in_that_order);
    RUN_TEST(test_a_note_on_waits_for_room);
    RUN_TEST(test_a_note_on_waits_behind_the_clocks);
    RUN_TEST(test_a_note_on_still_waiting_when_its_gate_falls_is_given_up);
    RUN_TEST(test_a_note_whose_gate_never_rose_is_not_sent);
    RUN_TEST(test_a_later_note_replaces_one_still_waiting);
    RUN_TEST(test_a_note_off_waits_for_room);
    RUN_TEST(test_a_note_off_still_owed_is_sent_though_the_gate_is_high_again);
    RUN_TEST(test_a_tie_at_the_same_pitch_holds_the_note);
    RUN_TEST(test_a_tie_at_another_pitch_sends_the_new_note_on_then_the_old_note_off);
    RUN_TEST(test_a_tie_with_room_for_the_note_on_only_sends_the_note_off_later);
    RUN_TEST(test_a_tie_with_no_room_sends_both_later);
    RUN_TEST(test_a_tie_that_never_went_is_given_up_when_the_gate_falls);
    RUN_TEST(test_a_tie_back_to_the_sounding_pitch_drops_the_note_that_waited);
    RUN_TEST(test_a_second_tie_waits_for_the_first_ones_note_off);
    RUN_TEST(test_two_notes_sounding_are_both_ended_when_the_gate_falls);
    RUN_TEST(test_a_stop_while_a_note_sounds_ends_the_note);
    return TEST_RESULT();
}
