/**
 * @file   pickup.c
 * @brief  Pickup implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "pickup.h"
#include <stddef.h>

// A reading this near the value has met it. Wider than the noise of a
// 10-bit reading, so that a pot left on a value is found there again
#define CATCH_WINDOW (8U)



/**
 * @brief  Says whether a reading has met a value: it is near it, or the
 *         reading before was on the value or on the other side of it.
 * @param  p_state  Pickup state, with the reading before this one.
 * @param  reading  The pot's position now.
 * @param  value    The value it is to meet.
 * @return true if the pot has caught the value.
 */
static bool has_caught(const pickup_state_t *p_state, uint16_t reading, uint16_t value)
{
    const uint16_t low      = (reading < value) ? reading : value;
    const uint16_t high     = (reading < value) ? value : reading;
    bool           b_caught = ((uint16_t)(high - low) <= CATCH_WINDOW);

    if ((!b_caught) && p_state->b_known)
    {
        const bool b_rose_past = (p_state->last <= value) && (reading > value);
        const bool b_fell_past = (p_state->last >= value) && (reading < value);

        b_caught = b_rose_past || b_fell_past;
    }

    return b_caught;
}



void pickup_init(pickup_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->b_attached = false;
        p_state->b_known    = false;
        p_state->last       = 0U;
    }
}



void pickup_release(pickup_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->b_attached = false;
    }
}



bool pickup_update(pickup_state_t *p_state, uint16_t reading, uint16_t *p_value)
{
    bool b_attached = false;

    if ((NULL != p_state) && (NULL != p_value))
    {
        if (!p_state->b_attached)
        {
            p_state->b_attached = has_caught(p_state, reading, *p_value);
        }
        if (p_state->b_attached)
        {
            *p_value = reading;
        }
        p_state->b_known = true;
        p_state->last    = reading;
        b_attached       = p_state->b_attached;
    }

    return b_attached;
}
