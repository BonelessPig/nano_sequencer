#ifndef PROFILE_H
#define PROFILE_H
/**
 * @file   profile.h
 * @brief  The profile button: what a press of it asks for, by how long it
 *         was held. Pure logic with no hardware access; compiles unchanged
 *         for the MCU and for a PC.
 * @author BonelessPig
 *
 * A profile is a stored set of amounts (see profile_record.h). One button
 * works them, and what it asks for is decided when it is let go:
 *   held under 1 s      recall the profile
 *   held 1 s or more    save to it
 *   held 5 s or more    clear it
 *   held 10 s or more   clear every profile
 * The button is debounced here (see debounce.h), so it arrives as read. A
 * button that is already down at the first tick counts as held from then.
 *
 * At power-on the profile is recalled without the button: the first tick
 * on which the button could be read, which is the first on which the
 * caller's selector could be, asks for a load.
 *
 * Which profile is meant is not this module's business: the caller takes it
 * from wherever its selector stands when the action is given.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include "debounce.h"

/**
 * @brief What the button asked for on this tick.
 */
typedef enum
{
    PROFILE_NONE = 0,  // Nothing
    PROFILE_RECALL,    // Put the stored amounts in use
    PROFILE_SAVE,      // Store the amounts in use
    PROFILE_CLEAR,     // Empty the profile
    PROFILE_CLEAR_ALL, // Empty every profile
    PROFILE_LOAD       // Put the stored amounts in use, at power-on: a
                       // recall the button did not ask for
} profile_action_t;

/**
 * @brief State the button carries from one tick to the next.
 */
typedef struct
{
    debounce_state_t button;     // The button's settled level
    uint16_t         held_ticks; // Ticks it has been down, 0 to 10000
    bool             b_loaded;   // true once the power-on recall has been asked
} profile_state_t;

/**
 * @brief The button, as last read, and the time gone by.
 */
typedef struct
{
    bool    b_button;       // The button as read, bounce and all; true is down
    bool    b_button_valid; // false if it could not be read; b_button is
                            // then taken as it stands, and no recall is
                            // asked for at power-on until it can be
    uint8_t elapsed_ticks;  // Ticks (milliseconds) since the previous tick,
                            // 0 to 255. The button is taken to have stood
                            // as read for all of them; with 0 a hold is
                            // not counted on
} profile_inputs_t;

/**
 * @brief  Puts the button in its starting condition: not held, and the
 *         power-on recall still to be asked.
 * @param  p_state  State to initialize. Ignored if null.
 */
void profile_init(profile_state_t *p_state);

/**
 * @brief  Runs one tick: follows the button and, on the tick it is found
 *         let go, gives what its hold asked for. Reads *p_state and *p_in,
 *         writes *p_state, and has no other effect.
 * @param  p_state  State from profile_init() or the previous tick; updated.
 * @param  p_in     The button and the time gone by.
 * @return The action: PROFILE_LOAD on the first tick with a valid
 *         button, then one for each press, on one tick; PROFILE_NONE on
 *         every other, and if either pointer is null.
 */
profile_action_t profile_tick(profile_state_t *p_state, const profile_inputs_t *p_in);

/**
 * @brief  Gives how far the hold that is going on has got, so that it can
 *         be shown before the button is let go.
 * @param  p_state  State from the last profile_tick().
 * @return 0 while the button is up or has been down for under 1 s, 1 from
 *         1 s (a save), 2 from 5 s (a clear), 3 from 10 s (a clear of every
 *         profile); 0 if the pointer is null.
 */
uint8_t profile_hold_stage(const profile_state_t *p_state);

#endif /* PROFILE_H */
