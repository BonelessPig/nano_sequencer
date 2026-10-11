/**
 * @file   engine_stage.c
 * @brief  Stage engine implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "engine_stage.h"
#include <stddef.h>
#include "panel.h"

#define STAGE_COUNT (8U) // Stages; stage n's control is panel step n + 8

// A stage's control: its length less one in the low two switches, its gate
// mode in the high two
#define LENGTH_MASK (0x03U)
#define MODE_SHIFT  (2U)

#define MODE_SINGLE (0U) // The note on the first step only
#define MODE_REPEAT (1U) // The note again on every step
#define MODE_HOLD   (2U) // One note, tied from step to step
                         // 3 is a rest on every step



/**
 * @brief  Moves on to the next stage in the addressing order and reads its
 *         length.
 * @param  p_state  Engine state; not null. Left as it was if there is no
 *                  next stage.
 * @param  p_in     The panel bits and the addressing controls; neither null.
 * @return true if there is a stage to play; false if a one-shot pattern has
 *         finished.
 */
static bool next_stage(engine_stage_state_t *p_state, const engine_inputs_t *p_in)
{
    address_config_t stages;
    uint8_t          step = 0U;
    bool             b_stage;

    // The range is taken as stages, and a first stage above the last runs
    // round through stage 7: the last is put 8 higher, and the step that
    // comes back is folded into 0 to 7
    stages.direction  = p_in->p_address->direction;
    stages.first_step = (uint8_t)(p_in->p_address->first_step % STAGE_COUNT);
    stages.last_step  = (uint8_t)(p_in->p_address->last_step % STAGE_COUNT);
    stages.b_one_shot = p_in->p_address->b_one_shot;
    if (stages.last_step < stages.first_step)
    {
        stages.last_step = (uint8_t)(stages.last_step + STAGE_COUNT);
    }

    b_stage = address_next(&p_state->address, &stages, &step);
    if (b_stage)
    {
        const uint8_t stage   = (uint8_t)(step % STAGE_COUNT);
        const uint8_t control =
            panel_step_value(p_in->p_raw_steps, (uint8_t)(stage + STAGE_COUNT));

        p_state->stage  = stage;
        p_state->length = (uint8_t)((uint8_t)(control & LENGTH_MASK) + 1U);
        p_state->played = 0U;
    }

    return b_stage;
}



/**
 * @brief  Fills in the step of the current stage that is to play now, from
 *         the stage's note value and gate mode as they are read now.
 * @param  p_state  Engine state, with steps of the stage still to play; not
 *                  null.
 * @param  p_in     The panel bits; not null.
 * @param  p_step   Receives the step; not null.
 */
static void stage_step(const engine_stage_state_t *p_state, const engine_inputs_t *p_in,
                       engine_step_t *p_step)
{
    const uint8_t note    = panel_step_value(p_in->p_raw_steps, p_state->stage);
    const uint8_t control =
        panel_step_value(p_in->p_raw_steps, (uint8_t)(p_state->stage + STAGE_COUNT));
    const uint8_t mode    = (uint8_t)(control >> MODE_SHIFT);
    const bool    b_first = (0U == p_state->played);
    const bool    b_last  = ((uint8_t)(p_state->played + 1U) >= p_state->length);

    p_step->step  = p_state->stage;
    p_step->note  = 0U;
    p_step->b_tie = false;

    switch (mode)
    {
        case MODE_SINGLE:
            if (b_first)
            {
                p_step->note = note;
            }
            break;

        case MODE_REPEAT:
            p_step->note = note;
            break;

        case MODE_HOLD:
            p_step->note  = note;
            p_step->b_tie = !b_last;
            break;

        default:
            // A rest on every step
            break;
    }
}



void engine_stage_init(engine_stage_state_t *p_state)
{
    if (NULL != p_state)
    {
        address_init(&p_state->address);
        p_state->stage  = 0U;
        p_state->length = 0U;
        p_state->played = 0U;
    }
}



bool engine_stage_step(engine_stage_state_t *p_state, const engine_inputs_t *p_in,
                       engine_step_t *p_step)
{
    bool b_step = false;

    if ((NULL != p_state) && (NULL != p_in) && (NULL != p_step)
        && (NULL != p_in->p_raw_steps) && (NULL != p_in->p_address))
    {
        // A stage that has had all its steps gives way to the next
        b_step = true;
        if (p_state->played >= p_state->length)
        {
            b_step = next_stage(p_state, p_in);
        }

        if (b_step)
        {
            stage_step(p_state, p_in, p_step);
            p_state->played++;
        }
    }

    return b_step;
}
