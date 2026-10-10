/**
 * @file   engine_plain.c
 * @brief  Plain engine implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "engine_plain.h"
#include <stddef.h>
#include "panel.h"



void engine_plain_init(engine_plain_state_t *p_state)
{
    if (NULL != p_state)
    {
        address_init(&p_state->address);
    }
}



bool engine_plain_step(engine_plain_state_t *p_state, const engine_inputs_t *p_in,
                       engine_step_t *p_step)
{
    bool b_step = false;

    // The addressing checks its own pointers; without the panel bits there
    // is no step to give, so the pattern must not move on either
    if ((NULL != p_state) && (NULL != p_in) && (NULL != p_step)
        && (NULL != p_in->p_raw_steps))
    {
        uint8_t step = 0U;

        if (address_next(&p_state->address, p_in->p_address, &step))
        {
            p_step->step = step;
            p_step->note = panel_step_value(p_in->p_raw_steps, step);
            b_step       = true;
        }
    }

    return b_step;
}
