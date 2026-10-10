/**
 * @file   test_gate.c
 * @brief  Host unit tests for the gate (core/gate.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "gate.h"
#include "test_harness.h"

#define POISON_BYTE     (0xA5)
#define PULSES_PER_STEP (24U) // What the sequencer calls a step

/**
 * @brief  Advances the gate one pulse at a time until it closes.
 * @return How many pulses it stayed open for, at most limit.
 */
static unsigned int pulses_open(gate_state_t *p_state, unsigned int limit)
{
    unsigned int pulses = 0U;

    while ((pulses < limit) && gate_advance(p_state, 0U))
    {
        (void)gate_advance(p_state, 1U);
        pulses++;
    }
    return pulses;
}



static void test_init_closes_the_gate(void)
{
    gate_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    gate_init(&state);
    TEST_ASSERT_EQUAL(0, state.pulses_left);
    TEST_ASSERT(!gate_advance(&state, 0U));
    TEST_ASSERT(!gate_advance(&state, 1U));
}



static void test_a_note_opens_the_gate_for_its_length_in_pulses(void)
{
    static const uint8_t lengths[] = { 1U, 2U, 12U, 23U, 24U, 100U, 255U };

    for (size_t i = 0U; i < (sizeof(lengths) / sizeof(lengths[0])); i++)
    {
        gate_state_t        state;
        const gate_config_t config = { lengths[i] };

        gate_init(&state);
        gate_begin(&state, &config, true);
        TEST_ASSERT(gate_advance(&state, 0U)); // Open at once
        TEST_ASSERT_EQUAL(lengths[i], pulses_open(&state, 1000U));
        TEST_ASSERT(!gate_advance(&state, 1U)); // And it stays closed
    }
}



static void test_a_length_of_zero_never_opens_the_gate(void)
{
    gate_state_t        state;
    const gate_config_t config = { 0U };

    gate_init(&state);
    gate_begin(&state, &config, true);
    TEST_ASSERT(!gate_advance(&state, 0U));
}



static void test_a_rest_does_not_open_the_gate(void)
{
    gate_state_t        state;
    const gate_config_t config = { 12U };

    gate_init(&state);
    gate_begin(&state, &config, false);
    TEST_ASSERT(!gate_advance(&state, 0U));
    TEST_ASSERT_EQUAL(0, state.pulses_left);
}



static void test_several_pulses_at_once_count_in_full(void)
{
    gate_state_t        state;
    const gate_config_t config = { 12U };

    gate_init(&state);
    gate_begin(&state, &config, true);
    TEST_ASSERT(gate_advance(&state, 5U));
    TEST_ASSERT(gate_advance(&state, 6U));  // 11 gone
    TEST_ASSERT(!gate_advance(&state, 1U)); // The twelfth closes it

    // More pulses than were left close it and do not wrap round
    gate_begin(&state, &config, true);
    TEST_ASSERT(!gate_advance(&state, 13U));
    TEST_ASSERT_EQUAL(0, state.pulses_left);
    gate_begin(&state, &config, true);
    TEST_ASSERT(!gate_advance(&state, 65535U));
    TEST_ASSERT_EQUAL(0, state.pulses_left);
}



static void test_a_shorter_gate_closes_before_the_next_note(void)
{
    gate_state_t        state;
    const gate_config_t config = { 23U };

    gate_init(&state);
    for (unsigned int step = 0U; step < 3U; step++)
    {
        gate_begin(&state, &config, true);
        TEST_ASSERT_EQUAL(23, pulses_open(&state, PULSES_PER_STEP));
        TEST_ASSERT(!gate_advance(&state, 1U)); // The last pulse of the step
    }
}



static void test_a_whole_step_ties_into_the_next_note(void)
{
    gate_state_t        state;
    const gate_config_t config = { PULSES_PER_STEP };

    // Three notes in a row: the gate is never seen closed between them
    gate_init(&state);
    for (unsigned int step = 0U; step < 3U; step++)
    {
        gate_begin(&state, &config, true);
        for (unsigned int pulse = 1U; pulse < PULSES_PER_STEP; pulse++)
        {
            TEST_ASSERT(gate_advance(&state, 1U));
        }
        // The pulse that ends the step is the one the next begins on, so the
        // caller advances and begins before it looks at the level
        (void)gate_advance(&state, 1U);
    }
    TEST_ASSERT(!gate_advance(&state, 0U)); // With no note to follow, it closed
}



static void test_a_longer_length_is_cut_off_by_the_next_boundary(void)
{
    gate_state_t        state;
    const gate_config_t long_gate = { 200U };

    gate_init(&state);
    gate_begin(&state, &long_gate, true);
    TEST_ASSERT(gate_advance(&state, PULSES_PER_STEP));

    // A rest ends a tied note
    gate_begin(&state, &long_gate, false);
    TEST_ASSERT(!gate_advance(&state, 0U));

    // And a new note starts its own count, not what was left of the old one
    gate_begin(&state, &long_gate, true);
    (void)gate_advance(&state, PULSES_PER_STEP);
    {
        const gate_config_t short_gate = { 2U };

        gate_begin(&state, &short_gate, true);
        TEST_ASSERT_EQUAL(2, pulses_open(&state, 1000U));
    }
}



static void test_the_length_is_read_when_the_note_begins(void)
{
    gate_state_t  state;
    gate_config_t config = { 6U };

    gate_init(&state);
    gate_begin(&state, &config, true);
    config.length = 20U; // Changed while the note is held: no effect on it
    TEST_ASSERT_EQUAL(6, pulses_open(&state, 1000U));
}



static void test_null_pointers_are_ignored(void)
{
    gate_state_t        state;
    const gate_config_t config = { 12U };

    gate_init(NULL); // Must not crash
    gate_begin(NULL, &config, true);
    TEST_ASSERT(!gate_advance(NULL, 1U));

    // Without the control there is no length to open for; an open gate is
    // still ended by the boundary
    gate_init(&state);
    gate_begin(&state, NULL, true);
    TEST_ASSERT(!gate_advance(&state, 0U));
    gate_begin(&state, &config, true);
    gate_begin(&state, NULL, true);
    TEST_ASSERT(!gate_advance(&state, 0U));
}



int main(void)
{
    RUN_TEST(test_init_closes_the_gate);
    RUN_TEST(test_a_note_opens_the_gate_for_its_length_in_pulses);
    RUN_TEST(test_a_length_of_zero_never_opens_the_gate);
    RUN_TEST(test_a_rest_does_not_open_the_gate);
    RUN_TEST(test_several_pulses_at_once_count_in_full);
    RUN_TEST(test_a_shorter_gate_closes_before_the_next_note);
    RUN_TEST(test_a_whole_step_ties_into_the_next_note);
    RUN_TEST(test_a_longer_length_is_cut_off_by_the_next_boundary);
    RUN_TEST(test_the_length_is_read_when_the_note_begins);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
