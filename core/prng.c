/**
 * @file   prng.c
 * @brief  Pseudo-random number generator implementation. Pure logic: no
 *         hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "prng.h"
#include <stdbool.h>
#include <stddef.h>

// Feedback taps for a 16-bit register of maximal length (bits 16, 14, 13, 11)
#define TAPS          (0xB400U)
#define BITS_PER_BYTE (8U)



void prng_init(prng_state_t *p_state, uint16_t seed)
{
    if (NULL != p_state)
    {
        p_state->value = seed;
        if (0U == seed)
        {
            p_state->value = 1U;
        }
    }
}



uint8_t prng_byte(prng_state_t *p_state)
{
    uint8_t byte = 0U;

    if (NULL != p_state)
    {
        for (uint8_t i = 0U; i < BITS_PER_BYTE; i++)
        {
            const bool b_out = (0U != (p_state->value & 1U));

            p_state->value = (uint16_t)(p_state->value >> 1U);
            byte           = (uint8_t)(byte << 1U);
            if (b_out)
            {
                p_state->value ^= (uint16_t)TAPS;
                byte           |= 1U;
            }
        }
    }

    return byte;
}
