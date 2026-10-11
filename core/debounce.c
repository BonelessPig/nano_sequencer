/**
 * @file   debounce.c
 * @brief  Debounce implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "debounce.h"
#include <stddef.h>

// Ticks a new raw level must hold before it becomes the settled level
#define SETTLE_TICKS (20U)



void debounce_init(debounce_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->b_known = false;
        p_state->b_level = false;
        p_state->ticks   = 0U;
    }
}



bool debounce_update(debounce_state_t *p_state, bool b_raw, uint8_t elapsed_ticks)
{
    bool b_level = false;

    if (NULL != p_state)
    {
        // At most 19 + 255, so it fits in 16 bits
        const uint16_t ticks = (uint16_t)((uint16_t)p_state->ticks + elapsed_ticks);

        if ((!p_state->b_known) || (ticks >= SETTLE_TICKS))
        {
            p_state->b_known = true;
            p_state->b_level = b_raw;
            p_state->ticks   = 0U;
        }
        else if (b_raw == p_state->b_level)
        {
            p_state->ticks = 0U;
        }
        else
        {
            // Below SETTLE_TICKS, so it fits in a byte
            p_state->ticks = (uint8_t)ticks;
        }
        b_level = p_state->b_level;
    }

    return b_level;
}
