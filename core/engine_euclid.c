/**
 * @file   engine_euclid.c
 * @brief  Euclidean engine implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "engine_euclid.h"
#include <stddef.h>
#include "panel.h"

#define BAR_STEPS (16U) // Steps in the bar the beats are spread over

// The mode switches that are this engine's own: the beats in a bar, less one
#define BEATS_SHIFT (3U)
#define BEATS_MASK  (0x0FU)



void engine_euclid_init(engine_euclid_state_t *p_state)
{
    if (NULL != p_state)
    {
        address_init(&p_state->address);
        p_state->beat = 0U;
    }
}



bool engine_euclid_step(engine_euclid_state_t *p_state, const engine_inputs_t *p_in,
                        engine_step_t *p_step)
{
    bool b_step = false;

    if ((NULL != p_state) && (NULL != p_in) && (NULL != p_step)
        && (NULL != p_in->p_raw_steps))
    {
        uint8_t step = 0U;

        // The addressing checks its own pointers
        if (address_next(&p_state->address, p_in->p_address, &step))
        {
            const uint8_t mode  = panel_mode_value(p_in->p_raw_steps);
            const uint8_t beats =
                (uint8_t)((uint8_t)((uint8_t)(mode >> BEATS_SHIFT) & BEATS_MASK) + 1U);
            // A step of the bar is a beat if stepping to it carried the
            // running total of beats past a multiple of the bar. At most
            // 15 x 16, so the product fits in a byte
            const uint8_t phase = (uint8_t)((uint8_t)(p_state->beat * beats) % BAR_STEPS);

            // Which step sounds is moved; whether one does is not
            step          = address_offset_step(p_in->p_address, step, &p_in->offset);
            p_step->step  = step;
            p_step->note  = 0U;
            p_step->b_tie = false;
            if (phase < beats)
            {
                p_step->note = panel_step_value(p_in->p_raw_steps, step);
            }

            p_state->beat = (uint8_t)((uint8_t)(p_state->beat + 1U) % BAR_STEPS);
            b_step        = true;
        }
    }

    return b_step;
}
