/**
 * @file   blink.c
 * @brief  Status light implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "blink.h"
#include <stddef.h>

// How long each half of a flicker lasts, and of the long blink, in ticks
// (milliseconds): the half turned the other way, then the half back
#define FLICKER_HALF_TICKS (80U)
#define LONG_HALF_TICKS    (500U)

// The most flickers that are begun at once
#define FLICKERS_MAX (8U)



/**
 * @brief  Begins a run of flickers, in place of any that is running.
 * @param  p_state     The light's state; updated.
 * @param  flickers    How many, 1 to 8.
 * @param  half_ticks  The length of each half of one, in ticks.
 */
static void begin(blink_state_t *p_state, uint8_t flickers, uint16_t half_ticks)
{
    // At most 2 * 8
    p_state->halves     = (uint8_t)(flickers * 2U);
    p_state->ticks_left = half_ticks;
    p_state->half_ticks = half_ticks;
}



/**
 * @brief  Moves a running flicker on by the time gone by. A count that
 *         reaches the end of the half begins the next, whole.
 * @param  p_state        The light's state; updated.
 * @param  elapsed_ticks  Ticks since the previous tick.
 */
static void advance(blink_state_t *p_state, uint8_t elapsed_ticks)
{
    if (p_state->halves > 0U)
    {
        if ((uint16_t)elapsed_ticks >= p_state->ticks_left)
        {
            p_state->halves--;
            p_state->ticks_left = p_state->half_ticks;
        }
        else
        {
            p_state->ticks_left = (uint16_t)(p_state->ticks_left - elapsed_ticks);
        }
    }
}



void blink_init(blink_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->halves     = 0U;
        p_state->ticks_left = 0U;
        p_state->half_ticks = 0U;
        p_state->hold_stage = 0U;
    }
}



bool blink_tick(blink_state_t *p_state, const blink_inputs_t *p_in)
{
    bool b_lit = false;

    if ((NULL != p_state) && (NULL != p_in))
    {
        advance(p_state, p_in->elapsed_ticks);

        if (p_in->b_refused)
        {
            begin(p_state, 1U, (uint16_t)LONG_HALF_TICKS);
        }
        else if (p_in->flickers > FLICKERS_MAX)
        {
            begin(p_state, (uint8_t)FLICKERS_MAX, (uint16_t)FLICKER_HALF_TICKS);
        }
        else if (p_in->flickers > 0U)
        {
            begin(p_state, p_in->flickers, (uint16_t)FLICKER_HALF_TICKS);
        }
        else if (p_in->hold_stage > p_state->hold_stage)
        {
            begin(p_state, 1U, (uint16_t)FLICKER_HALF_TICKS);
        }
        else
        {
            // Nothing new
        }
        p_state->hold_stage = p_in->hold_stage;

        // The halves count down from an even number, so an even one left is
        // the half turned the other way
        b_lit = p_in->b_on;
        if ((p_state->halves > 0U) && (0U == (p_state->halves & 1U)))
        {
            b_lit = !b_lit;
        }
    }

    return b_lit;
}
