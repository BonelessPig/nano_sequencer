#ifndef ENGINE_STAGE_H
#define ENGINE_STAGE_H
/**
 * @file   engine_stage.h
 * @brief  The stage engine: eight stages, each with a pitch, a length in
 *         steps and a gate mode. Pure logic with no hardware access;
 *         compiles unchanged for the MCU and for a PC. See engine.h for the
 *         interface every engine follows.
 * @author BonelessPig
 *
 * The first eight panel steps are the stages' note values. The second eight
 * are their controls: step 8 belongs to stage 0, step 9 to stage 1, and so
 * on. A control's low two switches are the stage's length, 1 to 4 steps
 * (the switches as a number, plus one). Its high two are the gate mode:
 *
 *   0  single:  the note plays on the stage's first step, and the rest of
 *               its steps are rests.
 *   1  repeat:  the note plays again on every step of the stage.
 *   2  hold:    the note plays once and is held for the whole stage: every
 *               step but the last is tied to the one after it.
 *   3  rest:    every step of the stage is a rest.
 *
 * So with the second eight steps all off, this is a plain pattern of eight
 * steps. A stage whose note value is 0 is a rest in any mode.
 *
 * The stages are played in the order the addressing controls set (see
 * address.h), with the first and last step each taken as a stage, 0 to 7 (a
 * larger value wraps round, so a last step of 15 is stage 7).
 *
 * The offset (see engine.h) moves the stage to another of that range, by up
 * to half of it either way. It is taken when a stage begins, like the
 * stage's length, so a stage of several steps stays on one stage.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include "address.h"
#include "engine.h"

/**
 * @brief State the stage engine carries from one step to the next.
 */
typedef struct
{
    address_state_t address; // Which stage comes after this one
    uint8_t         stage;   // Stage being played, 0 to 7
    uint8_t         length;  // Steps it lasts, 1 to 4; 0 before the first stage
    uint8_t         played;  // Steps of it played so far, 0 to length
} engine_stage_state_t;

/**
 * @brief  Goes back to the start of the pattern: the next step is the first
 *         step of the first stage. This is both the power-on condition and
 *         what a reset does.
 * @param  p_state  State to initialize. Ignored if null.
 */
void engine_stage_init(engine_stage_state_t *p_state);

/**
 * @brief  Gives the step to play now and moves on to the one after it. Reads
 *         *p_in and what it points to, reads and writes *p_state, writes
 *         *p_step, and has no other effect.
 *
 *         The step given is the stage's own, 0 to 7, once for each step the
 *         stage lasts. A stage's length is read when its first step is
 *         asked for; its note value and gate mode are read on every step,
 *         as they are then.
 * @param  p_state  State from engine_stage_init() or the previous call; updated.
 * @param  p_in     The panel bits and the addressing controls.
 * @param  p_step   Receives the step. Written only when the return value is
 *                  true.
 * @return true if there is a step to play; false if a one-shot pattern has
 *         finished, or if any pointer is null, those in *p_in included. The
 *         pattern does not move on when it returns false.
 */
bool engine_stage_step(engine_stage_state_t *p_state, const engine_inputs_t *p_in,
                       engine_step_t *p_step);

#endif /* ENGINE_STAGE_H */
