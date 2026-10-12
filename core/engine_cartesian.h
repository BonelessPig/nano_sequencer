#ifndef ENGINE_CARTESIAN_H
#define ENGINE_CARTESIAN_H
/**
 * @file   engine_cartesian.h
 * @brief  The Cartesian engine: the sixteen steps as a grid of four by
 *         four, crossed by two counters moving at their own rates. Pure
 *         logic with no hardware access; compiles unchanged for the MCU and
 *         for a PC. See engine.h for the interface every engine follows.
 * @author BonelessPig
 *
 * Step n is at column n modulo 4 of row n / 4, and its four switches are
 * its note value. The step played is the one at column x of row y. Both
 * start at 0, and each moves on by one, round from 3 to 0, every so many
 * steps. Mode switches 3 to 6 (see panel.h) set how many:
 *
 *   3, 4  x:  as a number 0 to 3, x moves every 1, 2, 3 or 4 steps.
 *   5, 6  y:  as a number 0 to 3, y moves every 4, 3, 2 or 1 steps.
 *
 * With all four off, x moves on every step and y every fourth, which is
 * the sixteen steps in order. Other settings trace other paths over the
 * grid, some of which never reach every step. A setting is read on every
 * step. The addressing controls are not used.
 *
 * The offset (see engine.h) moves the step round the sixteen, by up to 8
 * either way, its number being the row times four plus the column. x and y
 * are not moved.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include "engine.h"

/**
 * @brief State the Cartesian engine carries from one step to the next.
 */
typedef struct
{
    uint8_t x;       // Column of the step to play, 0 to 3
    uint8_t y;       // Its row, 0 to 3
    uint8_t x_steps; // Steps played since x last moved
    uint8_t y_steps; // Steps played since y last moved
} engine_cartesian_state_t;

/**
 * @brief  Goes back to the start of the pattern: step 0, with both counters
 *         at the beginning of their count. This is both the power-on
 *         condition and what a reset does.
 * @param  p_state  State to initialize. Ignored if null.
 */
void engine_cartesian_init(engine_cartesian_state_t *p_state);

/**
 * @brief  Gives the step to play now and moves the counters on. Reads *p_in
 *         and the panel it points to, reads and writes *p_state, writes
 *         *p_step, and has no other effect.
 * @param  p_state  State from engine_cartesian_init() or the previous call;
 *                  updated.
 * @param  p_in     The panel bits. Nothing else in it is read.
 * @param  p_step   Receives the step. Written only when the return value is
 *                  true.
 * @return true if there is a step to play, which there always is; false if
 *         p_state, p_in, p_step or the panel bits are null. The counters do
 *         not move when it returns false.
 */
bool engine_cartesian_step(engine_cartesian_state_t *p_state, const engine_inputs_t *p_in,
                           engine_step_t *p_step);

#endif /* ENGINE_CARTESIAN_H */
