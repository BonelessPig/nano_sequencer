/**
 * @file   env.c
 * @brief  Envelope implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "env.h"
#include <stddef.h>

// The time of a slope: the amount is brought to 8 bits, so that its square
// fits in 16, and the square brought down to milliseconds
#define AMOUNT_MAX       (1023U)
#define AMOUNT_PER_STEP  (4U)
#define SQUARE_PER_MS    (64U)

// An amplitude of 255 is 4095: 16 to a unit, and a sixteenth more
#define LEVEL_PER_AMPLITUDE (16U)



/**
 * @brief  Gives the time of a slope.
 * @param  amount  The amount that sets it; above AMOUNT_MAX is taken as
 *                 AMOUNT_MAX.
 * @return The time in milliseconds, 0 to 1016.
 */
static uint16_t slope_ms(uint16_t amount)
{
    const uint16_t limited = (amount > AMOUNT_MAX) ? (uint16_t)AMOUNT_MAX : amount;
    const uint16_t coarse  = (uint16_t)(limited / AMOUNT_PER_STEP);

    // At most 255 * 255, which fits in 16 bits
    return (uint16_t)((uint16_t)(coarse * coarse) / SQUARE_PER_MS);
}



/**
 * @brief  Gives the peak an amplitude asks for.
 * @param  amplitude  The amplitude, 0 to 255.
 * @return The peak, 0 to 4095.
 */
static uint16_t peak_level(uint8_t amplitude)
{
    const uint16_t wide = amplitude;

    // At most 255 * 16 + 15
    return (uint16_t)((uint16_t)(wide * LEVEL_PER_AMPLITUDE)
                      + (uint16_t)(wide / LEVEL_PER_AMPLITUDE));
}



/**
 * @brief  Gives the lesser of two values.
 * @param  first   One value.
 * @param  second  The other.
 * @return The lesser.
 */
static uint16_t lesser(uint16_t first, uint16_t second)
{
    return (first < second) ? first : second;
}



/**
 * @brief  Gives how far the level moves in one millisecond of the slope it
 *         is on, keeping what is left over for the next, so that a slope
 *         moves its whole span in its whole time.
 * @param  p_state  Envelope state; what is owed is updated.
 * @param  time     The time of a slope in milliseconds; not 0.
 * @return Units of level to move by.
 */
static uint16_t slope_move(env_state_t *p_state, uint16_t time)
{
    // What is owed is less than a time that was at most 1016, and the span
    // is at most 4095, so the sum fits in 16 bits
    const uint16_t owed = (uint16_t)(p_state->remainder + p_state->span);

    p_state->remainder = (uint16_t)(owed % time);

    return (uint16_t)(owed / time);
}



/**
 * @brief  Starts the decay from the peak.
 * @param  p_state  Envelope state; updated.
 */
static void begin_decay(env_state_t *p_state)
{
    p_state->phase     = ENV_DECAY;
    p_state->span      = p_state->peak;
    p_state->remainder = 0U;
}



/**
 * @brief  Starts the envelope for a note: an attack from the level as it
 *         is to the note's peak, which may be below it.
 * @param  p_state    Envelope state; updated.
 * @param  amplitude  How high the peak is, 0 to 255.
 */
static void begin_attack(env_state_t *p_state, uint8_t amplitude)
{
    const uint16_t peak  = peak_level(amplitude);
    const uint16_t level = p_state->level;

    p_state->peak      = peak;
    p_state->phase     = ENV_ATTACK;
    p_state->span      = (level < peak) ? (uint16_t)(peak - level) : (uint16_t)(level - peak);
    p_state->remainder = 0U;
}



/**
 * @brief  Moves the level on by one millisecond of the slope it is on.
 * @param  p_state  Envelope state; updated.
 * @param  time     The time of a slope in milliseconds; not 0.
 */
static void move_one_ms(env_state_t *p_state, uint16_t time)
{
    const uint16_t peak = p_state->peak;

    switch (p_state->phase)
    {
        case ENV_ATTACK:
        {
            const uint16_t move = slope_move(p_state, time);

            if (p_state->level < peak)
            {
                const uint16_t gap = (uint16_t)(peak - p_state->level);

                p_state->level = (uint16_t)(p_state->level + lesser(move, gap));
            }
            else
            {
                const uint16_t gap = (uint16_t)(p_state->level - peak);

                p_state->level = (uint16_t)(p_state->level - lesser(move, gap));
            }
            if (p_state->level == peak)
            {
                begin_decay(p_state);
            }
            break;
        }

        case ENV_DECAY:
        {
            const uint16_t move = slope_move(p_state, time);

            p_state->level = (uint16_t)(p_state->level - lesser(move, p_state->level));
            if (0U == p_state->level)
            {
                p_state->phase = ENV_IDLE;
            }
            break;
        }

        case ENV_IDLE:
        default:
            // Nothing under way
            break;
    }
}



/**
 * @brief  Puts the level where the gate has it, with no envelope: at the
 *         peak while it is open, ready to fall from there should a time be
 *         set, and at 0 while it is closed.
 * @param  p_state  Envelope state; updated.
 * @param  b_gate   Level of the gate.
 */
static void follow_gate(env_state_t *p_state, bool b_gate)
{
    if (b_gate)
    {
        p_state->level = p_state->peak;
        begin_decay(p_state);
    }
    else
    {
        p_state->level = 0U;
        p_state->phase = ENV_IDLE;
    }
}



void env_init(env_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->level     = 0U;
        p_state->peak      = 0U;
        p_state->span      = 0U;
        p_state->remainder = 0U;
        p_state->phase     = ENV_IDLE;
        p_state->b_gate    = false;
    }
}



uint16_t env_tick(env_state_t *p_state, const env_inputs_t *p_in)
{
    uint16_t level = 0U;

    if ((NULL != p_state) && (NULL != p_in))
    {
        const uint16_t time   = slope_ms(p_in->time);
        const bool     b_rose = p_in->b_gate && (!p_state->b_gate);

        p_state->b_gate = p_in->b_gate;
        if (b_rose)
        {
            begin_attack(p_state, p_in->amplitude);
        }

        if (0U == time)
        {
            follow_gate(p_state, p_in->b_gate);
        }
        else
        {
            for (uint8_t tick = 0U; tick < p_in->elapsed_ticks; tick++)
            {
                move_one_ms(p_state, time);
            }
        }

        level = p_state->level;
    }

    return level;
}
