/**
 * @file   gate.c
 * @brief  Gate implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "gate.h"
#include <stddef.h>



void gate_init(gate_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->pulses_left = 0U;
    }
}



void gate_begin(gate_state_t *p_state, const gate_config_t *p_config, bool b_note)
{
    if (NULL != p_state)
    {
        // The boundary ends whatever gate was open; only a note opens another
        p_state->pulses_left = 0U;
        if ((NULL != p_config) && b_note)
        {
            p_state->pulses_left = p_config->length;
        }
    }
}



bool gate_advance(gate_state_t *p_state, uint16_t pulses)
{
    bool b_open = false;

    if (NULL != p_state)
    {
        if (pulses >= p_state->pulses_left)
        {
            p_state->pulses_left = 0U;
        }
        else
        {
            // Smaller than pulses_left, so it fits in a byte
            p_state->pulses_left = (uint8_t)(p_state->pulses_left - pulses);
        }
        b_open = (p_state->pulses_left > 0U);
    }

    return b_open;
}
