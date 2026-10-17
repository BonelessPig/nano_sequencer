/**
 * @file   midi_in.c
 * @brief  MIDI in implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "midi_in.h"
#include <stddef.h>

#define MIDI_IN_CLOCK    (0xF8U) // Timing clock, 24 to a quarter note
#define MIDI_IN_START    (0xFAU)
#define MIDI_IN_CONTINUE (0xFBU)
#define MIDI_IN_STOP     (0xFCU)

// Clocks passed over between one edge and the next: six to a step, less the
// edge itself
#define CLOCKS_BETWEEN_EDGES (5U)



/**
 * @brief  Takes one byte: counts a clock, notes a Start or a Stop, and
 *         passes over anything else.
 * @param  p_state  MIDI in state; the count of clocks moves on.
 * @param  byte     The byte received.
 * @param  p_out    The events so far in this call; updated.
 */
static void take_byte(midi_in_state_t *p_state, uint8_t byte, midi_in_events_t *p_out)
{
    switch (byte)
    {
        case MIDI_IN_CLOCK:
            if (0U == p_state->clocks_to_edge)
            {
                p_out->b_edge           = true;
                p_state->clocks_to_edge = CLOCKS_BETWEEN_EDGES;
            }
            else
            {
                p_state->clocks_to_edge--;
            }
            break;

        case MIDI_IN_START:
        case MIDI_IN_CONTINUE:
            // The pattern cannot be taken up where it stopped, so a Continue
            // starts it as a Start does, on the clock that comes next
            p_out->b_start          = true;
            p_out->b_stop           = false;
            p_out->b_edge           = false;
            p_state->clocks_to_edge = 0U;
            break;

        case MIDI_IN_STOP:
            p_out->b_stop  = true;
            p_out->b_start = false;
            break;

        default:
            // A byte of a note or of any other message: passed over
            break;
    }
}



void midi_in_init(midi_in_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->clocks_to_edge = 0U;
    }
}



void midi_in_read(midi_in_state_t *p_state, const uint8_t *p_bytes, uint8_t count,
                  midi_in_events_t *p_out)
{
    if ((NULL != p_state) && (NULL != p_out))
    {
        p_out->b_edge  = false;
        p_out->b_start = false;
        p_out->b_stop  = false;

        if (NULL != p_bytes)
        {
            for (uint8_t i = 0U; i < count; i++)
            {
                take_byte(p_state, p_bytes[i], p_out);
            }
        }
    }
}
