/**
 * @file   midi_out.c
 * @brief  MIDI out implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "midi_out.h"
#include <stddef.h>

// Status bytes. The notes are on channel 1, which is 0 in the low four bits
#define STATUS_NOTE_OFF   (0x80U)
#define STATUS_NOTE_ON    (0x90U)
#define STATUS_CLOCK      (0xF8U)
#define STATUS_START      (0xFAU)
#define STATUS_STOP       (0xFCU)

#define REALTIME_BYTES    (1U) // A clock, a Start or a Stop
#define NOTE_BYTES        (3U) // A Note On or a Note Off

#define LOWEST_NOTE       (24U) // Semitone 0 is C1
#define SEMITONE_MAX      (45U)
#define NOTE_NONE         (0xFFU) // No note; not a valid note number
#define NOTE_ON_VELOCITY  (100U)
#define NOTE_OFF_VELOCITY (0U)

#define PULSES_PER_CLOCK  (4U) // 96 pulses to the quarter note, 24 clocks
#define CLOCKS_OWED_MAX   (255U)
#define DOWNBEAT_CLOCKS   (1U) // Clocks owed by a start on the internal clock

// On the external clock the first step waits for the first pulse, so the
// count begins one pulse short of a clock and that pulse makes the downbeat
#define DOWNBEAT_PULSES   (PULSES_PER_CLOCK - 1U)



/**
 * @brief  Gives the MIDI note number of a pitch.
 * @param  semitone  Semitones above the lowest pitch; above SEMITONE_MAX is
 *                   treated as SEMITONE_MAX.
 * @return The note number, LOWEST_NOTE to LOWEST_NOTE + SEMITONE_MAX.
 */
static uint8_t note_number(uint8_t semitone)
{
    const uint8_t limited = (semitone > SEMITONE_MAX) ? (uint8_t)SEMITONE_MAX : semitone;

    // At most 24 + 45 = 69, so it fits in a byte
    return (uint8_t)(LOWEST_NOTE + limited);
}



/**
 * @brief  Tells whether a message can still be added to this tick's bytes.
 * @param  p_out   The bytes so far; count is no more than limit.
 * @param  limit   The most bytes this tick may produce.
 * @param  length  Bytes in the message.
 * @return true if all of it fits.
 */
static bool fits(const midi_out_bytes_t *p_out, uint8_t limit, uint8_t length)
{
    return ((uint8_t)(limit - p_out->count) >= length);
}



/**
 * @brief  Adds one byte to this tick's bytes. The caller has checked that it
 *         fits.
 * @param  p_out  The bytes so far; one is added.
 * @param  byte   The byte.
 */
static void put_byte(midi_out_bytes_t *p_out, uint8_t byte)
{
    p_out->bytes[p_out->count] = byte;
    p_out->count++;
}



/**
 * @brief  Adds a Note On or a Note Off to this tick's bytes. The caller has
 *         checked that it fits.
 * @param  p_out     The bytes so far; NOTE_BYTES are added.
 * @param  status    STATUS_NOTE_ON or STATUS_NOTE_OFF.
 * @param  note      The note number.
 * @param  velocity  Its velocity, 0 to 127.
 */
static void put_note(midi_out_bytes_t *p_out, uint8_t status, uint8_t note, uint8_t velocity)
{
    put_byte(p_out, status);
    put_byte(p_out, note);
    put_byte(p_out, velocity);
}



/**
 * @brief  Notes a start or a stop of the pattern as owed. A start also
 *         begins the count of pulses again, giving up the clocks owed from
 *         before it: on the internal clock it owes its downbeat clock at
 *         once, and on the external clock the first pulse will make it.
 * @param  p_state     MIDI out state; updated.
 * @param  b_running   true if the pattern is running on this tick.
 * @param  b_external  true if it runs from the external clock; only used at
 *                     a start.
 */
static void mark_transport(midi_out_state_t *p_state, bool b_running, bool b_external)
{
    if (b_running != p_state->b_running)
    {
        if (b_running)
        {
            p_state->b_start_owed = true;
            p_state->pulse_count  = b_external ? (uint8_t)DOWNBEAT_PULSES : (uint8_t)0U;
            p_state->clocks_owed  = b_external ? (uint8_t)0U : (uint8_t)DOWNBEAT_CLOCKS;
        }
        else if (p_state->b_start_owed)
        {
            // The receiver never heard of the start, so it needs no Stop
            p_state->b_start_owed = false;
        }
        else
        {
            p_state->b_stop_owed = true;
        }
        p_state->b_running = b_running;
    }
}



/**
 * @brief  Counts this tick's pulses into clocks owed.
 * @param  p_state  MIDI out state; updated.
 * @param  pulses   Clock pulses that fell on this tick.
 */
static void count_pulses(midi_out_state_t *p_state, uint8_t pulses)
{
    uint16_t total = pulses;
    uint16_t owed  = p_state->clocks_owed;

    // At most 255 + 3 = 258 pulses, which is 64 clocks, and with the 255
    // that can be owed already that is 319
    total = (uint16_t)(total + p_state->pulse_count);
    owed  = (uint16_t)(owed + (uint16_t)(total / PULSES_PER_CLOCK));

    if (owed > CLOCKS_OWED_MAX)
    {
        owed = CLOCKS_OWED_MAX;
    }

    p_state->pulse_count = (uint8_t)(total % PULSES_PER_CLOCK);
    p_state->clocks_owed = (uint8_t)owed;
}



/**
 * @brief  Notes what this tick did to the notes: one to start, or the one
 *         sounding to end.
 * @param  p_state  MIDI out state; updated.
 * @param  p_in     Inputs for this tick.
 */
static void mark_note(midi_out_state_t *p_state, const midi_out_inputs_t *p_in)
{
    if (p_in->b_note_started)
    {
        const uint8_t note = note_number(p_in->semitone);

        // The pitch already sounding, and still wanted, is left held; any
        // other takes the place of a note that was still waiting
        const bool b_held = (note == p_state->note_on) && !p_state->b_release;

        p_state->note_wait = b_held ? (uint8_t)NOTE_NONE : note;
    }

    if (!p_in->b_gate)
    {
        // A note still waiting never sounded, so it is given up
        p_state->note_wait = NOTE_NONE;
        p_state->b_release = (NOTE_NONE != p_state->note_on);
    }
}



/**
 * @brief  Sends the Stop and the Start that are owed, in that order.
 * @param  p_state  MIDI out state; updated.
 * @param  p_out    This tick's bytes; added to.
 * @param  limit    The most bytes this tick may produce.
 */
static void send_transport(midi_out_state_t *p_state, midi_out_bytes_t *p_out, uint8_t limit)
{
    if (p_state->b_stop_owed && fits(p_out, limit, REALTIME_BYTES))
    {
        put_byte(p_out, STATUS_STOP);
        p_state->b_stop_owed = false;
    }

    // A Stop that did not fit left no room, so a Start cannot pass it
    if (p_state->b_start_owed && fits(p_out, limit, REALTIME_BYTES))
    {
        put_byte(p_out, STATUS_START);
        p_state->b_start_owed = false;
    }
}



/**
 * @brief  Sends as many of the clocks owed as fit. None fits while a Start
 *         or a Stop is still owed, as that left no room.
 * @param  p_state  MIDI out state; updated.
 * @param  p_out    This tick's bytes; added to.
 * @param  limit    The most bytes this tick may produce.
 */
static void send_clocks(midi_out_state_t *p_state, midi_out_bytes_t *p_out, uint8_t limit)
{
    while ((p_state->clocks_owed > 0U) && fits(p_out, limit, REALTIME_BYTES))
    {
        put_byte(p_out, STATUS_CLOCK);
        p_state->clocks_owed--;
    }
}



/**
 * @brief  Sends the Note On that is waiting, if it fits and no Note Off is
 *         owed from an earlier tick. The note it takes over from, if any, is
 *         then owed its Note Off.
 * @param  p_state  MIDI out state; updated.
 * @param  p_out    This tick's bytes; added to.
 * @param  limit    The most bytes this tick may produce.
 */
static void send_note_on(midi_out_state_t *p_state, midi_out_bytes_t *p_out, uint8_t limit)
{
    const bool b_waiting = (NOTE_NONE != p_state->note_wait);
    const bool b_clear   = (NOTE_NONE == p_state->note_off) && !p_state->b_release;

    if (b_waiting && b_clear && fits(p_out, limit, NOTE_BYTES))
    {
        put_note(p_out, STATUS_NOTE_ON, p_state->note_wait, NOTE_ON_VELOCITY);
        p_state->note_off  = p_state->note_on;
        p_state->note_on   = p_state->note_wait;
        p_state->note_wait = NOTE_NONE;
    }
}



/**
 * @brief  Sends the Note Offs that are owed and fit: the note a tie took
 *         over from, then the note sounding if its gate has fallen.
 * @param  p_state  MIDI out state; updated.
 * @param  p_out    This tick's bytes; added to.
 * @param  limit    The most bytes this tick may produce.
 */
static void send_note_offs(midi_out_state_t *p_state, midi_out_bytes_t *p_out, uint8_t limit)
{
    if ((NOTE_NONE != p_state->note_off) && fits(p_out, limit, NOTE_BYTES))
    {
        put_note(p_out, STATUS_NOTE_OFF, p_state->note_off, NOTE_OFF_VELOCITY);
        p_state->note_off = NOTE_NONE;
    }

    if (p_state->b_release && fits(p_out, limit, NOTE_BYTES))
    {
        put_note(p_out, STATUS_NOTE_OFF, p_state->note_on, NOTE_OFF_VELOCITY);
        p_state->note_on   = NOTE_NONE;
        p_state->b_release = false;
    }
}



void midi_out_init(midi_out_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->b_running    = false;
        p_state->b_stop_owed  = false;
        p_state->b_start_owed = false;
        p_state->b_release    = false;
        p_state->pulse_count  = 0U;
        p_state->clocks_owed  = 0U;
        p_state->note_on      = NOTE_NONE;
        p_state->note_off     = NOTE_NONE;
        p_state->note_wait    = NOTE_NONE;
    }
}



void midi_out_tick(midi_out_state_t *p_state, const midi_out_inputs_t *p_in,
                   midi_out_bytes_t *p_out)
{
    if ((NULL != p_state) && (NULL != p_in) && (NULL != p_out))
    {
        const uint8_t limit =
            (p_in->room < MIDI_OUT_MAX_BYTES) ? p_in->room : (uint8_t)MIDI_OUT_MAX_BYTES;

        p_out->count = 0U;

        // What this tick adds to what is owed
        mark_transport(p_state, p_in->b_running, p_in->b_external);
        count_pulses(p_state, p_in->pulses);
        mark_note(p_state, p_in);

        // As much of it as fits, in the order the header gives
        send_transport(p_state, p_out, limit);
        send_clocks(p_state, p_out, limit);
        send_note_on(p_state, p_out, limit);
        send_note_offs(p_state, p_out, limit);
    }
}
