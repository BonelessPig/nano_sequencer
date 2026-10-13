#ifndef BLINK_H
#define BLINK_H
/**
 * @file   blink.h
 * @brief  A status light: whether it is lit on this tick. Pure logic with
 *         no hardware access; compiles unchanged for the MCU and for a PC.
 * @author BonelessPig
 *
 * The light has a steady level, which the caller gives on every tick, and
 * says three things over it by turning the other way for a moment (dark if
 * it is lit, lit if it is dark):
 *   a number of short flickers   something was done; the number says what
 *   one long blink               something was asked and could not be done
 *   one short flicker            a stage of a hold has been reached
 * A flicker is 80 ms the other way and then 80 ms at the steady level, and
 * a long blink 500 ms of each. One asked for while another is running takes
 * its place.
 *
 * What the flickers mean is not this module's business.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief State the light carries from one tick to the next.
 */
typedef struct
{
    uint8_t  halves;     // Halves of flickers still to run, this one included;
                         // 0 when none is running, at most 16
    uint16_t ticks_left; // Ticks left of this half, 1 to 500
    uint16_t half_ticks; // The length of each half, in ticks
    uint8_t  hold_stage; // The hold's stage, as given on the last tick
} blink_state_t;

/**
 * @brief What the light is to show, and the time gone by.
 */
typedef struct
{
    bool    b_on;          // The steady level; true is lit
    uint8_t flickers;      // Short flickers to begin on this tick, 0 for
                           // none. More than 8 are taken as 8
    bool    b_refused;     // true to begin the long blink on this tick; it
                           // is begun in place of any flickers
    uint8_t hold_stage;    // How far a hold has got, in stages. One flicker
                           // is begun on a tick it is higher than on the
                           // tick before, unless something else is begun
    uint8_t elapsed_ticks; // Ticks (milliseconds) since the previous tick,
                           // 0 to 255. With 0 a flicker does not move on;
                           // a count longer than what is left of a half
                           // ends that half and no more
} blink_inputs_t;

/**
 * @brief  Puts the light in its starting condition: nothing running, and a
 *         hold at stage 0.
 * @param  p_state  State to initialize. Ignored if null.
 */
void blink_init(blink_state_t *p_state);

/**
 * @brief  Runs one tick: moves a running flicker on, begins what is asked
 *         for, and gives the light's level. Reads *p_state and *p_in,
 *         writes *p_state, and has no other effect.
 * @param  p_state  State from blink_init() or the previous tick; updated.
 * @param  p_in     What the light is to show and the time gone by.
 * @return true if the light is lit on this tick; false if either pointer
 *         is null.
 */
bool blink_tick(blink_state_t *p_state, const blink_inputs_t *p_in);

#endif /* BLINK_H */
