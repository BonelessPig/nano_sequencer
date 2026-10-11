#ifndef ENGINE_KLEE_H
#define ENGINE_KLEE_H
/**
 * @file   engine_klee.h
 * @brief  The Klee engine: a pattern of bits going round a sixteen-stage
 *         register, the note being the sum of the values under them. Pure
 *         logic with no hardware access; compiles unchanged for the MCU and
 *         for a PC. See engine.h for the interface every engine follows.
 * @author BonelessPig
 *
 * Each of the sixteen panel steps is a stage. Its highest switch is its
 * pattern switch and its low three are its value, 0 to 7.
 *
 * A load copies the sixteen pattern switches into the register: stage n is
 * active if step n's pattern switch is on. The first step after
 * engine_klee_init() is a load, and so is the step after the load switch is
 * moved, either way. Every other step moves the register on one stage, the
 * bit leaving stage 15 coming round into stage 0.
 *
 * The note value of a step is the sum of the values of the active stages,
 * as the register then stands, limited to 15. A sum of 0 is a rest. The
 * values are read on every step, as they are then; the pattern switches
 * only at a load.
 *
 * Mode switches 3 to 5 (see panel.h) are this engine's own:
 *
 *   3  load:    moving it, on or off, loads the register on the next step.
 *   4  invert:  while on, the bit coming round into stage 0 is turned over,
 *               so a pattern comes back upside down after sixteen steps and
 *               as it was after thirty-two.
 *   5  merge:   while on, every note is tied to the step after it, so notes
 *               on consecutive steps are one long gate, and a rest ends it.
 *   6, 7  chance: as a number 0 to 3, how many times in eight the bit coming
 *               round is turned over, by a draw of the random numbers the
 *               engine is given. 0 leaves the pattern locked; anything else
 *               lets it drift, the less the slower. It is counted on top of
 *               invert.
 *
 * The step given is how many times the register has moved since the last
 * load, 0 to 15 and round again. The addressing controls are not used.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include "engine.h"

/**
 * @brief State the Klee engine carries from one step to the next.
 */
typedef struct
{
    uint16_t stages;        // The register: bit n is set while stage n is active
    uint8_t  position;      // Times it has moved since the last load, 0 to 15
    bool     b_loaded;      // false until the first load after init
    bool     b_load_switch; // The load switch as it was on the last step
} engine_klee_state_t;

/**
 * @brief  Goes back to the start of the pattern: the next step loads the
 *         register. This is both the power-on condition and what a reset
 *         does.
 * @param  p_state  State to initialize. Ignored if null.
 */
void engine_klee_init(engine_klee_state_t *p_state);

/**
 * @brief  Loads or moves the register and gives the step to play. Reads
 *         *p_in and the panel it points to, reads and writes *p_state,
 *         writes *p_step, and has no other effect.
 * @param  p_state  State from engine_klee_init() or the previous call; updated.
 * @param  p_in     The panel bits and the random numbers. The addressing
 *                  controls are not read and may be null.
 * @param  p_step   Receives the step. Written only when the return value is
 *                  true.
 * @return true if there is a step to play, which there always is; false if
 *         p_state, p_in, p_step or the panel bits are null. The register
 *         does not move when it returns false.
 */
bool engine_klee_step(engine_klee_state_t *p_state, const engine_inputs_t *p_in,
                      engine_step_t *p_step);

#endif /* ENGINE_KLEE_H */
