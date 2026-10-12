#ifndef LFO_H
#define LFO_H
/**
 * @file   lfo.h
 * @brief  The modulating signal: an LFO of the sequencer's own, a sine at a
 *         rate a control sets, added to the one from outside. Pure logic
 *         with no hardware access; compiles unchanged for the MCU and for a
 *         PC.
 * @author BonelessPig
 *
 * What it gives is a signal as mod.h describes one: a reading from 0 to
 * 1023 that swings either side of its centre, 512. The LFO's own swing is
 * the whole of that, 511 either way, and the amounts say how much of it
 * each thing gets.
 *
 * The rate control is a reading from 0 to 1023, on a curve, so that the slow
 * rates have most of its travel: the reading is brought to 0 to 255, squared
 * and divided by 48, and that is how far the cycle moves on in a tick, in
 * 65536ths of a cycle. Below a reading of 28 that is nothing and the LFO is
 * off. From there it is one cycle in 65.5 s, 1.3 Hz a quarter of the way
 * up, 5.2 Hz half way and 20.7 Hz at the top.
 *
 * It runs free: nothing but the rate at 0 starts its cycle again. Off, it
 * is at its centre, and from there it begins by going up.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include "mod.h"

/**
 * @brief State carried from one tick to the next. Set up by lfo_init().
 */
typedef struct
{
    uint16_t phase; // Where the cycle has got to, 65536 to a cycle
    uint16_t rate;  // The last valid reading of the rate control
} lfo_state_t;

/**
 * @brief What the LFO is given on a tick.
 */
typedef struct
{
    uint16_t rate;             // The rate control, 0 to 1023; more is 1023
    bool     b_rate_valid;     // false if rate could not be read; the last
                               // valid one is then kept
    uint16_t external;         // The signal from outside, 0 to 1023; more
                               // is 1023
    bool     b_external_valid; // false if external could not be read; it
                               // is then taken to be at its centre
    uint8_t  elapsed_ticks;    // Ticks (milliseconds) since the tick before
} lfo_inputs_t;

/**
 * @brief  Puts the state into its power-on condition: off, until a rate is
 *         read.
 * @param  p_state  State to initialize. Ignored if null.
 */
void lfo_init(lfo_state_t *p_state);

/**
 * @brief  Runs one tick: moves the cycle on by the elapsed time at the rate
 *         set, and gives the signal, which is the one from outside with the
 *         LFO's own swing added to it. Reads *p_state and *p_in, writes
 *         *p_state, and has no other effect.
 * @param  p_state  State from lfo_init() or the previous tick; updated.
 * @param  p_in     Inputs for this tick.
 * @return The signal, 0 to 1023: a sum past either end is that end. With
 *         the LFO off it is the signal from outside. 512, the centre, if
 *         either pointer is null.
 */
uint16_t lfo_tick(lfo_state_t *p_state, const lfo_inputs_t *p_in);

#endif /* LFO_H */
