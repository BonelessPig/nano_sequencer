#ifndef MIDI_OUT_H
#define MIDI_OUT_H
/**
 * @file   midi_out.h
 * @brief  MIDI out: which bytes this tick sends for the clock, the transport
 *         and the notes. Pure logic with no hardware access; compiles
 *         unchanged for the MCU and for a PC.
 * @author BonelessPig
 *
 * Everything goes out on channel 1 with no running status: every message
 * carries its status byte. Only whole messages are produced, and never more
 * bytes in a tick than the transport has room for, nor more than
 * MIDI_OUT_MAX_BYTES. What does not fit is owed and goes on a later tick, in
 * this order within a tick:
 *   - Stop (0xFC) when the pattern stops, and Start (0xFA) when it starts.
 *     The state begins stopped, so the first running tick sends Start. A stop
 *     and a start that both waited go as Stop and then Start; a start that
 *     was still waiting when the pattern stopped again is not sent, and
 *     neither is that stop.
 *   - Clock (0xF8), one for every 4 pulses, running or stopped. A start
 *     begins the count again so that a clock, the downbeat, falls where the
 *     first step begins and the rest every 4 pulses from it. On the internal
 *     clock the first step begins with the start, so the downbeat follows
 *     the Start at once and the pulses of the tick that started count
 *     towards the next. On the external clock the first step waits for the
 *     first pulse, so that pulse makes the downbeat, on the starting tick if
 *     it falls there and on a later one if not. Clocks owed from before a
 *     start are given up at it, so that the first clock after a Start is
 *     always its downbeat. The downbeat of a start that was never sent still
 *     goes, as a clock of the stopped pattern. A change of clock while
 *     running is neither a start nor a stop, and changes nothing.
 *   - Note On (0x90, note, 100) when a note starts. The lowest pitch is note
 *     24 (C1).
 *   - Note Off (0x80, note, 0) when the gate is low and a note is sounding.
 *
 * A note that starts while another sounds, the gate having stayed high, is a
 * tie. At a different pitch the new Note On goes first and then the old
 * note's Note Off; at the same pitch nothing is sent and the note stays held.
 *
 * A Start or a Stop is one byte, so it waits only when there is no room at
 * all, and then nothing else goes either: the clocks and notes wait behind
 * it, and a clock is never sent between a start and its Start.
 *
 * No note is left sounding. A Note Off that did not fit is sent when there is
 * room, however long that takes. A Note On waits while an earlier Note Off is
 * still owed, so at most two notes ever sound at the receiver, and one that
 * is still waiting when its gate falls is given up: it never sounded, so it
 * has no Note Off either. A note whose gate is already low after the tick it
 * started on is given up in the same way, as the gate output never rose for
 * it. A later note replaces one still waiting.
 *
 * The module does not look at b_running to end a note: the caller gives the
 * gate as low on every stopped tick.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>

// The most bytes one tick produces
#define MIDI_OUT_MAX_BYTES (8U)

/**
 * @brief State MIDI out carries from one tick to the next.
 */
typedef struct
{
    bool    b_running;    // The pattern as last seen: true while running
    bool    b_stop_owed;  // A Stop has still to be sent
    bool    b_start_owed; // A Start has still to be sent, after any Stop
    bool    b_release;    // note_on has still to be given its Note Off
    uint8_t pulse_count;  // Pulses towards the next clock, 0 to 3. A start
                          // makes it 0 on the internal clock and 3 on the
                          // external one
    uint8_t clocks_owed;  // Clocks still to be sent, 0 to 255. It stops at
                          // 255 and does not wrap
    uint8_t note_on;      // Note the receiver is sounding, 24 to 69; 255 if
                          // there is none
    uint8_t note_off;     // The note before it, which has still to be given
                          // its Note Off after a tie; 255 if there is none
    uint8_t note_wait;    // Note whose Note On has still to be sent; 255 if
                          // there is none
} midi_out_state_t;

/**
 * @brief What happened on one tick, and what can be sent.
 */
typedef struct
{
    bool    b_running;      // true while the pattern is running, on either
                            // clock
    bool    b_external;     // true while it runs from the external clock,
                            // where a start's first step waits for the first
                            // pulse. Only used on the tick a start is seen
    bool    b_note_started; // true on the tick a step with a pitch begins
    uint8_t semitone;       // Its pitch in semitones above the lowest, 0 to
                            // 45. A higher value is treated as 45, so the
                            // note is 69 at most. Only used when
                            // b_note_started is true
    bool    b_gate;         // Level of the gate after this tick
    uint8_t pulses;         // Clock pulses (96 to the quarter note) that
                            // fell on this tick, 0 to 255
    uint8_t room;           // Bytes the MIDI transport can take now, 0 to 255
} midi_out_inputs_t;

/**
 * @brief The bytes to send after a tick, in order.
 */
typedef struct
{
    uint8_t bytes[MIDI_OUT_MAX_BYTES]; // The first count of them are to be
                                       // sent; the rest are not written
    uint8_t count;                     // 0 to MIDI_OUT_MAX_BYTES, and no more
                                       // than the room given
} midi_out_bytes_t;

/**
 * @brief  Puts the state into its power-on condition: stopped, with no note
 *         sounding and nothing owed.
 * @param  p_state  State to initialize. Ignored if null.
 */
void midi_out_init(midi_out_state_t *p_state);

/**
 * @brief  Runs one tick: works out the MIDI bytes to send for it, with any
 *         owed from earlier ticks first in their place (see the top of this
 *         file). Reads *p_state and *p_in, writes *p_state and *p_out, and
 *         has no other effect.
 * @param  p_state  State from midi_out_init() or the previous tick; updated.
 * @param  p_in     Inputs for this tick.
 * @param  p_out    Receives the bytes. count is always written; only the
 *                  first count of bytes are.
 * @note   Does nothing if any pointer is null.
 */
void midi_out_tick(midi_out_state_t *p_state, const midi_out_inputs_t *p_in,
                   midi_out_bytes_t *p_out);

#endif /* MIDI_OUT_H */
