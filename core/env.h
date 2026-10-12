#ifndef ENV_H
#define ENV_H
/**
 * @file   env.h
 * @brief  An envelope: the level, this tick, of a voltage that rises to a
 *         peak when a gate opens and falls back at once. Pure logic with no
 *         hardware access; compiles unchanged for the MCU and for a PC.
 * @author BonelessPig
 *
 * A rise of the gate starts the envelope: the level goes to the peak in a
 * straight line (the attack) and then back to 0 in another (the decay),
 * each taking the same time, whatever the gate does meanwhile. Nothing
 * sustains. The attack starts from wherever the level is, so a note that
 * begins before the last has died away does not make it jump. A gate that
 * stays open from one note to the next starts nothing.
 *
 * The peak is set by an amplitude, 0 to 255, taken as the gate rises: 255
 * is the highest level, 4095, and the rest are in proportion.
 *
 * The time of each slope comes from an amount, 0 to 1023, on a curve that
 * is fine at the bottom and coarse at the top: the amount is divided by 4,
 * squared and divided by 64, which is a time in milliseconds of 0 to 1016
 * (62 a quarter of the way up, 252 half way). It is read on every tick, so
 * a slope under way takes up the new time. A tick is taken to be a
 * millisecond.
 *
 * A time of 0, which is any amount below 32, is no envelope: the level
 * follows the gate, at the peak while it is open and at 0 while it is
 * closed.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief What the envelope is doing.
 */
typedef enum
{
    ENV_IDLE = 0, // Nothing: the level stays where it is
    ENV_ATTACK,   // Going to the peak
    ENV_DECAY     // Going back to 0
} env_phase_t;

/**
 * @brief State the envelope carries from one tick to the next.
 */
typedef struct
{
    uint16_t    level;     // The level now, 0 to 4095
    uint16_t    peak;      // The peak of the note last begun
    uint16_t    span;      // How far the slope it is on goes, end to end
    uint16_t    remainder; // What of the slope's movement is still owed,
                           // being less than one unit of level
    env_phase_t phase;     // What it is doing
    bool        b_gate;    // The gate as it was on the last tick
} env_state_t;

/**
 * @brief What the envelope is given on one tick.
 */
typedef struct
{
    bool     b_gate;        // Level of the gate after this tick
    uint8_t  amplitude;     // How high the peak is, 0 to 255. Only used on
                            // the tick the gate rises
    uint16_t time;          // The amount that sets the time of a slope, 0 to
                            // 1023. A larger value is taken as 1023
    uint8_t  elapsed_ticks; // Ticks (milliseconds) since the previous tick
} env_inputs_t;

/**
 * @brief  Puts the state into its power-on condition: at 0, with the gate
 *         closed and nothing under way.
 * @param  p_state  State to initialize. Ignored if null.
 */
void env_init(env_state_t *p_state);

/**
 * @brief  Runs one tick: starts the envelope if the gate has risen, and
 *         moves the level on by the elapsed time. Reads *p_in, reads and
 *         writes *p_state, and has no other effect.
 * @param  p_state  State from env_init() or the previous tick; updated.
 * @param  p_in     Inputs for this tick.
 * @return The level after this tick, 0 to 4095. 0 if either
 *         pointer is null.
 */
uint16_t env_tick(env_state_t *p_state, const env_inputs_t *p_in);

#endif /* ENV_H */
