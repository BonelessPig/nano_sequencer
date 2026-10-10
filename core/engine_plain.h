#ifndef ENGINE_PLAIN_H
#define ENGINE_PLAIN_H
/**
 * @file   engine_plain.h
 * @brief  The plain engine: sixteen steps, each one's four switches being its
 *         note value, played in the order the addressing controls set. Pure
 *         logic with no hardware access; compiles unchanged for the MCU and
 *         for a PC. See engine.h for the interface every engine follows.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include "address.h"
#include "engine.h"

/**
 * @brief State the plain engine carries from one step to the next.
 */
typedef struct
{
    address_state_t address; // Where the pattern has got to
} engine_plain_state_t;

/**
 * @brief  Goes back to the start of the pattern. This is both the power-on
 *         condition and what a reset does.
 * @param  p_state  State to initialize. Ignored if null.
 */
void engine_plain_init(engine_plain_state_t *p_state);

/**
 * @brief  Gives the step to play now and moves on to the one after it. Reads
 *         *p_in and what it points to, reads and writes *p_state, writes
 *         *p_step, and has no other effect.
 *
 *         The step is the next one in the addressing order (see address.h),
 *         and its note value is that step's four panel switches as a number,
 *         read as they are now.
 * @param  p_state  State from engine_plain_init() or the previous call; updated.
 * @param  p_in     The panel bits and the addressing controls.
 * @param  p_step   Receives the step. Written only when the return value is
 *                  true.
 * @return true if there is a step to play; false if a one-shot pattern has
 *         finished, or if any pointer is null, those in *p_in included. The
 *         pattern does not move on when it returns false.
 */
bool engine_plain_step(engine_plain_state_t *p_state, const engine_inputs_t *p_in,
                       engine_step_t *p_step);

#endif /* ENGINE_PLAIN_H */
