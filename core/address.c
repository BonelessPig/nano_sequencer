/**
 * @file   address.c
 * @brief  Step addressing implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "address.h"
#include <stddef.h>
#include "panel.h"

#define SINGLE_STEP    (1U) // A range this long has nowhere to turn round
#define PENDULUM_LEGS  (2U) // A pendulum cycle goes out and comes back



/**
 * @brief  Gives the number of steps in one cycle.
 * @param  direction  The order the range is played in.
 * @param  length     Steps in the range, 1 to PANEL_STEP_COUNT.
 * @return Steps in one cycle, 1 to 30.
 */
static uint8_t cycle_length(address_direction_t direction, uint8_t length)
{
    uint8_t cycle = length;

    // Out and back, with neither end repeated
    if ((ADDRESS_PENDULUM == direction) && (length > SINGLE_STEP))
    {
        cycle = (uint8_t)(PENDULUM_LEGS * (length - 1U));
    }

    return cycle;
}



/**
 * @brief  Gives how far into the range the step at a point in the cycle is.
 * @param  direction  The order the range is played in.
 * @param  length     Steps in the range, 1 to PANEL_STEP_COUNT.
 * @param  position   Steps already played in this cycle, below the cycle
 *                    length.
 * @return Offset from the first step of the range, 0 to length - 1.
 */
static uint8_t range_offset(address_direction_t direction, uint8_t length,
                            uint8_t position)
{
    uint8_t offset = position;

    switch (direction)
    {
        case ADDRESS_REVERSE:
            offset = (uint8_t)((length - 1U) - position);
            break;

        case ADDRESS_PENDULUM:
            if (position >= length)
            {
                // On the way back
                offset = (uint8_t)(cycle_length(direction, length) - position);
            }
            break;

        case ADDRESS_FORWARD:
        default:
            // Forward, and any unknown direction: the offset is the position
            break;
    }

    return offset;
}



void address_init(address_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->position   = 0U;
        p_state->b_finished = false;
    }
}



bool address_next(address_state_t *p_state, const address_config_t *p_config,
                  uint8_t *p_step)
{
    bool b_step = false;

    if ((NULL != p_state) && (NULL != p_config) && (NULL != p_step))
    {
        // Looping again after a one-shot cycle starts the next cycle
        if (!p_config->b_one_shot)
        {
            p_state->b_finished = false;
        }

        if (!p_state->b_finished)
        {
            const uint8_t first  = (uint8_t)(p_config->first_step % PANEL_STEP_COUNT);
            const uint8_t last   = (uint8_t)(p_config->last_step % PANEL_STEP_COUNT);
            const uint8_t length =
                (uint8_t)((((last + PANEL_STEP_COUNT) - first) % PANEL_STEP_COUNT) + 1U);
            const uint8_t cycle  = cycle_length(p_config->direction, length);
            uint8_t       position = p_state->position;

            // The range or direction changed and left the count past the end
            if (position >= cycle)
            {
                position = 0U;
            }

            *p_step = (uint8_t)((first + range_offset(p_config->direction, length, position))
                                % PANEL_STEP_COUNT);
            b_step  = true;

            position++;
            if (position >= cycle)
            {
                position            = 0U;
                p_state->b_finished = p_config->b_one_shot;
            }
            p_state->position = position;
        }
    }

    return b_step;
}
