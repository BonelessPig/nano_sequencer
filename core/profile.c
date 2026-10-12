/**
 * @file   profile.c
 * @brief  Profile button implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "profile.h"
#include <stddef.h>

// How long the button has to be held for each thing it asks, in ticks
// (milliseconds). A shorter hold than the first is a recall
#define SAVE_HOLD_TICKS      (1000U)
#define CLEAR_HOLD_TICKS     (5000U)
#define CLEAR_ALL_HOLD_TICKS (10000U)

// The count stops here: nothing is asked by a longer hold than the longest
#define HELD_TICKS_MAX (CLEAR_ALL_HOLD_TICKS)



/**
 * @brief  Gives what a hold of some length asks for.
 * @param  held_ticks  How long the button was down, in ticks.
 * @return The action; never PROFILE_NONE.
 */
static profile_action_t action_for(uint16_t held_ticks)
{
    profile_action_t action = PROFILE_RECALL;

    if (held_ticks >= CLEAR_ALL_HOLD_TICKS)
    {
        action = PROFILE_CLEAR_ALL;
    }
    else if (held_ticks >= CLEAR_HOLD_TICKS)
    {
        action = PROFILE_CLEAR;
    }
    else if (held_ticks >= SAVE_HOLD_TICKS)
    {
        action = PROFILE_SAVE;
    }
    else
    {
        // A click
    }

    return action;
}



void profile_init(profile_state_t *p_state)
{
    if (NULL != p_state)
    {
        debounce_init(&p_state->button);
        p_state->held_ticks = 0U;
        p_state->b_loaded   = false;
    }
}



profile_action_t profile_tick(profile_state_t *p_state, const profile_inputs_t *p_in)
{
    profile_action_t action = PROFILE_NONE;

    if ((NULL != p_state) && (NULL != p_in))
    {
        const bool b_down =
            debounce_update(&p_state->button, p_in->b_button, p_in->elapsed_ticks);

        if ((!p_state->b_loaded) && p_in->b_button_valid)
        {
            // Power-on: the profile is recalled once, as soon as it can be
            p_state->b_loaded = true;
            action            = PROFILE_RECALL;
        }
        else if (b_down)
        {
            // At most 10000 + 255, so the sum fits in 16 bits
            uint16_t held = (uint16_t)(p_state->held_ticks + p_in->elapsed_ticks);

            if (held > HELD_TICKS_MAX)
            {
                held = (uint16_t)HELD_TICKS_MAX;
            }
            p_state->held_ticks = held;
        }
        else if (p_state->held_ticks > 0U)
        {
            action              = action_for(p_state->held_ticks);
            p_state->held_ticks = 0U;
        }
        else
        {
            // Up, and nothing was held
        }
    }

    return action;
}
