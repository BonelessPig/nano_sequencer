/**
 * @file   transport.c
 * @brief  Transport implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "transport.h"
#include <stddef.h>



void transport_init(transport_state_t *p_state)
{
    if (NULL != p_state)
    {
        debounce_init(&p_state->run_button);
        debounce_init(&p_state->sync_switch);
        // As if held, so that a button down at power-on is not a press
        p_state->b_pressed  = true;
        p_state->b_run      = true;
        p_state->b_starting = true;
        p_state->mode       = TRANSPORT_INTERNAL;
    }
}



void transport_tick(transport_state_t *p_state, const transport_inputs_t *p_in,
                    transport_outputs_t *p_out)
{
    if ((NULL != p_state) && (NULL != p_in) && (NULL != p_out))
    {
        const bool b_pressed = debounce_update(&p_state->run_button, p_in->b_run_button,
                                               p_in->elapsed_ticks);
        const bool b_sync    = debounce_update(&p_state->sync_switch, p_in->b_sync_switch,
                                               p_in->elapsed_ticks);
        transport_mode_t mode = TRANSPORT_STOPPED;

        // Each press of the button turns running on or off
        if (b_pressed && (!p_state->b_pressed))
        {
            p_state->b_run      = !p_state->b_run;
            p_state->b_starting = p_state->b_run;
        }
        p_state->b_pressed = b_pressed;

        if (p_state->b_run)
        {
            mode = b_sync ? TRANSPORT_EXTERNAL : TRANSPORT_INTERNAL;
        }

        p_out->mode       = mode;
        p_out->b_started  = p_state->b_starting;
        p_out->b_changed  = (mode != p_state->mode);
        p_out->b_external = b_sync;

        p_state->b_starting = false;
        p_state->mode       = mode;
    }
}
