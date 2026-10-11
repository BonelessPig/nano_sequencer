/**
 * @file   test_engine_euclid.c
 * @brief  Host unit tests for the Euclidean engine (core/engine_euclid.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "engine_euclid.h"
#include "panel.h"
#include "test_harness.h"

#define POISON_BYTE (0xA5)
#define MODE_BYTE   (8U) // Where the mode switches are in the raw bytes
#define BAR_STEPS   (16U)

// The engine's mode switches: the beats in a bar, less one, in bits 3 to 6
#define BEATS(count) ((uint8_t)(((count) - 1U) << 3U))

// Every step holds note value 9, so a step that sounds is told from a rest
static uint8_t g_raw[PANEL_RAW_BYTE_COUNT] =
{
    0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x00U
};

static address_config_t g_address; // The controls the inputs point at

/**
 * @brief Inputs for the panel, looping forward over a range, with the mode
 *        switches set.
 */
static engine_inputs_t make_inputs(uint8_t first_step, uint8_t last_step, uint8_t mode)
{
    engine_inputs_t in;

    (void)memset(&g_address, 0, sizeof(g_address));
    g_address.direction  = ADDRESS_FORWARD;
    g_address.first_step = first_step;
    g_address.last_step  = last_step;
    g_address.b_one_shot = false;
    g_raw[MODE_BYTE]     = mode;

    in.p_raw_steps = g_raw;
    in.p_address   = &g_address;
    in.p_prng      = NULL;
    return in;
}



/**
 * @brief Plays two bars over the whole panel from a fresh start and checks
 *        that exactly the steps marked in a 16-bit map sound (bit n for
 *        step n of the bar), in order.
 */
static void expect_beats(uint8_t mode, uint16_t beat_map)
{
    const engine_inputs_t in = make_inputs(0U, 15U, mode);
    engine_euclid_state_t state;

    engine_euclid_init(&state);
    for (unsigned int count = 0U; count < (2U * BAR_STEPS); count++)
    {
        const unsigned int place   = count % BAR_STEPS;
        const bool         b_beat  = (0U != ((beat_map >> place) & 1U));
        engine_step_t      played;

        (void)memset(&played, POISON_BYTE, sizeof(played));
        TEST_ASSERT(engine_euclid_step(&state, &in, &played));
        TEST_ASSERT_EQUAL(place, played.step);
        TEST_ASSERT_EQUAL(b_beat ? 9U : 0U, played.note);
        TEST_ASSERT(!played.b_tie);
    }
}



static void test_init_goes_to_the_start_of_the_pattern_and_the_bar(void)
{
    engine_euclid_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    engine_euclid_init(&state);
    TEST_ASSERT_EQUAL(0, state.address.position);
    TEST_ASSERT(!state.address.b_finished);
    TEST_ASSERT_EQUAL(0, state.beat);
}



static void test_beats_are_spread_evenly_over_the_bar(void)
{
    expect_beats(BEATS(1U), 0x0001U);  // The first step alone
    expect_beats(BEATS(2U), 0x0101U);  // Steps 0 and 8
    expect_beats(BEATS(4U), 0x1111U);  // Every fourth
    expect_beats(BEATS(5U), 0x2491U);  // Steps 0, 4, 7, 10 and 13
    expect_beats(BEATS(8U), 0x5555U);  // Every other
    expect_beats(BEATS(16U), 0xFFFFU); // Every step: the plain engine
}



static void test_every_setting_gives_that_many_beats(void)
{
    for (uint8_t beats = 1U; beats <= BAR_STEPS; beats++)
    {
        // Other engines' switches on as well; they change nothing
        const engine_inputs_t in = make_inputs(0U, 15U, (uint8_t)(BEATS(beats) | 0x87U));
        engine_euclid_state_t state;
        unsigned int          sounded = 0U;

        engine_euclid_init(&state);
        for (unsigned int count = 0U; count < BAR_STEPS; count++)
        {
            engine_step_t played;

            TEST_ASSERT(engine_euclid_step(&state, &in, &played));
            sounded += (0U != played.note) ? 1U : 0U;
        }
        TEST_ASSERT_EQUAL(beats, sounded);
    }
}



static void test_the_bar_is_counted_apart_from_the_pattern(void)
{
    // Three steps round and round under four beats to the bar: the beats
    // fall on steps 0, 1, 2, 0 of the pattern in turn
    static const uint8_t  steps[]  = { 0U, 1U, 2U, 0U, 1U, 2U, 0U, 1U, 2U, 0U, 1U, 2U, 0U };
    static const uint8_t  notes[]  = { 9U, 0U, 0U, 0U, 9U, 0U, 0U, 0U, 9U, 0U, 0U, 0U, 9U };
    const engine_inputs_t in = make_inputs(0U, 2U, BEATS(4U));
    engine_euclid_state_t state;

    engine_euclid_init(&state);
    for (size_t i = 0U; i < sizeof(steps); i++)
    {
        engine_step_t played;

        TEST_ASSERT(engine_euclid_step(&state, &in, &played));
        TEST_ASSERT_EQUAL(steps[i], played.step);
        TEST_ASSERT_EQUAL(notes[i], played.note);
    }
}



static void test_finished_one_shot_gives_no_step(void)
{
    const engine_inputs_t in = make_inputs(4U, 5U, BEATS(16U));
    engine_euclid_state_t state;
    engine_euclid_state_t before;
    engine_step_t         played;

    g_address.b_one_shot = true;
    engine_euclid_init(&state);
    TEST_ASSERT(engine_euclid_step(&state, &in, &played));
    TEST_ASSERT(engine_euclid_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(5, played.step);

    before = state;
    (void)memset(&played, POISON_BYTE, sizeof(played));
    TEST_ASSERT(!engine_euclid_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(POISON_BYTE, played.step); // Not written
    TEST_ASSERT_EQUAL(0, memcmp(&before, &state, sizeof(state))); // Nor moved on

    // Init is the reset: pattern and bar both start again
    engine_euclid_init(&state);
    TEST_ASSERT(engine_euclid_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(4, played.step);
    TEST_ASSERT_EQUAL(1, state.beat);
}



static void test_null_pointers_are_ignored(void)
{
    engine_inputs_t       in = make_inputs(0U, 15U, BEATS(16U));
    engine_euclid_state_t state;
    engine_step_t         played;

    engine_euclid_init(NULL); // Must not crash

    engine_euclid_init(&state);
    (void)memset(&played, POISON_BYTE, sizeof(played));
    TEST_ASSERT(!engine_euclid_step(NULL, &in, &played));
    TEST_ASSERT(!engine_euclid_step(&state, NULL, &played));
    TEST_ASSERT(!engine_euclid_step(&state, &in, NULL));

    in.p_address = NULL;
    TEST_ASSERT(!engine_euclid_step(&state, &in, &played));

    in = make_inputs(0U, 15U, BEATS(16U));
    in.p_raw_steps = NULL;
    TEST_ASSERT(!engine_euclid_step(&state, &in, &played));

    TEST_ASSERT_EQUAL(POISON_BYTE, played.step); // Nothing written
    TEST_ASSERT_EQUAL(0, state.address.position); // And nothing has moved
    TEST_ASSERT_EQUAL(0, state.beat);
}



int main(void)
{
    RUN_TEST(test_init_goes_to_the_start_of_the_pattern_and_the_bar);
    RUN_TEST(test_beats_are_spread_evenly_over_the_bar);
    RUN_TEST(test_every_setting_gives_that_many_beats);
    RUN_TEST(test_the_bar_is_counted_apart_from_the_pattern);
    RUN_TEST(test_finished_one_shot_gives_no_step);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
