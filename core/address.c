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

// An offset: its fullest depth, how much of a ring that moves a place by,
// and half the depth, added before dividing by it to round to the nearest
#define DEPTH_MAX      (255U)
#define OFFSET_DIVISOR (2U)
#define ROUNDING       (127U)



/**
 * @brief  Gives the number of steps in a range.
 * @param  first  The first step, 0 to PANEL_STEP_COUNT - 1.
 * @param  last   The last step, 0 to 2 * PANEL_STEP_COUNT - 1; one below the
 *                first runs round through step 15.
 * @return Steps in the range, 1 to PANEL_STEP_COUNT.
 */
static uint8_t range_length(uint8_t first, uint8_t last)
{
    return (uint8_t)((((last + PANEL_STEP_COUNT) - first) % PANEL_STEP_COUNT) + 1U);
}



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
            const uint8_t length = range_length(first, last);
            const uint8_t cycle = cycle_length(p_config->direction, length);
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



uint8_t address_turn(uint8_t place, uint8_t count, const address_offset_t *p_offset)
{
    uint8_t turned = place;

    if ((NULL != p_offset) && (count > 0U))
    {
        const uint8_t  folded  = (uint8_t)(place % count);
        const uint16_t half    = (uint16_t)count / OFFSET_DIVISOR;
        // At most 127 * 255 + 127, which fits in 16 bits
        const uint16_t product = (uint16_t)(half * p_offset->depth);
        const uint16_t places  = (uint16_t)((uint16_t)(product + ROUNDING) / DEPTH_MAX);
        uint16_t       forward = places;

        // Back by some is forward by the rest, and some is never above half
        if (p_offset->b_negative)
        {
            forward = (uint16_t)(count - places);
        }

        // At most 254 + 255 before the remainder
        turned = (uint8_t)((uint16_t)(folded + forward) % count);
    }

    return turned;
}



uint8_t address_offset_step(const address_config_t *p_config, uint8_t step,
                            const address_offset_t *p_offset)
{
    uint8_t moved = step;

    if ((NULL != p_config) && (NULL != p_offset))
    {
        const uint8_t first  = (uint8_t)(p_config->first_step % PANEL_STEP_COUNT);
        const uint8_t last   = (uint8_t)(p_config->last_step % PANEL_STEP_COUNT);
        const uint8_t length = range_length(first, last);
        // How far the step is into the range. At most 15 + 16
        const uint8_t place  =
            (uint8_t)((uint8_t)((uint8_t)(step % PANEL_STEP_COUNT) + PANEL_STEP_COUNT) - first)
            % PANEL_STEP_COUNT;

        moved = (uint8_t)((uint8_t)(first + address_turn(place, length, p_offset))
                          % PANEL_STEP_COUNT);
    }

    return moved;
}
