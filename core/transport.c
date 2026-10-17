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
        // As if held, so that a button down at power-on is not a press
        p_state->b_pressed  = true;
        p_state->b_run      = true;
        p_state->b_starting = true;
        p_state->b_external = false;
        p_state->b_midi     = false;
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
        transport_mode_t mode = TRANSPORT_STOPPED;

        // Each press of the button turns running on or off
        if (b_pressed && (!p_state->b_pressed))
        {
            p_state->b_run      = !p_state->b_run;
            p_state->b_starting = p_state->b_run;
            if (!p_state->b_run)
            {
                // A stop forgets the external clock
                p_state->b_external = false;
            }
        }
        p_state->b_pressed = b_pressed;

        // A MIDI Stop is a stop as a press is one
        if (p_in->b_midi_stop)
        {
            p_state->b_run      = false;
            p_state->b_starting = false;
            p_state->b_external = false;
        }

        // A MIDI Start starts the pattern, running or not, on the MIDI clock
        if (p_in->b_midi_start)
        {
            p_state->b_run      = true;
            p_state->b_starting = true;
            p_state->b_external = true;
            p_state->b_midi     = true;
        }

        // An edge brings the external clock back, stopped or running: the
        // first of the two clocks to give one
        if (!p_state->b_external)
        {
            if (p_in->b_clock_edge)
            {
                p_state->b_external = true;
                p_state->b_midi     = false;
            }
            else if (p_in->b_midi_edge)
            {
                p_state->b_external = true;
                p_state->b_midi     = true;
            }
            else
            {
                // Still no external clock
            }
        }

        if (p_state->b_run)
        {
            mode = p_state->b_external ? TRANSPORT_EXTERNAL : TRANSPORT_INTERNAL;
        }

        p_out->mode       = mode;
        p_out->b_started  = p_state->b_starting;
        p_out->b_changed  = (mode != p_state->mode);
        p_out->b_external = p_state->b_external;
        p_out->b_edge     = p_state->b_external
                            && (p_state->b_midi ? p_in->b_midi_edge : p_in->b_clock_edge);

        p_state->b_starting = false;
        p_state->mode       = mode;
    }
}
