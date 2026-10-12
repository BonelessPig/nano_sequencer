/**
 * @file   lfo.c
 * @brief  The modulating signal: the sequencer's own LFO added to the one
 *         from outside. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "lfo.h"
#include <stddef.h>

#define SIGNAL_MAX (1023U) // The top of the signal and of the rate control

// The rate control is brought down to 8 bits, so that its square fits in
// 16, and the square divided by this is the cycle's step in a tick
#define RATE_PER_LEVEL (4U)
#define RATE_DIVISOR   (48U)

// The cycle is 65536 long, in four quarters: up from the centre, back to
// it, down from it, and back. The second half is below the centre, and
// the second and fourth quarters run the first one backwards
#define PHASE_LOWER_HALF (0x8000U)
#define PHASE_RETURNING  (0x4000U)
#define QUARTER_LENGTH   (0x4000U)
#define QUARTER_MASK     (0x3FFFU)

// A quarter is cut into this many segments, with a straight line across
// each: 1024 of the cycle to a segment
#define SEGMENT_COUNT (16U)
#define SEGMENT_SHIFT (10U)
#define SEGMENT_MASK  (0x03FFU)
#define SEGMENT_HALF  (512U) // Added before the shift to round to the nearest

// The sine at its top, as the table holds it, and the shift that takes the
// top bit of one to make twice it the whole swing, 511
#define LEVEL_TOP_SHIFT (7U)



/**
 * @brief  Gives the lesser of a value and a limit.
 * @param  value  The value.
 * @param  limit  The most it may be.
 * @return value, or limit if value is above it.
 */
static uint16_t capped(uint16_t value, uint16_t limit)
{
    return (value > limit) ? limit : value;
}



/**
 * @brief  Gives how far the cycle moves on in a tick at a rate.
 * @param  rate  The rate control, 0 to 1023; more is 1023.
 * @return 65536ths of a cycle, 0 to 1354; 0 is off.
 */
static uint16_t step_per_tick(uint16_t rate)
{
    const uint16_t level = (uint16_t)(capped(rate, (uint16_t)SIGNAL_MAX) / RATE_PER_LEVEL);

    // At most 255 * 255, which fits in 16 bits
    return (uint16_t)((uint16_t)(level * level) / RATE_DIVISOR);
}



/**
 * @brief  Gives how far the sine is from its centre at a place in the
 *         cycle, whichever side it is on.
 * @param  phase  Where the cycle has got to, 65536 to a cycle.
 * @return The swing, 0 to 511.
 */
static uint16_t sine_swing(uint16_t phase)
{
    // A quarter of a sine, 255 at its top, at the ends of the segments
    static const uint8_t quarter[SEGMENT_COUNT + 1U] =
    {
        0U,   25U,  50U,  74U,  98U,  120U, 142U, 162U, 180U,
        197U, 212U, 225U, 236U, 244U, 250U, 254U, 255U
    };
    uint16_t place = (uint16_t)(phase & QUARTER_MASK);
    uint16_t segment;
    uint16_t level;

    if (0U != (phase & PHASE_RETURNING))
    {
        // 1 to 16384: the top of the quarter is the start of this one
        place = (uint16_t)(QUARTER_LENGTH - place);
    }

    segment = (uint16_t)(place >> SEGMENT_SHIFT);
    level   = quarter[segment];
    if (segment < SEGMENT_COUNT)
    {
        // The table rises by 25 at the most, so at most 25 * 1023 + 512
        const uint16_t rise   = (uint16_t)((uint16_t)quarter[segment + 1U] - level);
        const uint16_t across = (uint16_t)(place & SEGMENT_MASK);

        level = (uint16_t)(level
                           + (uint16_t)((uint16_t)((uint16_t)(rise * across) + SEGMENT_HALF)
                                        >> SEGMENT_SHIFT));
    }

    // Twice the level, and one more at the top: 255 is 511
    return (uint16_t)((uint16_t)(level * 2U) + (uint16_t)(level >> LEVEL_TOP_SHIFT));
}



void lfo_init(lfo_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->phase = 0U;
        p_state->rate  = 0U;
    }
}



uint16_t lfo_tick(lfo_state_t *p_state, const lfo_inputs_t *p_in)
{
    uint16_t signal = (uint16_t)MOD_SIGNAL_CENTRE;

    if ((NULL != p_state) && (NULL != p_in))
    {
        uint16_t step;
        uint16_t swing;

        if (p_in->b_rate_valid)
        {
            p_state->rate = p_in->rate;
        }

        step = step_per_tick(p_state->rate);
        if (0U == step)
        {
            // Off: at the centre, to begin from there by going up
            p_state->phase = 0U;
        }
        else
        {
            // The cycle comes round at 65536, which is where 16 bits do,
            // so the product and the sum are both left to wrap
            const uint16_t moved = (uint16_t)(step * p_in->elapsed_ticks);

            p_state->phase = (uint16_t)(p_state->phase + moved);
        }

        if (p_in->b_external_valid)
        {
            signal = capped(p_in->external, (uint16_t)SIGNAL_MAX);
        }

        // At most 1023 + 511
        swing = sine_swing(p_state->phase);
        if (p_state->phase < PHASE_LOWER_HALF)
        {
            signal = capped((uint16_t)(signal + swing), (uint16_t)SIGNAL_MAX);
        }
        else
        {
            signal = (signal > swing) ? (uint16_t)(signal - swing) : 0U;
        }
    }

    return signal;
}
