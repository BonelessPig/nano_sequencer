#ifndef ENGINE_EUCLID_H
#define ENGINE_EUCLID_H
/**
 * @file   engine_euclid.h
 * @brief  The Euclidean engine: the plain engine's sixteen steps, sounded
 *         only on a number of beats spread evenly over a bar of sixteen.
 *         Pure logic with no hardware access; compiles unchanged for the
 *         MCU and for a PC. See engine.h for the interface every engine
 *         follows.
 * @author BonelessPig
 *
 * Each step's four switches are its note value, and the steps are played in
 * the order the addressing controls set (see address.h), as for the plain
 * engine. Beside that the engine counts a bar of sixteen steps, and a step
 * sounds only if it falls on a beat; any other is a rest.
 *
 * Mode switches 3 to 6 (see panel.h), as a number 0 to 15, set the beats in
 * a bar: one more than the number, 1 to 16. They are spread as evenly as
 * sixteen steps allow, the first on the first step of the bar: 4 beats are
 * every fourth step, 5 are steps 0, 4, 7, 10 and 13, and 16 are every step,
 * which is the plain engine. The setting is read on every step.
 *
 * The bar is counted apart from the pattern, so a range that is not 16, 8,
 * 4, 2 or 1 steps long moves against it and a step sounds on some cycles
 * and not on others.
 *
 * The offset (see engine.h) moves the step to another of the range, as for
 * the plain engine: which step's note a beat sounds, never which steps of
 * the bar are beats.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include "address.h"
#include "engine.h"

/**
 * @brief State the Euclidean engine carries from one step to the next.
 */
typedef struct
{
    address_state_t address; // Where the pattern has got to
    uint8_t         beat;    // Where the bar has got to, 0 to 15
} engine_euclid_state_t;

/**
 * @brief  Goes back to the start of the pattern and of the bar. This is
 *         both the power-on condition and what a reset does.
 * @param  p_state  State to initialize. Ignored if null.
 */
void engine_euclid_init(engine_euclid_state_t *p_state);

/**
 * @brief  Gives the step to play now and moves on. Reads *p_in and what it
 *         points to, reads and writes *p_state, writes *p_step, and has no
 *         other effect.
 * @param  p_state  State from engine_euclid_init() or the previous call; updated.
 * @param  p_in     The panel bits and the addressing controls.
 * @param  p_step   Receives the step. Written only when the return value is
 *                  true.
 * @return true if there is a step to play; false if a one-shot pattern has
 *         finished, or if p_state, p_in, p_step, the panel bits or the
 *         addressing controls are null. Neither the pattern nor the bar
 *         moves on when it returns false.
 */
bool engine_euclid_step(engine_euclid_state_t *p_state, const engine_inputs_t *p_in,
                        engine_step_t *p_step);

#endif /* ENGINE_EUCLID_H */
