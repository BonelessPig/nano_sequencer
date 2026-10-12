/**
 * @file   test_midi_out.c
 * @brief  Host unit tests for MIDI out (core/midi_out.c): Start, Stop, the
 *         clock and null pointers.
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

// Counts how many of a tick's bytes are a given one
static unsigned int count_of(const midi_out_bytes_t *p_out, uint8_t byte)
{
    unsigned int found = 0U;

    for (size_t i = 0U; i < p_out->count; i++)
    {
        if (byte == p_out->bytes[i])
        {
            found++;
        }
    }
    return found;
}



static void test_init_is_stopped_with_nothing_owed(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    midi_out_init(&state);
    TEST_ASSERT(!state.b_running);
    TEST_ASSERT_EQUAL(0, state.pulse_count);
    TEST_ASSERT_EQUAL(0, state.clocks_owed);

    // Stopped, gate low, no pulses: nothing to say, however long it lasts
    for (unsigned int i = 0U; i < 10U; i++)
    {
        out = stopped(&state, 0U, PLENTY);
        EXPECT_NOTHING(out);
    }
}



static void test_the_first_running_tick_sends_start_and_a_clock(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    midi_out_init(&state);
    out = pulse(&state, 0U, PLENTY);
    EXPECT_BYTES(out, START, CLOCK);

    // Only the once
    out = pulse(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);
}



static void test_a_clock_is_sent_for_every_four_pulses(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    for (unsigned int i = 1U; i <= 40U; i++)
    {
        out = pulse(&state, 1U, PLENTY);
        if (0U == (i % 4U))
        {
            EXPECT_BYTES(out, CLOCK);
        }
        else
        {
            EXPECT_NOTHING(out);
        }
    }

    // Several at once: 9 pulses is two clocks and one pulse over
    out = pulse(&state, 9U, PLENTY);
    EXPECT_BYTES(out, CLOCK, CLOCK);
    out = pulse(&state, 3U, PLENTY);
    EXPECT_BYTES(out, CLOCK);
}



static void test_pulses_on_the_starting_tick_count_towards_the_next_clock(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    // Three with the start: the downbeat only, and the fourth brings a clock
    midi_out_init(&state);
    out = pulse(&state, 3U, PLENTY);
    EXPECT_BYTES(out, START, CLOCK);
    out = pulse(&state, 1U, PLENTY);
    EXPECT_BYTES(out, CLOCK);

    // Four with the start: the downbeat and the clock they make
    midi_out_init(&state);
    out = pulse(&state, 4U, PLENTY);
    EXPECT_BYTES(out, START, CLOCK, CLOCK);
}



static void test_the_clock_carries_on_while_stopped_if_pulses_come(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    // Never started: clocks and no Start
    midi_out_init(&state);
    out = stopped(&state, 3U, PLENTY);
    EXPECT_NOTHING(out);
    out = stopped(&state, 1U, PLENTY);
    EXPECT_BYTES(out, CLOCK);
    out = stopped(&state, 8U, PLENTY);
    EXPECT_BYTES(out, CLOCK, CLOCK);
}



static void test_a_stop_sends_stop_and_a_start_sends_start_again(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = pulse(&state, 2U, PLENTY);
    EXPECT_NOTHING(out);

    out = stopped(&state, 0U, PLENTY);
    EXPECT_BYTES(out, STOP);
    out = stopped(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);

    // The two pulses before the stop still count while stopped
    out = stopped(&state, 2U, PLENTY);
    EXPECT_BYTES(out, CLOCK);

    out = pulse(&state, 0U, PLENTY);
    EXPECT_BYTES(out, START, CLOCK);
    out = pulse(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);
}



static void test_a_start_begins_the_count_of_pulses_again(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    // Three pulses towards a clock while stopped
    midi_out_init(&state);
    out = stopped(&state, 3U, PLENTY);
    EXPECT_NOTHING(out);

    // The start forgets them: the next clock is four pulses after it
    out = pulse(&state, 0U, PLENTY);
    EXPECT_BYTES(out, START, CLOCK);
    out = pulse(&state, 3U, PLENTY);
    EXPECT_NOTHING(out);
    out = pulse(&state, 1U, PLENTY);
    EXPECT_BYTES(out, CLOCK);
}



static void test_an_external_start_waits_for_the_first_pulse_for_its_downbeat(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    // The Start alone, and nothing until the first edge brings a pulse
    g_b_external = true;
    midi_out_init(&state);
    out = pulse(&state, 0U, PLENTY);
    EXPECT_BYTES(out, START);
    for (unsigned int i = 0U; i < 50U; i++)
    {
        out = pulse(&state, 0U, PLENTY);
        EXPECT_NOTHING(out);
    }

    // The pulse that begins step 0 is the downbeat, and every step of 24
    // pulses after it has 6 clocks, the first on the step's first pulse
    for (unsigned int i = 0U; i < (3U * 24U); i++)
    {
        out = pulse(&state, 1U, PLENTY);
        if (0U == (i % 4U))
        {
            EXPECT_BYTES(out, CLOCK);
        }
        else
        {
            EXPECT_NOTHING(out);
        }
    }

    // After a stop, the next external start does the same
    out = stopped(&state, 0U, PLENTY);
    EXPECT_BYTES(out, STOP);
    out = pulse(&state, 0U, PLENTY);
    EXPECT_BYTES(out, START);
    out = pulse(&state, 1U, PLENTY);
    EXPECT_BYTES(out, CLOCK);
    out = pulse(&state, 3U, PLENTY);
    EXPECT_NOTHING(out);
    out = pulse(&state, 1U, PLENTY);
    EXPECT_BYTES(out, CLOCK);
    g_b_external = false;
}



static void test_an_external_start_with_a_pulse_sends_its_downbeat_at_once(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    // The edge lands on the starting tick
    g_b_external = true;
    midi_out_init(&state);
    out = pulse(&state, 1U, PLENTY);
    EXPECT_BYTES(out, START, CLOCK);
    out = pulse(&state, 3U, PLENTY);
    EXPECT_NOTHING(out);
    out = pulse(&state, 1U, PLENTY);
    EXPECT_BYTES(out, CLOCK);

    // Five pulses with the start: the downbeat and the clock after it
    midi_out_init(&state);
    out = pulse(&state, 5U, PLENTY);
    EXPECT_BYTES(out, START, CLOCK, CLOCK);
    g_b_external = false;
}



static void test_an_external_start_gives_up_what_was_counted_before_it(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    // Ten clocks owed and two pulses over, while stopped
    g_b_external = true;
    midi_out_init(&state);
    out = stopped(&state, 42U, 0U);
    EXPECT_NOTHING(out);

    out = pulse(&state, 0U, PLENTY);
    EXPECT_BYTES(out, START);
    out = pulse(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);
    out = pulse(&state, 1U, PLENTY);
    EXPECT_BYTES(out, CLOCK);

    // A Start with no room: the first pulse still makes the downbeat, which
    // waits behind it
    midi_out_init(&state);
    out = pulse(&state, 0U, 0U);
    EXPECT_NOTHING(out);
    out = pulse(&state, 1U, 0U);
    EXPECT_NOTHING(out);
    out = pulse(&state, 0U, 1U);
    EXPECT_BYTES(out, START);
    out = pulse(&state, 0U, PLENTY);
    EXPECT_BYTES(out, CLOCK);
    g_b_external = false;
}



static void test_a_change_of_clock_while_running_changes_nothing(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    // Started on the internal clock, two pulses towards the next clock
    start_running(&state);
    out = pulse(&state, 2U, PLENTY);
    EXPECT_NOTHING(out);

    // To the external clock: no Start, no Stop, and the count carries on
    g_b_external = true;
    out = pulse(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);
    out = pulse(&state, 1U, PLENTY);
    EXPECT_NOTHING(out);
    out = pulse(&state, 1U, PLENTY);
    EXPECT_BYTES(out, CLOCK);

    // And back again, part of the way to the next
    out = pulse(&state, 3U, PLENTY);
    EXPECT_NOTHING(out);
    g_b_external = false;
    out = pulse(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);
    out = pulse(&state, 1U, PLENTY);
    EXPECT_BYTES(out, CLOCK);

    // Nor does it matter which clock a stopped pattern is on
    out = stopped(&state, 0U, PLENTY);
    EXPECT_BYTES(out, STOP);
    g_b_external = true;
    out = stopped(&state, 4U, PLENTY);
    EXPECT_BYTES(out, CLOCK);
    g_b_external = false;
    out = stopped(&state, 4U, PLENTY);
    EXPECT_BYTES(out, CLOCK);
}



static void test_clocks_that_do_not_fit_are_owed(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);

    // Five clocks' worth of pulses and room for two
    out = pulse(&state, 20U, 2U);
    EXPECT_BYTES(out, CLOCK, CLOCK);
    out = pulse(&state, 0U, 1U);
    EXPECT_BYTES(out, CLOCK);
    out = pulse(&state, 0U, 0U);
    EXPECT_NOTHING(out);
    out = pulse(&state, 0U, PLENTY);
    EXPECT_BYTES(out, CLOCK, CLOCK);
    out = pulse(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);
}



static void test_the_clock_count_is_exact_over_a_long_run(void)
{
    // 25 pulses in every 11 ticks, and room that is often short
    static const uint8_t chunks[] = { 0U, 1U, 0U, 2U, 3U, 0U, 1U, 5U, 0U, 0U, 13U };
    static const uint8_t rooms[]  = { 8U, 0U, 1U, 0U, 2U, 255U, 0U, 3U };
    midi_out_state_t     state;
    midi_out_bytes_t     out;
    unsigned int         pulses = 0U;
    unsigned int         clocks = 0U;

    midi_out_init(&state);
    for (unsigned int i = 0U; i < 440U; i++)
    {
        const uint8_t chunk = chunks[i % (sizeof(chunks) / sizeof(chunks[0]))];
        const uint8_t room  = rooms[i % (sizeof(rooms) / sizeof(rooms[0]))];

        out = pulse(&state, chunk, room);
        pulses += chunk;
        clocks += count_of(&out, CLOCK);

        // Nothing but clocks after the Start, which leads the first tick
        TEST_ASSERT_EQUAL((0U == i) ? 1U : 0U, count_of(&out, START));
        TEST_ASSERT_EQUAL(out.count, count_of(&out, CLOCK) + count_of(&out, START));
    }
    TEST_ASSERT_EQUAL(1000, pulses);

    // Whatever is still owed comes out once there is room
    out = pulse(&state, 0U, PLENTY);
    clocks += count_of(&out, CLOCK);
    out = pulse(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);

    // The downbeat and one for every four pulses since
    TEST_ASSERT_EQUAL(1U + (1000U / 4U), clocks);

    // Three more make none, and the fourth makes one
    out = pulse(&state, 3U, PLENTY);
    EXPECT_NOTHING(out);
    out = pulse(&state, 1U, PLENTY);
    EXPECT_BYTES(out, CLOCK);
}



static void test_a_burst_of_pulses_is_spread_over_ticks(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;
    unsigned int     clocks = 0U;
    unsigned int     ticks  = 0U;

    // 255 pulses with the start: the downbeat and 63 clocks, 8 bytes a tick
    midi_out_init(&state);
    out = pulse(&state, 255U, PLENTY);
    EXPECT_BYTES(out, START, CLOCK, CLOCK, CLOCK, CLOCK, CLOCK, CLOCK, CLOCK);
    clocks += count_of(&out, CLOCK);

    do
    {
        out = pulse(&state, 0U, PLENTY);
        TEST_ASSERT_EQUAL(out.count, count_of(&out, CLOCK));
        clocks += out.count;
        ticks++;
    } while ((out.count > 0U) && (ticks < 100U));

    TEST_ASSERT_EQUAL(64, clocks);
    TEST_ASSERT_EQUAL(9, ticks); // 7 full ticks, one with the last, one empty

    // The three pulses left over are still counted
    out = pulse(&state, 1U, PLENTY);
    EXPECT_BYTES(out, CLOCK);
}



static void test_the_clocks_owed_stop_at_255(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;
    unsigned int     clocks = 0U;
    unsigned int     ticks  = 0U;

    // Stopped, so no downbeat. Four ticks of 255 pulses are 1020 pulses,
    // which is exactly 255 clocks: the most that can be owed, and not yet
    // more than that
    midi_out_init(&state);
    for (unsigned int i = 0U; i < 4U; i++)
    {
        out = stopped(&state, 255U, 0U);
        EXPECT_NOTHING(out);
    }
    TEST_ASSERT_EQUAL(255, state.clocks_owed);

    // More are lost, and the count does not wrap round to a small one
    for (unsigned int i = 0U; i < 5U; i++)
    {
        out = stopped(&state, 255U, 0U);
        EXPECT_NOTHING(out);
        TEST_ASSERT_EQUAL(255, state.clocks_owed);
    }
    out = stopped(&state, 4U, 0U);
    TEST_ASSERT_EQUAL(255, state.clocks_owed);

    do
    {
        out = stopped(&state, 0U, PLENTY);
        TEST_ASSERT_EQUAL(out.count, count_of(&out, CLOCK));
        clocks += out.count;
        ticks++;
    } while ((out.count > 0U) && (ticks < 100U));

    TEST_ASSERT_EQUAL(255, clocks);
}



static void test_a_start_with_no_room_waits_and_its_clocks_wait_behind_it(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    midi_out_init(&state);
    out = pulse(&state, 0U, 0U);
    EXPECT_NOTHING(out);

    // Pulses after the start are counted from it, though it has not gone
    out = pulse(&state, 4U, 0U);
    EXPECT_NOTHING(out);

    // Room for one byte: the Start, and never a clock ahead of it
    out = pulse(&state, 0U, 1U);
    EXPECT_BYTES(out, START);
    out = pulse(&state, 0U, 1U);
    EXPECT_BYTES(out, CLOCK); // The downbeat
    out = pulse(&state, 0U, PLENTY);
    EXPECT_BYTES(out, CLOCK); // The four pulses
    out = pulse(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);
}



static void test_a_start_gives_up_the_clocks_owed_from_before_it(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    // Ten clocks owed while stopped
    midi_out_init(&state);
    out = stopped(&state, 40U, 0U);
    EXPECT_NOTHING(out);

    // The first clock after the Start is its downbeat, and the only one
    out = pulse(&state, 0U, PLENTY);
    EXPECT_BYTES(out, START, CLOCK);
    out = pulse(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);
}



static void test_a_stop_with_no_room_waits(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = stopped(&state, 0U, 0U);
    EXPECT_NOTHING(out);
    out = stopped(&state, 8U, 0U);
    EXPECT_NOTHING(out);

    // The Stop, then the clocks that came after it
    out = stopped(&state, 0U, 2U);
    EXPECT_BYTES(out, STOP, CLOCK);
    out = stopped(&state, 0U, PLENTY);
    EXPECT_BYTES(out, CLOCK);
    out = stopped(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);
}



static void test_a_stop_and_a_start_that_both_waited_are_both_sent(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    start_running(&state);
    out = stopped(&state, 0U, 0U);
    EXPECT_NOTHING(out);
    out = pulse(&state, 0U, 0U); // Running again, the Stop still owed
    EXPECT_NOTHING(out);

    // One byte of room takes the Stop; the Start does not pass it
    out = pulse(&state, 0U, 1U);
    EXPECT_BYTES(out, STOP);
    out = pulse(&state, 0U, PLENTY);
    EXPECT_BYTES(out, START, CLOCK);
    out = pulse(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);

    // And with room for both, they go together in that order
    out = stopped(&state, 0U, 0U);
    EXPECT_NOTHING(out);
    out = pulse(&state, 0U, 0U);
    EXPECT_NOTHING(out);
    out = pulse(&state, 0U, PLENTY);
    EXPECT_BYTES(out, STOP, START, CLOCK);
}



static void test_a_start_that_never_went_needs_no_stop(void)
{
    midi_out_state_t state;
    midi_out_bytes_t out;

    midi_out_init(&state);
    out = pulse(&state, 0U, 0U); // A start, with no room for its Start
    EXPECT_NOTHING(out);

    // Stopped again before it went: neither is sent. The clock the start
    // owed goes as one of the clocks of a stopped pattern
    out = stopped(&state, 0U, PLENTY);
    EXPECT_BYTES(out, CLOCK);
    out = stopped(&state, 0U, PLENTY);
    EXPECT_NOTHING(out);

    // The same after a Stop that did go
    out = pulse(&state, 0U, PLENTY);
    EXPECT_BYTES(out, START, CLOCK);
    out = stopped(&state, 0U, PLENTY);
    EXPECT_BYTES(out, STOP);
    out = pulse(&state, 0U, 0U);
    EXPECT_NOTHING(out);
    out = stopped(&state, 0U, 0U);
    EXPECT_NOTHING(out);
    out = stopped(&state, 0U, PLENTY);
    EXPECT_BYTES(out, CLOCK);
}



static void test_null_pointers_are_ignored(void)
{
    midi_out_state_t        state;
    midi_out_state_t        before;
    midi_out_bytes_t        out;
    midi_out_bytes_t        poisoned;
    const midi_out_inputs_t in = { true, false, true, PITCH_A, 255U, true, 4U, PLENTY };

    midi_out_init(NULL); // Must not crash

    midi_out_init(&state);
    (void)memcpy(&before, &state, sizeof(before));
    (void)memset(&out, POISON_BYTE, sizeof(out));
    (void)memset(&poisoned, POISON_BYTE, sizeof(poisoned));

    midi_out_tick(NULL, &in, &out);
    TEST_ASSERT(0 == memcmp(&poisoned, &out, sizeof(out)));

    midi_out_tick(&state, NULL, &out);
    TEST_ASSERT(0 == memcmp(&poisoned, &out, sizeof(out)));
    TEST_ASSERT(0 == memcmp(&before, &state, sizeof(state)));

    midi_out_tick(&state, &in, NULL);
    TEST_ASSERT(0 == memcmp(&before, &state, sizeof(state)));

    // Nothing was taken in by those: the tick is still the first
    midi_out_tick(&state, &in, &out);
    EXPECT_BYTES(out, START, CLOCK, CLOCK, NOTE_ON, NOTE_A, VELOCITY);
}



int main(void)
{
    RUN_TEST(test_init_is_stopped_with_nothing_owed);
    RUN_TEST(test_the_first_running_tick_sends_start_and_a_clock);
    RUN_TEST(test_a_clock_is_sent_for_every_four_pulses);
    RUN_TEST(test_pulses_on_the_starting_tick_count_towards_the_next_clock);
    RUN_TEST(test_the_clock_carries_on_while_stopped_if_pulses_come);
    RUN_TEST(test_a_stop_sends_stop_and_a_start_sends_start_again);
    RUN_TEST(test_a_start_begins_the_count_of_pulses_again);
    RUN_TEST(test_an_external_start_waits_for_the_first_pulse_for_its_downbeat);
    RUN_TEST(test_an_external_start_with_a_pulse_sends_its_downbeat_at_once);
    RUN_TEST(test_an_external_start_gives_up_what_was_counted_before_it);
    RUN_TEST(test_a_change_of_clock_while_running_changes_nothing);
    RUN_TEST(test_clocks_that_do_not_fit_are_owed);
    RUN_TEST(test_the_clock_count_is_exact_over_a_long_run);
    RUN_TEST(test_a_burst_of_pulses_is_spread_over_ticks);
    RUN_TEST(test_the_clocks_owed_stop_at_255);
    RUN_TEST(test_a_start_with_no_room_waits_and_its_clocks_wait_behind_it);
    RUN_TEST(test_a_start_gives_up_the_clocks_owed_from_before_it);
    RUN_TEST(test_a_stop_with_no_room_waits);
    RUN_TEST(test_a_stop_and_a_start_that_both_waited_are_both_sent);
    RUN_TEST(test_a_start_that_never_went_needs_no_stop);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
