/**
 * @file   test_midi_out.c
 * @brief  Host unit tests for MIDI out (core/midi_out.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "midi_out.h"
#include "test_harness.h"

#define POISON_BYTE (0xA5)

#define NOTE_OFF    (0x80U)
#define NOTE_ON     (0x90U)
#define CLOCK       (0xF8U)
#define START       (0xFAU)
#define STOP        (0xFCU)
#define VELOCITY    (100U)
#define LOWEST_NOTE (24U)
#define PLENTY      (255U) // Room for anything a tick can produce

// Semitones used as pitches, and the note numbers they come out as
#define PITCH_A     (0U)
#define PITCH_B     (7U)
#define PITCH_C     (45U)
#define NOTE_A      (LOWEST_NOTE + PITCH_A)
#define NOTE_B      (LOWEST_NOTE + PITCH_B)
#define NOTE_C      (LOWEST_NOTE + PITCH_C)

// Checks that a tick produced exactly these bytes, in this order
#define EXPECT_BYTES(out, ...)                                              \
    do                                                                      \
    {                                                                       \
        static const uint8_t expected_[] = { __VA_ARGS__ };                 \
        expect_bytes(__LINE__, &(out), expected_, sizeof(expected_));       \
    } while (0)

// Checks that a tick produced nothing
#define EXPECT_NOTHING(out) TEST_ASSERT_EQUAL(0, (out).count)

// Which clock the ticks below are given as running from. Each test that
// sets it puts it back
static bool g_b_external = false;



static void expect_bytes(int line, const midi_out_bytes_t *p_out, const uint8_t *p_expected,
                         size_t count)
{
    const bool b_same = (count == p_out->count) && (0 == memcmp(p_expected, p_out->bytes, count));

    if (!b_same)
    {
        (void)printf("  at line %d: got %u bytes:", line, (unsigned int)p_out->count);
        for (size_t i = 0U; (i < p_out->count) && (i < MIDI_OUT_MAX_BYTES); i++)
        {
            (void)printf(" %02X", (unsigned int)p_out->bytes[i]);
        }
        (void)printf("\n");
    }
    TEST_ASSERT(b_same);
}



/**
 * @brief  Runs one tick with everything given. The bytes start out poisoned,
 *         so a count that was not written shows.
 */
static midi_out_bytes_t tick(midi_out_state_t *p_state, bool b_running, bool b_note_started,
                             uint8_t semitone, bool b_gate, uint8_t pulses, uint8_t room)
{
    midi_out_inputs_t in;
    midi_out_bytes_t  out;

    in.b_running      = b_running;
    in.b_external     = g_b_external;
    in.b_note_started = b_note_started;
    in.semitone       = semitone;
    in.b_gate         = b_gate;
    in.pulses         = pulses;
    in.room           = room;

    (void)memset(&out, POISON_BYTE, sizeof(out));
    midi_out_tick(p_state, &in, &out);
    TEST_ASSERT(out.count <= MIDI_OUT_MAX_BYTES);
    TEST_ASSERT(out.count <= room);
    return out;
}



// A running tick on which a note starts and its gate is high
static midi_out_bytes_t note(midi_out_state_t *p_state, uint8_t semitone, uint8_t room)
{
    return tick(p_state, true, true, semitone, true, 0U, room);
}



// A running tick on which nothing starts and no pulse falls
static midi_out_bytes_t hold(midi_out_state_t *p_state, bool b_gate, uint8_t room)
{
    return tick(p_state, true, false, 0U, b_gate, 0U, room);
}



// A running tick on which only pulses fall, with the gate low
static midi_out_bytes_t pulse(midi_out_state_t *p_state, uint8_t pulses, uint8_t room)
{
    return tick(p_state, true, false, 0U, false, pulses, room);
}



// A stopped tick, with the gate low as the sequencer gives it
static midi_out_bytes_t stopped(midi_out_state_t *p_state, uint8_t pulses, uint8_t room)
{
    return tick(p_state, false, false, 0U, false, pulses, room);
}



// Initializes the state and takes it past the Start and its clock
static void start_running(midi_out_state_t *p_state)
{
    midi_out_bytes_t out;

    midi_out_init(p_state);
    out = pulse(p_state, 0U, PLENTY);
    EXPECT_BYTES(out, START, CLOCK);
}



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



static void test_null_pointers_are_ignored(void)
{
    midi_out_state_t        state;
    midi_out_state_t        before;
    midi_out_bytes_t        out;
    midi_out_bytes_t        poisoned;
    const midi_out_inputs_t in = { true, false, true, PITCH_A, true, 4U, PLENTY };

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
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
