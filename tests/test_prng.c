/**
 * @file   test_prng.c
 * @brief  Host unit tests for the pseudo-random generator (core/prng.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "prng.h"
#include "test_harness.h"

#define POISON_BYTE (0xA5)
#define SEED        (0xACE1U)
#define PERIOD      (65535UL) // Every value a 16-bit register can hold but 0



static void test_init_takes_the_seed_and_never_zero(void)
{
    prng_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    prng_init(&state, SEED);
    TEST_ASSERT_EQUAL(SEED, state.value);

    prng_init(&state, 0U);
    TEST_ASSERT_EQUAL(1, state.value);

    prng_init(NULL, SEED); // Must not crash
}



static void test_bytes_match_a_sequence_worked_out_elsewhere(void)
{
    // From a few lines of Python doing the same shifts, not from this code
    static const uint8_t from_seed[] = { 135U, 35U, 70U, 220U, 176U, 221U, 238U, 248U };
    static const uint8_t from_one[]  = { 128U, 22U, 130U, 40U };
    prng_state_t         state;

    prng_init(&state, SEED);
    for (size_t i = 0U; i < sizeof(from_seed); i++)
    {
        TEST_ASSERT_EQUAL(from_seed[i], prng_byte(&state));
    }

    prng_init(&state, 1U);
    for (size_t i = 0U; i < sizeof(from_one); i++)
    {
        TEST_ASSERT_EQUAL(from_one[i], prng_byte(&state));
    }
    TEST_ASSERT_EQUAL(0x137B, state.value);
}



static void test_it_repeats_only_after_every_value_and_spreads_evenly(void)
{
    static unsigned int counts[256];
    prng_state_t        state;
    unsigned long       bytes = 0UL;
    bool                b_zero = false;

    (void)memset(counts, 0, sizeof(counts));
    prng_init(&state, SEED);
    do
    {
        counts[prng_byte(&state)]++;
        bytes++;
        b_zero = b_zero || (0U == state.value);
    } while ((SEED != state.value) && (bytes <= PERIOD));

    // Eight shifts a byte and 65535 states: back at the seed after exactly
    // 65535 bytes, having been everywhere, with every byte value 256 times
    // but 0, which is one short
    TEST_ASSERT_EQUAL(PERIOD, bytes);
    TEST_ASSERT(!b_zero);
    TEST_ASSERT_EQUAL(255, counts[0]);
    for (size_t value = 1U; value < 256U; value++)
    {
        TEST_ASSERT_EQUAL(256, counts[value]);
    }
}



static void test_null_state_gives_zero(void)
{
    TEST_ASSERT_EQUAL(0, prng_byte(NULL));
}



int main(void)
{
    RUN_TEST(test_init_takes_the_seed_and_never_zero);
    RUN_TEST(test_bytes_match_a_sequence_worked_out_elsewhere);
    RUN_TEST(test_it_repeats_only_after_every_value_and_spreads_evenly);
    RUN_TEST(test_null_state_gives_zero);
    return TEST_RESULT();
}
