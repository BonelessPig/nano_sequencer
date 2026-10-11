/**
 * @file   engine_cartesian.c
 * @brief  Cartesian engine implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "engine_cartesian.h"
#include <stddef.h>
#include "panel.h"

#define GRID_SIDE (4U) // Columns in a row, and rows

// The mode switches that are this engine's own: two for each counter
#define X_SHIFT      (3U)
#define Y_SHIFT      (5U)
#define SETTING_MASK (0x03U)



/**
 * @brief  Counts one step towards a counter's next move, and makes the move
 *         if it is due.
 * @param  p_position  The counter, 0 to 3; moved on, round to 0, when due.
 * @param  p_steps     Steps played since it last moved; updated.
 * @param  period      Steps between moves, 1 to 4.
 */
static void count_step(uint8_t *p_position, uint8_t *p_steps, uint8_t period)
{
    (*p_steps)++;
    if (*p_steps >= period)
    {
        *p_steps    = 0U;
        *p_position = (uint8_t)((uint8_t)(*p_position + 1U) % GRID_SIDE);
    }
}



void engine_cartesian_init(engine_cartesian_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->x       = 0U;
        p_state->y       = 0U;
        p_state->x_steps = 0U;
        p_state->y_steps = 0U;
    }
}



bool engine_cartesian_step(engine_cartesian_state_t *p_state, const engine_inputs_t *p_in,
                           engine_step_t *p_step)
{
    bool b_step = false;

    if ((NULL != p_state) && (NULL != p_in) && (NULL != p_step)
        && (NULL != p_in->p_raw_steps))
    {
        const uint8_t mode      = panel_mode_value(p_in->p_raw_steps);
        const uint8_t x_setting = (uint8_t)((uint8_t)(mode >> X_SHIFT) & SETTING_MASK);
        const uint8_t y_setting = (uint8_t)((uint8_t)(mode >> Y_SHIFT) & SETTING_MASK);
        const uint8_t step      = (uint8_t)((uint8_t)(p_state->y * GRID_SIDE) + p_state->x);

        p_step->step  = step;
        p_step->note  = panel_step_value(p_in->p_raw_steps, step);
        p_step->b_tie = false;

        // x counts up from every step, y down from every fourth
        count_step(&p_state->x, &p_state->x_steps, (uint8_t)(x_setting + 1U));
        count_step(&p_state->y, &p_state->y_steps, (uint8_t)(GRID_SIDE - y_setting));
        b_step = true;
    }

    return b_step;
}
