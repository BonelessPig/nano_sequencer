#ifndef MIDI_IN_H
#define MIDI_IN_H
/**
 * @file   midi_in.h
 * @brief  MIDI in: what the bytes received since the last tick ask of the
 *         transport. Pure logic with no hardware access; compiles unchanged
 *         for the MCU and for a PC.
 * @author BonelessPig
 *
 * Three things are taken from the stream, each a single byte that can come
 * anywhere in it: the clock (0xF8), Start (0xFA) and Continue (0xFB), which
 * are one thing here, and Stop (0xFC). Every other byte is passed over, the
 * bytes of a note among them, so nothing has to be put together from more
 * than one byte and a byte that was lost on the way costs nothing but
 * itself.
 *
 * A MIDI clock comes 24 times a quarter note, which is six times a step. The
 * sixth of them is an edge, as an edge on the clock input is one at a step
 * to an edge, and the others only count. The first clock after
 * midi_in_init() is an edge, and so is the first after a Start, so that the
 * pattern a Start starts begins on the clock that follows it.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief State MIDI in carries from one tick to the next.
 */
typedef struct
{
    uint8_t clocks_to_edge; // Clocks to pass over before the next edge, 0 to 5
} midi_in_state_t;

/**
 * @brief What the bytes of one tick came to.
 */
typedef struct
{
    bool b_edge;  // true if a clock that is an edge arrived, and no Start
                  // after it
    bool b_start; // true if a Start or a Continue arrived, and no Stop after it
    bool b_stop;  // true if a Stop arrived, and no Start or Continue after it
} midi_in_events_t;

/**
 * @brief  Puts MIDI in where the next clock is an edge.
 * @param  p_state  State to initialize. Ignored if null.
 */
void midi_in_init(midi_in_state_t *p_state);

/**
 * @brief  Takes the bytes received since the previous call, in the order
 *         they came, and says what they ask for. Reads *p_state and the
 *         bytes, writes *p_state and *p_out, and has no other effect.
 *
 *         Of a Start and a Stop in the same call the later one is given and
 *         the earlier forgotten. A Start also forgets an edge that came
 *         ahead of it, since that clock belonged to what was playing before.
 * @param  p_state  State from midi_in_init() or the previous call; updated.
 * @param  p_bytes  The bytes. Null is taken as none.
 * @param  count    How many, 0 to 255.
 * @param  p_out    Receives the events; every field is written.
 * @note   Does nothing if p_state or p_out is null.
 */
void midi_in_read(midi_in_state_t *p_state, const uint8_t *p_bytes, uint8_t count,
                  midi_in_events_t *p_out);

#endif /* MIDI_IN_H */
