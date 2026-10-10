/**
 * @file   test_clock.c
 * @brief  Host unit tests for the musical clock (core/clock.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "clock.h"
#include "test_harness.h"

#define POISON_BYTE      (0xA5)
#define TICKS_PER_MINUTE (60000UL)

/**
 * @brief Runs a fresh clock for a number of ticks, handed over in batches,
 *        and returns the pulses it emitted.
 */
static unsigned long count_pulses(uint16_t bpm, unsigned long ticks, uint8_t batch)
{
    clock_state_t state;
    unsigned long pulses    = 0UL;
    unsigned long remaining = ticks;

    clock_init(&state);
    while (remaining > 0UL)
    {
        const uint8_t step = (remaining < batch) ? (uint8_t)remaining : batch;

        pulses    += clock_advance(&state, bpm, step);
        remaining -= step;
    }
    return pulses;
}



static void test_init_clears_the_phase(void)
{
    clock_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    clock_init(&state);
    TEST_ASSERT_EQUAL(0, state.phase);
}



static void test_a_minute_holds_exactly_bpm_times_96_pulses(void)
{
    static const uint16_t tempos[] = { 1U, 30U, 59U, 60U, 119U, 120U, 125U, 285U, 300U };

    for (size_t i = 0U; i < (sizeof(tempos) / sizeof(tempos[0])); i++)
    {
        const unsigned long expected = (unsigned long)tempos[i] * CLOCK_PPQN;

        TEST_ASSERT_EQUAL(expected, count_pulses(tempos[i], TICKS_PER_MINUTE, 1U));
    }
}



static void test_ten_minutes_do_not_drift(void)
{
    // 125 BPM is 200 pulses a second: a pulse every 5 ticks, with no remainder
    TEST_ASSERT_EQUAL(125UL * CLOCK_PPQN * 10UL,
                      count_pulses(125U, TICKS_PER_MINUTE * 10UL, 1U));
    // 119 BPM does leave a remainder on most pulses; it must still add up
    TEST_ASSERT_EQUAL(119UL * CLOCK_PPQN * 10UL,
                      count_pulses(119U, TICKS_PER_MINUTE * 10UL, 1U));
}



static void test_batched_ticks_give_the_same_count(void)
{
    static const uint8_t batches[] = { 2U, 7U, 100U, 255U };

    for (size_t i = 0U; i < (sizeof(batches) / sizeof(batches[0])); i++)
    {
        TEST_ASSERT_EQUAL(120UL * CLOCK_PPQN,
                          count_pulses(120U, TICKS_PER_MINUTE, batches[i]));
    }
}



static void test_pulses_fall_on_the_expected_ticks(void)
{
    // 125 BPM adds 1000 a tick, so every fifth tick carries a pulse
    clock_state_t state;

    clock_init(&state);
    for (unsigned int tick = 1U; tick <= 20U; tick++)
    {
        const uint8_t expected = (0U == (tick % 5U)) ? 1U : 0U;

        TEST_ASSERT_EQUAL(expected, clock_advance(&state, 125U, 1U));
    }
}



static void test_remainder_is_kept_between_pulses(void)
{
    // 120 BPM adds 960 a tick: 5760 after six ticks, one pulse, 760 left over
    clock_state_t state;

    clock_init(&state);
    TEST_ASSERT_EQUAL(0, clock_advance(&state, 120U, 5U));
    TEST_ASSERT_EQUAL(4800, state.phase);
    TEST_ASSERT_EQUAL(1, clock_advance(&state, 120U, 1U));
    TEST_ASSERT_EQUAL(760, state.phase);
}



static void test_no_ticks_means_no_pulses_and_no_change(void)
{
    clock_state_t state;

    clock_init(&state);
    (void)clock_advance(&state, 120U, 3U);
    TEST_ASSERT_EQUAL(0, clock_advance(&state, 120U, 0U));
    TEST_ASSERT_EQUAL(2880, state.phase);
}



static void test_zero_bpm_stops_the_clock_and_keeps_the_phase(void)
{
    clock_state_t state;

    clock_init(&state);
    (void)clock_advance(&state, 120U, 3U);
    TEST_ASSERT_EQUAL(0, clock_advance(&state, 0U, 255U));
    TEST_ASSERT_EQUAL(2880, state.phase);
}



static void test_tempo_above_the_maximum_runs_at_the_maximum(void)
{
    const unsigned long at_maximum = (unsigned long)CLOCK_BPM_MAX * CLOCK_PPQN;

    TEST_ASSERT_EQUAL(at_maximum, count_pulses(CLOCK_BPM_MAX + 1U, TICKS_PER_MINUTE, 1U));
    TEST_ASSERT_EQUAL(at_maximum, count_pulses(UINT16_MAX, TICKS_PER_MINUTE, 1U));
}



static void test_null_state_is_ignored(void)
{
    clock_init(NULL); // Must not crash
    TEST_ASSERT_EQUAL(0, clock_advance(NULL, 120U, 255U));
}



int main(void)
{
    RUN_TEST(test_init_clears_the_phase);
    RUN_TEST(test_a_minute_holds_exactly_bpm_times_96_pulses);
    RUN_TEST(test_ten_minutes_do_not_drift);
    RUN_TEST(test_batched_ticks_give_the_same_count);
    RUN_TEST(test_pulses_fall_on_the_expected_ticks);
    RUN_TEST(test_remainder_is_kept_between_pulses);
    RUN_TEST(test_no_ticks_means_no_pulses_and_no_change);
    RUN_TEST(test_zero_bpm_stops_the_clock_and_keeps_the_phase);
    RUN_TEST(test_tempo_above_the_maximum_runs_at_the_maximum);
    RUN_TEST(test_null_state_is_ignored);
    return TEST_RESULT();
}
