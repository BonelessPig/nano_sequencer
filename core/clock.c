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

// Following external edges: the spacing assumed before one has been
// measured, and the gap at which a measurement is no longer believed
#define FIRST_PERIOD_TICKS (20U)
#define GAP_MAX_TICKS      (4000U)



/**
 * @brief  Stops following external edges: the next sync edge is the first.
 * @param  p_state  Clock state; updated.
 */
static void unsync(clock_state_t *p_state)
{
    p_state->b_synced   = false;
    p_state->period     = (uint16_t)FIRST_PERIOD_TICKS;
    p_state->sync_ticks = 0U;
    p_state->per_sync   = (uint8_t)CLOCK_PULSES_PER_EDGE;
    p_state->given      = 0U;
    p_state->edges_left = 0U;
}



/**
 * @brief  Says whether an edge is a sync edge, and counts it if it is not.
 * @param  p_state  Clock state; an edge passed over is counted off.
 * @param  b_edge   true if an edge arrived.
 * @return true if an edge arrived and it is the one the pattern locks to.
 */
static bool is_sync_edge(clock_state_t *p_state, bool b_edge)
{
    bool b_sync = false;

    if (b_edge)
    {
        if ((!p_state->b_synced) || (0U == p_state->edges_left))
        {
            b_sync = true;
        }
        else
        {
            p_state->edges_left--;
        }
    }

    return b_sync;
}



/**
 * @brief  Begins the stretch from this sync edge to the next: takes the
 *         spacing from the stretch just ended, if there was one and it was
 *         not too long, and the ratio from the inputs.
 * @param  p_state  Clock state; updated.
 * @param  p_in     The ratio.
 */
static void begin_stretch(clock_state_t *p_state, const clock_follow_t *p_in)
{
    const bool b_measured = p_state->b_synced && (p_state->sync_ticks > 0U)
                            && (p_state->sync_ticks < GAP_MAX_TICKS);
    uint8_t    divide     = p_in->divide;
    uint8_t    multiply   = p_in->multiply;

    if (0U == divide)
    {
        divide = 1U;
    }
    if (multiply > CLOCK_MULTIPLY_MAX)
    {
        multiply = (uint8_t)CLOCK_MULTIPLY_MAX;
    }
    // With no spacing to go by, steps cannot be fitted in between
    if ((0U == multiply) || (!b_measured))
    {
        multiply = 1U;
    }

    p_state->period = b_measured ? p_state->sync_ticks : (uint16_t)FIRST_PERIOD_TICKS;
    // At most 24 * 4, so it fits in a byte
    p_state->per_sync   = (uint8_t)(CLOCK_PULSES_PER_EDGE * multiply);
    p_state->given      = 0U;
    p_state->edges_left = (uint8_t)(divide - 1U);
    p_state->phase      = 0U;
    p_state->sync_ticks = 0U;
    p_state->b_synced   = true;
}



/**
 * @brief  Gives the pulses that fall due between sync edges, spaced by the
 *         measured period, and no more than there is room for.
 * @param  p_state        Clock state; the phase is updated.
 * @param  elapsed_ticks  Ticks since the previous call.
 * @param  room           Most pulses that may be given.
 * @return Pulses that fell due, 0 to room.
 */
static uint8_t spaced_pulses(clock_state_t *p_state, uint8_t elapsed_ticks, uint8_t room)
{
    const uint16_t per_sync = p_state->per_sync;
    uint8_t        pulses   = 0U;

    // The phase starts below 5000 and this adds at most 96 * 255, so the
    // sum fits in 16 bits
    p_state->phase = (uint16_t)(p_state->phase + (uint16_t)(per_sync * elapsed_ticks));

    while ((p_state->phase >= p_state->period) && (pulses < room))
    {
        p_state->phase = (uint16_t)(p_state->phase - p_state->period);
        pulses++;
    }
    if (pulses >= room)
    {
        // Nothing more can be given before the next sync edge
        p_state->phase = 0U;
    }

    return pulses;
}



void clock_init(clock_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->phase = 0U;
        unsync(p_state);
    }
}



uint8_t clock_advance(clock_state_t *p_state, uint16_t bpm, uint8_t elapsed_ticks)
{
    uint8_t pulses = 0U;

    if (NULL != p_state)
    {
        uint16_t limited_bpm = bpm;
        uint16_t phase_per_tick;

        if (p_state->b_synced)
        {
            unsync(p_state);
        }

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



uint8_t clock_follow(clock_state_t *p_state, const clock_follow_t *p_in)
{
    uint8_t pulses = 0U;

    if ((NULL != p_state) && (NULL != p_in))
    {
        // At most 4000 + 255, so it fits in 16 bits
        const uint16_t sync_ticks = (uint16_t)(p_state->sync_ticks + p_in->elapsed_ticks);

        p_state->sync_ticks = (sync_ticks < GAP_MAX_TICKS) ? sync_ticks
                                                           : (uint16_t)GAP_MAX_TICKS;

        if (is_sync_edge(p_state, p_in->b_edge))
        {
            // Whatever is left of this stretch, so that the totals are exact
            pulses = p_state->b_synced ? (uint8_t)(p_state->per_sync - p_state->given)
                                       : p_in->pulses_to_step;
            begin_stretch(p_state, p_in);
        }
        else if (p_state->b_synced)
        {
            // One is kept back for the sync edge to give
            const uint8_t room = (uint8_t)((p_state->per_sync - p_state->given) - 1U);

            pulses         = spaced_pulses(p_state, p_in->elapsed_ticks, room);
            p_state->given = (uint8_t)(p_state->given + pulses);
        }
        else if (p_in->pulses_to_step > 0U)
        {
            pulses = spaced_pulses(p_state, p_in->elapsed_ticks,
                                   (uint8_t)(p_in->pulses_to_step - 1U));
        }
        else
        {
            // The caller's step is already due: nothing to give before the edge
        }
    }

    return pulses;
}
