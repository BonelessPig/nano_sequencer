#ifndef ENGINE_CHANCE_H
#define ENGINE_CHANCE_H
/**
 * @file   engine_chance.h
 * @brief  The chance engine: the plain engine's sixteen steps, with some of
 *         it left to chance. Pure logic with no hardware access; compiles
 *         unchanged for the MCU and for a PC. See engine.h for the interface
 *         every engine follows.
 * @author BonelessPig
 *
 * Each step's four switches are its note value, and the addressing controls
 * (see address.h) set the range and how many steps make a cycle, as for the
 * plain engine. Mode switches 3 to 5 (see panel.h) are this engine's own:
 *
 *   3, 4  skip:     as a number 0 to 3, how many times in four a step is
 *                   turned into a rest: never, 1, 2 or 3 in 4.
 *   5     shuffle:  while on, each step is one picked from the range at
 *                   random, in place of the next in order.
 *
 * With all three off it is the plain engine. Both draw from the random
 * numbers the engine is given; with none given, nothing is skipped and a
 * shuffled step is always the first of the range.
 *
 * The offset (see engine.h) moves the step to another of the range, as for
 * the plain engine, a shuffled one too, before the skip is drawn.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include "address.h"
#include "engine.h"

/**
 * @brief State the chance engine carries from one step to the next.
 */
typedef struct
{
    address_state_t address; // Where the pattern has got to in its cycle
} engine_chance_state_t;

/**
 * @brief  Goes back to the start of the pattern. This is both the power-on
 *         condition and what a reset does. The random numbers are not
 *         started again.
 * @param  p_state  State to initialize. Ignored if null.
 */
void engine_chance_init(engine_chance_state_t *p_state);

/**
 * @brief  Gives the step to play now and moves on. Reads *p_in and what it
 *         points to, reads and writes *p_state, moves the random numbers
 *         on, writes *p_step, and has no other effect.
 * @param  p_state  State from engine_chance_init() or the previous call; updated.
 * @param  p_in     The panel bits, the addressing controls and the random
 *                  numbers.
 * @param  p_step   Receives the step. Written only when the return value is
 *                  true.
 * @return true if there is a step to play; false if a one-shot pattern has
 *         finished, or if p_state, p_in, p_step, the panel bits or the
 *         addressing controls are null. The pattern does not move on when
 *         it returns false.
 */
bool engine_chance_step(engine_chance_state_t *p_state, const engine_inputs_t *p_in,
                        engine_step_t *p_step);

#endif /* ENGINE_CHANCE_H */
