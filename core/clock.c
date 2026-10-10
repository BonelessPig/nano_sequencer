/**
 * @file   clock.c
 * @brief  Musical clock implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "clock.h"
#include <stddef.h>

// See clock.h: each tick adds bpm * PHASE_PER_BPM, and PHASE_PER_PULSE of
// phase is one pulse. At CLOCK_BPM_MAX a tick adds 2400, so the phase stays
// below 7400 before a pulse is taken out and a tick never holds two pulses.
#define PHASE_PER_BPM   (8U)
#define PHASE_PER_PULSE (5000U)



void clock_init(clock_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->phase = 0U;
    }
}



uint8_t clock_advance(clock_state_t *p_state, uint16_t bpm, uint8_t elapsed_ticks)
{
    uint8_t pulses = 0U;

    if (NULL != p_state)
    {
        uint16_t limited_bpm = bpm;
        uint16_t phase_per_tick;

        if (limited_bpm > CLOCK_BPM_MAX)
        {
            limited_bpm = (uint16_t)CLOCK_BPM_MAX;
        }
        phase_per_tick = (uint16_t)(limited_bpm * PHASE_PER_BPM);

        for (uint8_t tick = 0U; tick < elapsed_ticks; tick++)
        {
            p_state->phase = (uint16_t)(p_state->phase + phase_per_tick);
            if (p_state->phase >= PHASE_PER_PULSE)
            {
                p_state->phase = (uint16_t)(p_state->phase - PHASE_PER_PULSE);
                pulses++;
            }
        }
    }

    return pulses;
}
