/**
 * @file   engine_chance.c
 * @brief  Chance engine implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "engine_chance.h"
#include <stddef.h>
#include "panel.h"

// The mode switches that are this engine's own
#define SKIP_SHIFT   (3U)    // Mode switches 3 and 4: skips in four
#define SKIP_MASK    (0x03U)
#define MODE_SHUFFLE (0x20U)

#define DRAW_MASK  (0x03U) // A draw for a skip is 0 to 3
#define DRAW_COUNT (4U)



/**
 * @brief  Picks a step from the range at random.
 * @param  p_in  The addressing controls and the random numbers; the
 *               controls not null.
 * @return A step from the first to the last, 0 to PANEL_STEP_COUNT - 1.
 */
static uint8_t shuffled_step(const engine_inputs_t *p_in)
{
    const uint8_t first  = (uint8_t)(p_in->p_address->first_step % PANEL_STEP_COUNT);
    const uint8_t last   = (uint8_t)(p_in->p_address->last_step % PANEL_STEP_COUNT);
    // Steps in the range, less one; it runs round through step 15 if the
    // first is above the last. At most 15 + 16 before the remainder
    const uint8_t span   =
        (uint8_t)((uint8_t)((uint8_t)(last + PANEL_STEP_COUNT) - first) % PANEL_STEP_COUNT);
    const uint8_t offset = (uint8_t)(prng_byte(p_in->p_prng) % (uint8_t)(span + 1U));

    return (uint8_t)((uint8_t)(first + offset) % PANEL_STEP_COUNT);
}



void engine_chance_init(engine_chance_state_t *p_state)
{
    if (NULL != p_state)
    {
        address_init(&p_state->address);
    }
}



bool engine_chance_step(engine_chance_state_t *p_state, const engine_inputs_t *p_in,
                        engine_step_t *p_step)
{
    bool b_step = false;

    if ((NULL != p_state) && (NULL != p_in) && (NULL != p_step)
        && (NULL != p_in->p_raw_steps) && (NULL != p_in->p_address))
    {
        uint8_t step = 0U;

        // The addressing still counts the cycle, so that a one-shot ends
        if (address_next(&p_state->address, p_in->p_address, &step))
        {
            const uint8_t mode = panel_mode_value(p_in->p_raw_steps);
            const uint8_t skip = (uint8_t)((uint8_t)(mode >> SKIP_SHIFT) & SKIP_MASK);
            uint8_t       draw;

            if (0U != (mode & MODE_SHUFFLE))
            {
                step = shuffled_step(p_in);
            }

            // A draw of 0 to 3 skips the step if it is among the top few,
            // as many as the setting says; with a setting of 0, never
            draw          = (uint8_t)(prng_byte(p_in->p_prng) & DRAW_MASK);
            p_step->step  = step;
            p_step->note  = panel_step_value(p_in->p_raw_steps, step);
            p_step->b_tie = false;
            if (draw >= (uint8_t)(DRAW_COUNT - skip))
            {
                p_step->note = 0U;
            }
            b_step = true;
        }
    }

    return b_step;
}
