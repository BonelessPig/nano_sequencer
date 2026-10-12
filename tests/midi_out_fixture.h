#ifndef MIDI_OUT_FIXTURE_H
#define MIDI_OUT_FIXTURE_H
/**
 * @file   midi_out_fixture.h
 * @brief  What the test_midi_out programs share: the bytes they look for and
 *         the helpers that run a tick of MIDI out.
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



static inline void expect_bytes(int line, const midi_out_bytes_t *p_out, const uint8_t *p_expected,
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
static inline midi_out_bytes_t tick(midi_out_state_t *p_state, bool b_running, bool b_note_started,
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
static inline midi_out_bytes_t note(midi_out_state_t *p_state, uint8_t semitone, uint8_t room)
{
    return tick(p_state, true, true, semitone, true, 0U, room);
}



// A running tick on which nothing starts and no pulse falls
static inline midi_out_bytes_t hold(midi_out_state_t *p_state, bool b_gate, uint8_t room)
{
    return tick(p_state, true, false, 0U, b_gate, 0U, room);
}



// A running tick on which only pulses fall, with the gate low
static inline midi_out_bytes_t pulse(midi_out_state_t *p_state, uint8_t pulses, uint8_t room)
{
    return tick(p_state, true, false, 0U, false, pulses, room);
}



// A stopped tick, with the gate low as the sequencer gives it
static inline midi_out_bytes_t stopped(midi_out_state_t *p_state, uint8_t pulses, uint8_t room)
{
    return tick(p_state, false, false, 0U, false, pulses, room);
}



// Initializes the state and takes it past the Start and its clock
static inline void start_running(midi_out_state_t *p_state)
{
    midi_out_bytes_t out;

    midi_out_init(p_state);
    out = pulse(p_state, 0U, PLENTY);
    EXPECT_BYTES(out, START, CLOCK);
}

#endif // MIDI_OUT_FIXTURE_H
