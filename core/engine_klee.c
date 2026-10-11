/**
 * @file   engine_klee.c
 * @brief  Klee engine implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "engine_klee.h"
#include <stddef.h>
#include "panel.h"

// A stage's four switches: the pattern switch, then its value
#define PATTERN_BIT (0x08U)
#define VALUE_MASK  (0x07U)

// The mode switches that are this engine's own
#define MODE_LOAD   (0x08U)
#define MODE_INVERT (0x10U)
#define MODE_MERGE  (0x20U)
#define CHANCE_SHIFT (6U)   // Above it: turns in eight of the bit coming round
#define DRAW_MASK    (0x07U)
#define DRAW_COUNT   (8U)

#define NOTE_MAX   (15U)     // The highest note value there is
#define LAST_STAGE (0x8000U) // The register's bit for stage 15



/**
 * @brief  Gives the register as the pattern switches set it.
 * @param  p_raw_steps  The panel bits; not null.
 * @return Bit n set if step n's pattern switch is on.
 */
static uint16_t pattern(const uint8_t *p_raw_steps)
{
    uint16_t stages = 0U;
    uint16_t bit    = 1U; // The register's bit for the stage being looked at

    for (uint8_t stage = 0U; stage < PANEL_STEP_COUNT; stage++)
    {
        if (0U != (panel_step_value(p_raw_steps, stage) & PATTERN_BIT))
        {
            stages |= bit;
        }
        bit = (uint16_t)(bit << 1U);
    }

    return stages;
}



/**
 * @brief  Moves the register on one stage, the bit leaving stage 15 coming
 *         round into stage 0.
 * @param  stages    The register.
 * @param  b_invert  true to turn the bit over as it comes round.
 * @return The register after the move.
 */
static uint16_t moved_on(uint16_t stages, bool b_invert)
{
    const bool b_last  = (0U != (stages & LAST_STAGE));
    uint16_t   shifted = (uint16_t)(stages << 1U);

    if (b_last != b_invert)
    {
        shifted |= 1U;
    }

    return shifted;
}



/**
 * @brief  Adds up the values of the active stages.
 * @param  stages       The register.
 * @param  p_raw_steps  The panel bits; not null.
 * @return The sum, limited to NOTE_MAX; 0 if no active stage has a value.
 */
static uint8_t sum_of_active(uint16_t stages, const uint8_t *p_raw_steps)
{
    uint8_t  sum = 0U; // Sixteen values of 7 at most: 112
    uint16_t bit = 1U; // The register's bit for the stage being looked at

    for (uint8_t stage = 0U; stage < PANEL_STEP_COUNT; stage++)
    {
        if (0U != (stages & bit))
        {
            sum = (uint8_t)(sum + (panel_step_value(p_raw_steps, stage) & VALUE_MASK));
        }
        bit = (uint16_t)(bit << 1U);
    }
    if (sum > NOTE_MAX)
    {
        sum = (uint8_t)NOTE_MAX;
    }

    return sum;
}



void engine_klee_init(engine_klee_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->stages        = 0U;
        p_state->position      = 0U;
        p_state->b_loaded      = false;
        p_state->b_load_switch = false;
    }
}



bool engine_klee_step(engine_klee_state_t *p_state, const engine_inputs_t *p_in,
                      engine_step_t *p_step)
{
    bool b_step = false;

    if ((NULL != p_state) && (NULL != p_in) && (NULL != p_step)
        && (NULL != p_in->p_raw_steps))
    {
        const uint8_t mode          = panel_mode_value(p_in->p_raw_steps);
        const bool    b_load_switch = (0U != (mode & MODE_LOAD));

        if ((!p_state->b_loaded) || (b_load_switch != p_state->b_load_switch))
        {
            p_state->stages   = pattern(p_in->p_raw_steps);
            p_state->position = 0U;
        }
        else
        {
            // A draw of 0 to 7 turns the bit over if it is among the top
            // few, as many as the chance says; with a chance of 0, never
            const uint8_t chance  = (uint8_t)(mode >> CHANCE_SHIFT);
            const uint8_t draw    = (uint8_t)(prng_byte(p_in->p_prng) & DRAW_MASK);
            const bool    b_turn  = (draw >= (uint8_t)(DRAW_COUNT - chance));
            const bool    b_invert = (0U != (mode & MODE_INVERT));

            p_state->stages   = moved_on(p_state->stages, b_invert != b_turn);
            p_state->position = (uint8_t)((p_state->position + 1U) % PANEL_STEP_COUNT);
        }
        p_state->b_loaded      = true;
        p_state->b_load_switch = b_load_switch;

        p_step->step  = p_state->position;
        p_step->note  = sum_of_active(p_state->stages, p_in->p_raw_steps);
        p_step->b_tie = (0U != (mode & MODE_MERGE));
        b_step        = true;
    }

    return b_step;
}
