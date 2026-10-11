#ifndef PRNG_H
#define PRNG_H
/**
 * @file   prng.h
 * @brief  A small pseudo-random number generator, shared by every feature
 *         that leaves something to chance. Pure logic with no hardware
 *         access; compiles unchanged for the MCU and for a PC.
 * @author BonelessPig
 *
 * A 16-bit linear feedback shift register that goes through all 65535
 * non-zero values before it repeats. It is not random in any strong sense:
 * the same seed always gives the same numbers.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

/**
 * @brief State the generator carries from one number to the next.
 */
typedef struct
{
    uint16_t value; // The register; never 0 once initialized
} prng_state_t;

/**
 * @brief  Starts the generator from a seed.
 * @param  p_state  State to initialize. Ignored if null.
 * @param  seed     Any value. 0, which the register cannot hold, is taken as 1.
 */
void prng_init(prng_state_t *p_state, uint16_t seed);

/**
 * @brief  Gives the next eight bits and moves the generator on. Reads and
 *         writes *p_state and has no other effect.
 * @param  p_state  State from prng_init() or the previous call; updated.
 * @return A number from 0 to 255, each about as likely as the next. 0 if
 *         p_state is null.
 */
uint8_t prng_byte(prng_state_t *p_state);

#endif /* PRNG_H */
