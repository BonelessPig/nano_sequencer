/**
 * @file   test_midi_in.c
 * @brief  Host unit tests for MIDI in (core/midi_in.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "midi_in.h"
#include "test_harness.h"

#define POISON_BYTE     (0xA5)
#define CLOCK           (0xF8U)
#define START           (0xFAU)
#define CONTINUE        (0xFBU)
#define STOP            (0xFCU)
#define CLOCKS_PER_EDGE (6U) // A step, as midi_in.h says

/**
 * @brief Gives one byte in a call of its own and returns what it came to.
 */
static midi_in_events_t give(midi_in_state_t *p_state, uint8_t byte)
{
    midi_in_events_t events;

    (void)memset(&events, POISON_BYTE, sizeof(events));
    midi_in_read(p_state, &byte, 1U, &events);
    return events;
}

/**
 * @brief Gives a number of clocks, a call each, and returns how many of them
 *        were edges.
 */
static unsigned int edges_in(midi_in_state_t *p_state, unsigned int clocks)
{
    unsigned int edges = 0U;

    for (unsigned int i = 0U; i < clocks; i++)
    {
        const midi_in_events_t events = give(p_state, CLOCK);

        TEST_ASSERT(!events.b_start);
        TEST_ASSERT(!events.b_stop);
        if (events.b_edge)
        {
            edges++;
        }
    }
    return edges;
}



static void test_init_makes_the_next_clock_an_edge(void)
{
    midi_in_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    midi_in_init(&state);
    TEST_ASSERT_EQUAL(0, state.clocks_to_edge);
    TEST_ASSERT(give(&state, CLOCK).b_edge);
}



static void test_no_bytes_ask_for_nothing(void)
{
    midi_in_state_t  state;
    midi_in_events_t events;
    const uint8_t    byte = CLOCK;

    midi_in_init(&state);
    (void)memset(&events, POISON_BYTE, sizeof(events));
    midi_in_read(&state, &byte, 0U, &events);
    TEST_ASSERT(!events.b_edge);
    TEST_ASSERT(!events.b_start);
    TEST_ASSERT(!events.b_stop);
    TEST_ASSERT_EQUAL(0, state.clocks_to_edge);
}



static void test_every_sixth_clock_is_an_edge(void)
{
    midi_in_state_t state;

    midi_in_init(&state);
    for (unsigned int step = 0U; step < 4U; step++)
    {
        TEST_ASSERT(give(&state, CLOCK).b_edge);
        TEST_ASSERT_EQUAL(0, edges_in(&state, CLOCKS_PER_EDGE - 1U));
    }
    // A quarter note of clocks is four steps, wherever the count stands
    TEST_ASSERT_EQUAL(4, edges_in(&state, 4U * CLOCKS_PER_EDGE));
}



static void test_the_count_is_kept_from_one_call_to_the_next(void)
{
    midi_in_state_t  state;
    midi_in_events_t events;
    const uint8_t    four[] = { CLOCK, CLOCK, CLOCK, CLOCK };

    midi_in_init(&state);
    midi_in_read(&state, four, 4U, &events);
    TEST_ASSERT(events.b_edge); // The first of them
    midi_in_read(&state, four, 2U, &events);
    TEST_ASSERT(!events.b_edge);
    midi_in_read(&state, four, 4U, &events);
    TEST_ASSERT(events.b_edge); // The seventh clock, the first of this call
    TEST_ASSERT_EQUAL(2, state.clocks_to_edge);
}



static void test_a_start_and_a_continue_make_the_next_clock_an_edge(void)
{
    const uint8_t starts[] = { START, CONTINUE };

    for (size_t i = 0U; i < sizeof(starts); i++)
    {
        midi_in_state_t  state;
        midi_in_events_t events;

        midi_in_init(&state);
        TEST_ASSERT_EQUAL(1, edges_in(&state, 3U));

        events = give(&state, starts[i]);
        TEST_ASSERT(events.b_start);
        TEST_ASSERT(!events.b_stop);
        TEST_ASSERT(!events.b_edge);

        TEST_ASSERT(give(&state, CLOCK).b_edge);
        TEST_ASSERT_EQUAL(0, edges_in(&state, CLOCKS_PER_EDGE - 1U));
        TEST_ASSERT(give(&state, CLOCK).b_edge);
    }
}



static void test_a_stop_leaves_the_count_of_clocks_alone(void)
{
    midi_in_state_t  state;
    midi_in_events_t events;

    midi_in_init(&state);
    TEST_ASSERT_EQUAL(1, edges_in(&state, 2U));

    events = give(&state, STOP);
    TEST_ASSERT(events.b_stop);
    TEST_ASSERT(!events.b_start);
    TEST_ASSERT(!events.b_edge);

    // Four more to the sixth, and the seventh is the edge
    TEST_ASSERT_EQUAL(0, edges_in(&state, 4U));
    TEST_ASSERT(give(&state, CLOCK).b_edge);
}



static void test_the_later_of_a_start_and_a_stop_is_the_one_given(void)
{
    midi_in_state_t  state;
    midi_in_events_t events;
    const uint8_t    stop_start[] = { STOP, START };
    const uint8_t    start_stop[] = { START, STOP };

    midi_in_init(&state);
    midi_in_read(&state, stop_start, sizeof(stop_start), &events);
    TEST_ASSERT(events.b_start);
    TEST_ASSERT(!events.b_stop);

    midi_in_read(&state, start_stop, sizeof(start_stop), &events);
    TEST_ASSERT(events.b_stop);
    TEST_ASSERT(!events.b_start);
}



static void test_a_start_forgets_an_edge_ahead_of_it_and_keeps_one_after(void)
{
    midi_in_state_t  state;
    midi_in_events_t events;
    const uint8_t    clock_start[]       = { CLOCK, START };
    const uint8_t    clock_start_clock[] = { CLOCK, START, CLOCK };

    midi_in_init(&state);
    midi_in_read(&state, clock_start, sizeof(clock_start), &events);
    TEST_ASSERT(events.b_start);
    TEST_ASSERT(!events.b_edge);
    // The clock still to come is the edge
    TEST_ASSERT(give(&state, CLOCK).b_edge);

    midi_in_init(&state);
    midi_in_read(&state, clock_start_clock, sizeof(clock_start_clock), &events);
    TEST_ASSERT(events.b_start);
    TEST_ASSERT(events.b_edge);
    TEST_ASSERT_EQUAL(CLOCKS_PER_EDGE - 1U, state.clocks_to_edge);
}



static void test_every_other_byte_is_passed_over(void)
{
    midi_in_state_t state;

    midi_in_init(&state);
    TEST_ASSERT_EQUAL(1, edges_in(&state, 3U));

    for (unsigned int byte = 0U; byte <= 0xFFU; byte++)
    {
        if ((CLOCK != byte) && (START != byte) && (CONTINUE != byte) && (STOP != byte))
        {
            const midi_in_events_t events = give(&state, (uint8_t)byte);

            TEST_ASSERT(!events.b_edge);
            TEST_ASSERT(!events.b_start);
            TEST_ASSERT(!events.b_stop);
            TEST_ASSERT_EQUAL(3, state.clocks_to_edge);
        }
    }
}



static void test_a_clock_between_the_bytes_of_a_note_still_counts(void)
{
    midi_in_state_t  state;
    midi_in_events_t events;
    // A Note On for channel 1 with a clock after its first data byte
    const uint8_t    bytes[] = { 0x90U, 0x3CU, CLOCK, 0x64U };

    midi_in_init(&state);
    midi_in_read(&state, bytes, sizeof(bytes), &events);
    TEST_ASSERT(events.b_edge);
    TEST_ASSERT(!events.b_start);
    TEST_ASSERT(!events.b_stop);
}



static void test_null_pointers_are_safe(void)
{
    midi_in_state_t  state;
    midi_in_events_t events;
    const uint8_t    byte = START;

    midi_in_init(NULL);

    midi_in_init(&state);
    (void)memset(&events, POISON_BYTE, sizeof(events));
    midi_in_read(NULL, &byte, 1U, &events);
    TEST_ASSERT(POISON_BYTE == *(const unsigned char *)&events);

    midi_in_read(&state, &byte, 1U, NULL);
    TEST_ASSERT_EQUAL(0, state.clocks_to_edge);

    // No bytes to read from: the events are cleared and nothing else moves
    midi_in_read(&state, NULL, 3U, &events);
    TEST_ASSERT(!events.b_edge);
    TEST_ASSERT(!events.b_start);
    TEST_ASSERT(!events.b_stop);
}



int main(void)
{
    RUN_TEST(test_init_makes_the_next_clock_an_edge);
    RUN_TEST(test_no_bytes_ask_for_nothing);
    RUN_TEST(test_every_sixth_clock_is_an_edge);
    RUN_TEST(test_the_count_is_kept_from_one_call_to_the_next);
    RUN_TEST(test_a_start_and_a_continue_make_the_next_clock_an_edge);
    RUN_TEST(test_a_stop_leaves_the_count_of_clocks_alone);
    RUN_TEST(test_the_later_of_a_start_and_a_stop_is_the_one_given);
    RUN_TEST(test_a_start_forgets_an_edge_ahead_of_it_and_keeps_one_after);
    RUN_TEST(test_every_other_byte_is_passed_over);
    RUN_TEST(test_a_clock_between_the_bytes_of_a_note_still_counts);
    RUN_TEST(test_null_pointers_are_safe);
    return TEST_RESULT();
}
