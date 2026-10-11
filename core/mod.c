/**
 * @file   mod.c
 * @brief  Modulation implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "mod.h"
#include <stddef.h>

#define READING_MAX (1023U) // The top of an amount and of the signal
#define DEPTH_MAX   (255U)

// A swing of less than this either side of the centre is taken as none
#define DEAD_ZONE (16U)

// An amount is 10 bits and the swing 9; each is brought down to 8, so that
// their product fits in 16
#define AMOUNT_PER_LEVEL (4U)
#define SWING_PER_LEVEL  (2U)

// Half of DEPTH_MAX, added before dividing by it to round to the nearest
#define ROUNDING (127U)



/**
 * @brief  Gives the lesser of a value and a limit.
 * @param  value  The value.
 * @param  limit  The most it may be.
 * @return value, or limit if value is above it.
 */
static uint16_t at_most(uint16_t value, uint16_t limit)
{
    return (value > limit) ? limit : value;
}



void mod_depth(uint16_t amount, uint16_t signal, mod_depth_t *p_depth)
{
    if (NULL != p_depth)
    {
        const uint16_t reading    = at_most(signal, (uint16_t)READING_MAX);
        const uint16_t level      = (uint16_t)(at_most(amount, (uint16_t)READING_MAX)
                                               / AMOUNT_PER_LEVEL);
        const bool     b_negative = (reading < MOD_SIGNAL_CENTRE);
        uint16_t       swing      = b_negative ? (uint16_t)(MOD_SIGNAL_CENTRE - reading)
                                               : (uint16_t)(reading - MOD_SIGNAL_CENTRE);

        if (swing < DEAD_ZONE)
        {
            swing = 0U;
        }
        // Up to 512 below the centre and 511 above, so 256 at the most
        swing = at_most((uint16_t)(swing / SWING_PER_LEVEL), (uint16_t)DEPTH_MAX);

        // At most 255 * 255, which fits in 16 bits
        p_depth->depth      = (uint8_t)((uint16_t)(level * swing) / DEPTH_MAX);
        p_depth->b_negative = b_negative;
    }
}



uint8_t mod_shift(uint8_t value, const mod_depth_t *p_depth, uint8_t range,
                  uint8_t low, uint8_t high)
{
    uint8_t shifted = value;

    if (NULL != p_depth)
    {
        // At most 255 * 255 + 127, which fits in 16 bits
        const uint16_t product = (uint16_t)((uint16_t)p_depth->depth * range);
        const uint16_t steps   = (uint16_t)((uint16_t)(product + ROUNDING) / DEPTH_MAX);

        if (steps > 0U)
        {
            // At most 255 + 255
            uint16_t moved = (uint16_t)((uint16_t)value + steps);

            if (p_depth->b_negative)
            {
                moved = (value > steps) ? (uint16_t)((uint16_t)value - steps) : 0U;
            }
            if (moved < low)
            {
                moved = low;
            }
            // Within a byte, high being one
            shifted = (uint8_t)at_most(moved, high);
        }
    }

    return shifted;
}



bool mod_chance(const mod_depth_t *p_depth, prng_state_t *p_prng)
{
    bool b_came_up = false;

    if ((NULL != p_depth) && (!p_depth->b_negative) && (p_depth->depth > 0U))
    {
        // The top `depth` draws of the 256 come up. A draw of 0 never does,
        // and 0 is all that a missing generator gives. At most 255 + 255
        const uint16_t draw  = prng_byte(p_prng);
        const uint16_t reach = (uint16_t)(draw + p_depth->depth);

        b_came_up = (reach > DEPTH_MAX);
    }

    return b_came_up;
}
