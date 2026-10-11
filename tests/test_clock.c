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



// Following external edges. The numbers are those clock.h gives
#define EDGE_PULSES   (24U)   // Pulses to an edge at a ratio of one
#define FIRST_PERIOD  (20U)   // Ticks between edges assumed before a measurement
#define GAP_MAX       (4000U) // Gap at which a measurement is not believed

static clock_state_t g_clock;

/**
 * @brief One call of clock_follow() on the shared clock.
 */
static uint8_t follow(bool b_edge, uint8_t elapsed_ticks, uint8_t divide, uint8_t multiply,
                      uint8_t pulses_to_step)
{
    clock_follow_t in;

    in.b_edge         = b_edge;
    in.elapsed_ticks  = elapsed_ticks;
    in.divide         = divide;
    in.multiply       = multiply;
    in.pulses_to_step = pulses_to_step;
    return clock_follow(&g_clock, &in);
}

/**
 * @brief A number of single ticks with no edge; returns the pulses given.
 */
static unsigned int quiet(unsigned int ticks, uint8_t divide, uint8_t multiply)
{
    unsigned int pulses = 0U;

    for (unsigned int tick = 0U; tick < ticks; tick++)
    {
        pulses += follow(false, 1U, divide, multiply, (uint8_t)EDGE_PULSES);
    }
    return pulses;
}

/**
 * @brief One tick with an edge on it; returns the pulses given.
 */
static unsigned int edge(uint8_t divide, uint8_t multiply)
{
    return follow(true, 1U, divide, multiply, (uint8_t)EDGE_PULSES);
}

/**
 * @brief Starts a fresh clock and gives it two edges a period apart, so that
 *        it is in a stretch whose spacing has been measured.
 */
static void lock(unsigned int period, uint8_t divide, uint8_t multiply)
{
    clock_init(&g_clock);
    (void)edge(divide, multiply);
    for (unsigned int passed = 1U; passed < divide; passed++)
    {
        (void)quiet(period - 1U, divide, multiply);
        (void)edge(divide, multiply);
    }
    (void)quiet(period - 1U, divide, multiply);
    (void)edge(divide, multiply);
}



static void test_init_follows_nothing(void)
{
    (void)memset(&g_clock, POISON_BYTE, sizeof(g_clock));
    clock_init(&g_clock);
    TEST_ASSERT(!g_clock.b_synced);
    TEST_ASSERT_EQUAL(FIRST_PERIOD, g_clock.period);
    TEST_ASSERT_EQUAL(0, g_clock.sync_ticks);
    TEST_ASSERT_EQUAL(EDGE_PULSES, g_clock.per_sync);
    TEST_ASSERT_EQUAL(0, g_clock.given);
    TEST_ASSERT_EQUAL(0, g_clock.edges_left);
}



static void test_the_first_edge_brings_the_callers_step_due(void)
{
    clock_init(&g_clock);
    TEST_ASSERT_EQUAL(7, follow(true, 1U, 1U, 1U, 7U));
    TEST_ASSERT(g_clock.b_synced);

    clock_init(&g_clock);
    TEST_ASSERT_EQUAL(0, follow(true, 1U, 1U, 1U, 0U));

    // Whatever the ratio: a divided clock still starts on its first edge
    clock_init(&g_clock);
    TEST_ASSERT_EQUAL(24, follow(true, 1U, 4U, 1U, 24U));
}



static void test_before_the_first_edge_the_step_is_finished_and_no_more(void)
{
    uint8_t      to_step = 24U;
    unsigned int total   = 0U;

    clock_init(&g_clock);
    for (unsigned int tick = 1U; tick <= 100U; tick++)
    {
        const uint8_t pulses = follow(false, 1U, 1U, 1U, to_step);

        to_step = (uint8_t)(to_step - pulses);
        total  += pulses;
        if (10U == tick)
        {
            TEST_ASSERT_EQUAL(12, total); // 24 pulses to every 20 ticks
        }
    }
    TEST_ASSERT_EQUAL(23, total); // One short: the edge brings the step
    TEST_ASSERT(!g_clock.b_synced);

    // A step that is already due gets nothing
    clock_init(&g_clock);
    TEST_ASSERT_EQUAL(0, follow(false, 255U, 1U, 1U, 0U));
    TEST_ASSERT_EQUAL(0, follow(false, 255U, 1U, 1U, 1U));
}



static void test_each_stretch_between_edges_holds_exactly_24_pulses(void)
{
    static const unsigned int periods[] = { 1U, 2U, 7U, 24U, 53U, 100U, 500U, 3999U };

    for (size_t i = 0U; i < (sizeof(periods) / sizeof(periods[0])); i++)
    {
        lock(periods[i], 1U, 1U);
        for (unsigned int stretch = 0U; stretch < 5U; stretch++)
        {
            const unsigned int spaced = quiet(periods[i] - 1U, 1U, 1U);

            TEST_ASSERT_EQUAL(EDGE_PULSES, spaced + edge(1U, 1U));
        }
        TEST_ASSERT_EQUAL(periods[i], g_clock.period);
    }
}



static void test_pulses_are_spaced_by_the_measured_period(void)
{
    lock(100U, 1U, 1U);
    TEST_ASSERT_EQUAL(0, quiet(4U, 1U, 1U));   // 96 of phase, not yet 100
    TEST_ASSERT_EQUAL(1, quiet(1U, 1U, 1U));
    TEST_ASSERT_EQUAL(11, quiet(45U, 1U, 1U)); // Half the pulses by half way
    TEST_ASSERT_EQUAL(11, quiet(49U, 1U, 1U));
    TEST_ASSERT_EQUAL(1, edge(1U, 1U));        // The edge gives the last
}



static void test_the_first_stretch_is_spaced_as_twenty_ticks(void)
{
    clock_init(&g_clock);
    TEST_ASSERT_EQUAL(24, edge(1U, 1U));
    TEST_ASSERT_EQUAL(12, quiet(10U, 1U, 1U));
    TEST_ASSERT_EQUAL(11, quiet(89U, 1U, 1U));
    TEST_ASSERT_EQUAL(1, edge(1U, 1U));
    TEST_ASSERT_EQUAL(100, g_clock.period);
}



static void test_a_late_edge_is_waited_for_one_pulse_short(void)
{
    lock(100U, 1U, 1U);
    TEST_ASSERT_EQUAL(23, quiet(99U, 1U, 1U));
    TEST_ASSERT_EQUAL(0, quiet(50U, 1U, 1U));
    TEST_ASSERT_EQUAL(1, edge(1U, 1U));
    TEST_ASSERT_EQUAL(150, g_clock.period);
}



static void test_an_early_edge_gives_all_that_is_left(void)
{
    lock(100U, 1U, 1U);
    TEST_ASSERT_EQUAL(11, quiet(49U, 1U, 1U));
    TEST_ASSERT_EQUAL(13, edge(1U, 1U));
    TEST_ASSERT_EQUAL(50, g_clock.period);
}



static void test_multiply_fits_more_pulses_between_edges(void)
{
    for (uint8_t multiply = 1U; multiply <= 4U; multiply++)
    {
        const unsigned int expected = EDGE_PULSES * multiply;
        unsigned int       spaced;

        lock(100U, 1U, multiply);
        spaced = quiet(99U, 1U, multiply);
        TEST_ASSERT_EQUAL(expected - 1U, spaced);
        TEST_ASSERT_EQUAL(1, edge(1U, multiply));
    }
}



static void test_multiply_out_of_range_is_limited(void)
{
    lock(100U, 1U, 0U);
    TEST_ASSERT_EQUAL(23, quiet(99U, 1U, 0U));

    lock(100U, 1U, 5U);
    TEST_ASSERT_EQUAL(95, quiet(99U, 1U, 5U));

    lock(100U, 1U, 255U);
    TEST_ASSERT_EQUAL(95, quiet(99U, 1U, 255U));
}



static void test_multiply_waits_for_a_measured_period(void)
{
    // The first stretch has no spacing to go by, so it is given as one step
    clock_init(&g_clock);
    (void)edge(1U, 4U);
    TEST_ASSERT_EQUAL(EDGE_PULSES, g_clock.per_sync);
    TEST_ASSERT_EQUAL(23, quiet(99U, 1U, 4U));
    TEST_ASSERT_EQUAL(1, edge(1U, 4U));
    TEST_ASSERT_EQUAL(4U * EDGE_PULSES, g_clock.per_sync);
}



static void test_divide_passes_over_edges(void)
{
    // Edges 50 ticks apart, every second one a sync edge: a stretch is 100
    lock(50U, 2U, 1U);
    TEST_ASSERT_EQUAL(100, g_clock.period);

    for (unsigned int stretch = 0U; stretch < 3U; stretch++)
    {
        unsigned int pulses = quiet(49U, 2U, 1U);

        TEST_ASSERT_EQUAL(11, pulses);
        pulses += edge(2U, 1U); // Passed over: only the pulse that fell due
        TEST_ASSERT_EQUAL(12, pulses);
        pulses += quiet(49U, 2U, 1U);
        TEST_ASSERT_EQUAL(23, pulses);
        TEST_ASSERT_EQUAL(1, edge(2U, 1U));
    }
}



static void test_divide_by_four_and_by_nothing(void)
{
    lock(25U, 4U, 1U);
    TEST_ASSERT_EQUAL(100, g_clock.period);
    TEST_ASSERT_EQUAL(3, g_clock.edges_left);

    // A divide of 0 is taken as 1
    lock(100U, 0U, 1U);
    TEST_ASSERT_EQUAL(100, g_clock.period);
    TEST_ASSERT_EQUAL(0, g_clock.edges_left);
    TEST_ASSERT_EQUAL(23, quiet(99U, 0U, 1U));
    TEST_ASSERT_EQUAL(1, edge(0U, 1U));
}



static void test_the_ratio_is_read_at_a_sync_edge_and_holds(void)
{
    lock(100U, 1U, 1U);

    // Turned to four times and to a divide of two in the middle of a stretch
    TEST_ASSERT_EQUAL(23, quiet(99U, 2U, 4U));
    TEST_ASSERT_EQUAL(1, edge(2U, 4U)); // Still a sync edge; the new ratio starts
    TEST_ASSERT_EQUAL(4U * EDGE_PULSES, g_clock.per_sync);
    TEST_ASSERT_EQUAL(1, g_clock.edges_left);
}



static void test_a_long_gap_is_not_taken_as_a_period(void)
{
    lock(100U, 1U, 4U);
    TEST_ASSERT_EQUAL(95, quiet(5000U, 1U, 4U));
    TEST_ASSERT_EQUAL(GAP_MAX, g_clock.sync_ticks);

    // The edge ends the stretch; the next is spaced as the first was
    TEST_ASSERT_EQUAL(1, edge(1U, 4U));
    TEST_ASSERT_EQUAL(FIRST_PERIOD, g_clock.period);
    TEST_ASSERT_EQUAL(EDGE_PULSES, g_clock.per_sync);
    TEST_ASSERT_EQUAL(23, quiet(30U, 1U, 4U));
}



static void test_a_gap_just_short_of_the_limit_is_a_period(void)
{
    lock(GAP_MAX - 1U, 1U, 1U);
    TEST_ASSERT_EQUAL(GAP_MAX - 1U, g_clock.period);

    lock(GAP_MAX, 1U, 1U);
    TEST_ASSERT_EQUAL(FIRST_PERIOD, g_clock.period);
}



static void test_two_edges_with_no_time_between_measure_nothing(void)
{
    clock_init(&g_clock);
    TEST_ASSERT_EQUAL(24, follow(true, 0U, 1U, 2U, 24U));
    TEST_ASSERT_EQUAL(24, follow(true, 0U, 1U, 2U, 24U));
    TEST_ASSERT_EQUAL(FIRST_PERIOD, g_clock.period);
    TEST_ASSERT_EQUAL(EDGE_PULSES, g_clock.per_sync);
}



static void test_a_stall_gives_no_more_than_the_stretch_holds(void)
{
    lock(100U, 1U, 4U);
    TEST_ASSERT_EQUAL(95, follow(false, 255U, 1U, 4U, 24U));
    TEST_ASSERT_EQUAL(0, g_clock.phase);
    TEST_ASSERT_EQUAL(0, follow(false, 255U, 1U, 4U, 24U));
    TEST_ASSERT_EQUAL(1, follow(true, 1U, 1U, 4U, 24U));
}



static void test_running_at_a_tempo_ends_the_following(void)
{
    lock(100U, 2U, 3U);
    (void)quiet(30U, 2U, 3U);

    (void)clock_advance(&g_clock, 0U, 1U);
    TEST_ASSERT(!g_clock.b_synced);
    TEST_ASSERT_EQUAL(FIRST_PERIOD, g_clock.period);
    TEST_ASSERT_EQUAL(EDGE_PULSES, g_clock.per_sync);
    TEST_ASSERT_EQUAL(0, g_clock.given);
    TEST_ASSERT_EQUAL(0, g_clock.edges_left);

    // The next edge is a first edge again, though one was to be passed over
    TEST_ASSERT_EQUAL(5, follow(true, 1U, 2U, 3U, 5U));
}



static void test_follow_ignores_null_pointers(void)
{
    clock_state_t  untouched;
    clock_follow_t in;

    in.b_edge         = true;
    in.elapsed_ticks  = 1U;
    in.divide         = 1U;
    in.multiply       = 1U;
    in.pulses_to_step = 24U;

    clock_init(&g_clock);
    untouched = g_clock;
    TEST_ASSERT_EQUAL(0, clock_follow(NULL, &in));
    TEST_ASSERT_EQUAL(0, clock_follow(&g_clock, NULL));
    TEST_ASSERT_EQUAL(0, memcmp(&untouched, &g_clock, sizeof(g_clock)));
}



int main(void)
{
    RUN_TEST(test_init_follows_nothing);
    RUN_TEST(test_the_first_edge_brings_the_callers_step_due);
    RUN_TEST(test_before_the_first_edge_the_step_is_finished_and_no_more);
    RUN_TEST(test_each_stretch_between_edges_holds_exactly_24_pulses);
    RUN_TEST(test_pulses_are_spaced_by_the_measured_period);
    RUN_TEST(test_the_first_stretch_is_spaced_as_twenty_ticks);
    RUN_TEST(test_a_late_edge_is_waited_for_one_pulse_short);
    RUN_TEST(test_an_early_edge_gives_all_that_is_left);
    RUN_TEST(test_multiply_fits_more_pulses_between_edges);
    RUN_TEST(test_multiply_out_of_range_is_limited);
    RUN_TEST(test_multiply_waits_for_a_measured_period);
    RUN_TEST(test_divide_passes_over_edges);
    RUN_TEST(test_divide_by_four_and_by_nothing);
    RUN_TEST(test_the_ratio_is_read_at_a_sync_edge_and_holds);
    RUN_TEST(test_a_long_gap_is_not_taken_as_a_period);
    RUN_TEST(test_a_gap_just_short_of_the_limit_is_a_period);
    RUN_TEST(test_two_edges_with_no_time_between_measure_nothing);
    RUN_TEST(test_a_stall_gives_no_more_than_the_stretch_holds);
    RUN_TEST(test_running_at_a_tempo_ends_the_following);
    RUN_TEST(test_follow_ignores_null_pointers);
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
