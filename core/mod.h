#ifndef MOD_H
#define MOD_H
/**
 * @file   mod.h
 * @brief  Modulation: how far a modulating signal, scaled by an amount,
 *         moves a value, and whether it makes a chance come up. Pure logic
 *         with no hardware access; compiles unchanged for the MCU and for a
 *         PC.
 * @author BonelessPig
 *
 * The signal is a reading from 0 to 1023 that swings either side of its
 * centre, 512. At the centre it does nothing. So does a reading within 16
 * of the centre, which is what an input with nothing plugged into it gives,
 * noise and all. The amount is 0 to 1023, and 0 is off.
 *
 * The two are multiplied into a depth, 0 to 255, with a direction: 255 is
 * the signal at one end of its swing with the amount at its top. What the
 * depth does to anything is the caller's choice, made with mod_shift() or
 * mod_chance().
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include "prng.h"

#define MOD_SIGNAL_CENTRE (512U) // The reading at which the signal does nothing

/**
 * @brief How far a signal and an amount move something, and which way.
 */
typedef struct
{
    uint8_t depth;      // 0 for nothing, up to 255 for the most
    bool    b_negative; // true if the signal is below its centre
} mod_depth_t;

/**
 * @brief  Works out a depth from an amount and the signal as it reads now.
 *         Writes *p_depth and has no other effect.
 * @param  amount   How much of the signal is let through, 0 to 1023. A
 *                  larger value is taken as 1023.
 * @param  signal   The signal's reading, 0 to 1023. A larger value is taken
 *                  as 1023.
 * @param  p_depth  Receives the depth; both fields are written. Nothing is
 *                  done if it is null.
 */
void mod_depth(uint16_t amount, uint16_t signal, mod_depth_t *p_depth);

/**
 * @brief  Moves a value up or down by a depth: by `range` at a depth of 255
 *         and in proportion below that, rounded to the nearest whole
 *         number, upwards if the signal is above its centre and downwards
 *         if below. Reads *p_depth and has no other effect.
 * @param  value    The value to move.
 * @param  p_depth  The depth, from mod_depth().
 * @param  range    How far a depth of 255 moves it, 0 to 255.
 * @param  low      The least the moved value may be.
 * @param  high     The most it may be. If low is above high, high is used.
 * @return The moved value, kept within low to high. A depth that does not
 *         come to a whole number moves nothing, and the value is then
 *         returned as it was given, within low to high or not. So is it if
 *         p_depth is null.
 */
uint8_t mod_shift(uint8_t value, const mod_depth_t *p_depth, uint8_t range,
                  uint8_t low, uint8_t high);

/**
 * @brief  Says whether a chance comes up: never at a depth of 0 or with the
 *         signal below its centre, and otherwise depth times in 256. It
 *         draws from the generator only when the chance is not 0, so that
 *         with no modulation the random numbers are left for whoever else
 *         uses them. Reads *p_depth, moves *p_prng on, and has no other
 *         effect.
 * @param  p_depth  The depth, from mod_depth().
 * @param  p_prng   The generator to draw from. With none, the chance never
 *                  comes up.
 * @return true if it came up. false if p_depth is null.
 */
bool mod_chance(const mod_depth_t *p_depth, prng_state_t *p_prng);

/**
 * @brief  Gives the part of a span that a depth comes to: all of it at a
 *         depth of 255 and in proportion below that, rounded to the nearest
 *         whole number. Which way the signal swings is left to the caller.
 *         Reads *p_depth and has no other effect.
 * @param  span     The span, in whatever the caller counts in, 0 to 65535.
 * @param  p_depth  The depth, from mod_depth().
 * @return The part, 0 to span. 0 if p_depth is null.
 */
uint16_t mod_scale(uint16_t span, const mod_depth_t *p_depth);

#endif /* MOD_H */
