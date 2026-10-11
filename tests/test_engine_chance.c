/**
 * @file   test_engine_chance.c
 * @brief  Host unit tests for the chance engine (core/engine_chance.c).
 * @author BonelessPig
 *
 * The random numbers come from the real generator, started from a seed whose
 * first bytes are known (see test_prng.c): 135, 35, 70, 220, 176, 221, 238,
 * 248, 253, 195. Their low two bits are 3, 3, 2, 0, 0, 1, 2, 0, 1, 3.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "engine_chance.h"
#include "panel.h"
#include "test_harness.h"

#define POISON_BYTE (0xA5)
#define SEED        (0xACE1U)
#define MODE_BYTE   (8U)    // Where the mode switches are in the raw bytes
#define SKIP_1_IN_4 (0x08U) // The engine's mode switches
#define SKIP_3_IN_4 (0x18U)
#define SHUFFLE     (0x20U)

// Step n holds note value 15 - n; step 15 would be a rest and is not used
static uint8_t g_raw[PANEL_RAW_BYTE_COUNT] =
{
    0xFEU, 0xDCU, 0xBAU, 0x98U, 0x76U, 0x54U, 0x32U, 0x10U, 0x00U
};

static address_config_t g_address; // The controls the inputs point at
static prng_state_t     g_prng;    // The random numbers they point at

/**
 * @brief Inputs for the descending panel, looping forward over a range,
 *        with the engine's mode switches set and the generator started.
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
    prng_init(&g_prng, SEED);

    in.p_raw_steps = g_raw;
    in.p_address   = &g_address;
    in.p_prng      = &g_prng;
    return in;
}



/**
 * @brief Asks for one step and checks it: the step, and whether it sounds
 *        (with the note its switches hold) or was skipped.
 */
static void expect_step(engine_chance_state_t *p_state, const engine_inputs_t *p_in,
                        uint8_t step, bool b_sounds)
{
    engine_step_t played;

    (void)memset(&played, POISON_BYTE, sizeof(played));
    TEST_ASSERT(engine_chance_step(p_state, p_in, &played));
    TEST_ASSERT_EQUAL(step, played.step);
    TEST_ASSERT_EQUAL(b_sounds ? (15U - step) : 0U, played.note);
    TEST_ASSERT(!played.b_tie);
}



static void test_init_goes_to_the_start_of_the_pattern(void)
{
    engine_chance_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    engine_chance_init(&state);
    TEST_ASSERT_EQUAL(0, state.address.position);
    TEST_ASSERT(!state.address.b_finished);
}



static void test_switches_off_is_the_plain_engine(void)
{
    const engine_inputs_t in = make_inputs(0U, 14U, 0xC7U); // Others' switches on
    engine_chance_state_t state;

    engine_chance_init(&state);
    for (unsigned int count = 0U; count < 30U; count++)
    {
        expect_step(&state, &in, (uint8_t)(count % 15U), true);
    }
}



static void test_skip_turns_that_share_of_steps_into_rests(void)
{
    // A step is skipped when its draw is among the top one, or three, of four
    static const bool one_in_four[]   = { false, false, true, true, true,
                                          true, true, true, true, false };
    static const bool three_in_four[] = { false, false, false, true, true,
                                          false, false, true, false, false };
    engine_inputs_t       in = make_inputs(0U, 14U, SKIP_1_IN_4);
    engine_chance_state_t state;

    engine_chance_init(&state);
    for (uint8_t i = 0U; i < 10U; i++)
    {
        expect_step(&state, &in, i, one_in_four[i]);
    }

    in = make_inputs(0U, 14U, SKIP_3_IN_4);
    engine_chance_init(&state);
    for (uint8_t i = 0U; i < 10U; i++)
    {
        expect_step(&state, &in, i, three_in_four[i]);
    }
}



static void test_shuffle_picks_each_step_from_the_range(void)
{
    // Every step takes two bytes: one for the pick and one for the skip.
    // The picks are 135, 70, 176 and 238
    engine_inputs_t       in = make_inputs(0U, 15U, SHUFFLE);
    engine_chance_state_t state;

    engine_chance_init(&state);
    expect_step(&state, &in, 7U, true);
    expect_step(&state, &in, 6U, true);
    expect_step(&state, &in, 0U, true);
    expect_step(&state, &in, 14U, true);

    in = make_inputs(4U, 6U, SHUFFLE);
    engine_chance_init(&state);
    expect_step(&state, &in, 4U, true);
    expect_step(&state, &in, 5U, true);
    expect_step(&state, &in, 6U, true);
    expect_step(&state, &in, 5U, true);

    // A range that runs round through step 15, and one of a single step
    in = make_inputs(14U, 1U, SHUFFLE);
    engine_chance_init(&state);
    expect_step(&state, &in, 1U, true);
    expect_step(&state, &in, 0U, true);
    expect_step(&state, &in, 14U, true);

    in = make_inputs(9U, 9U, SHUFFLE);
    engine_chance_init(&state);
    expect_step(&state, &in, 9U, true);
    expect_step(&state, &in, 9U, true);
}



static void test_with_no_random_numbers_nothing_is_left_to_chance(void)
{
    engine_inputs_t       in = make_inputs(3U, 8U, SKIP_3_IN_4 | SHUFFLE);
    engine_chance_state_t state;

    in.p_prng = NULL;
    engine_chance_init(&state);
    for (unsigned int i = 0U; i < 8U; i++)
    {
        expect_step(&state, &in, 3U, true);
    }
}



static void test_a_one_shot_still_ends_after_a_cycle(void)
{
    const engine_inputs_t in = make_inputs(4U, 6U, SHUFFLE);
    engine_chance_state_t state;
    engine_chance_state_t before;
    engine_step_t         played;
    prng_state_t          prng_before;

    g_address.b_one_shot = true;
    engine_chance_init(&state);
    TEST_ASSERT(engine_chance_step(&state, &in, &played));
    TEST_ASSERT(engine_chance_step(&state, &in, &played));
    TEST_ASSERT(engine_chance_step(&state, &in, &played));

    before      = state;
    prng_before = g_prng;
    (void)memset(&played, POISON_BYTE, sizeof(played));
    TEST_ASSERT(!engine_chance_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(POISON_BYTE, played.step); // Not written
    TEST_ASSERT_EQUAL(0, memcmp(&before, &state, sizeof(state)));
    TEST_ASSERT_EQUAL(prng_before.value, g_prng.value); // And nothing drawn

    engine_chance_init(&state); // The reset
    TEST_ASSERT(engine_chance_step(&state, &in, &played));
}



static void test_null_pointers_are_ignored(void)
{
    engine_inputs_t       in = make_inputs(0U, 14U, 0U);
    engine_chance_state_t state;
    engine_step_t         played;

    engine_chance_init(NULL); // Must not crash

    engine_chance_init(&state);
    (void)memset(&played, POISON_BYTE, sizeof(played));
    TEST_ASSERT(!engine_chance_step(NULL, &in, &played));
    TEST_ASSERT(!engine_chance_step(&state, NULL, &played));
    TEST_ASSERT(!engine_chance_step(&state, &in, NULL));

    in.p_address = NULL;
    TEST_ASSERT(!engine_chance_step(&state, &in, &played));

    in = make_inputs(0U, 14U, 0U);
    in.p_raw_steps = NULL;
    TEST_ASSERT(!engine_chance_step(&state, &in, &played));

    TEST_ASSERT_EQUAL(POISON_BYTE, played.step); // Nothing written
    TEST_ASSERT_EQUAL(0, state.address.position); // And the pattern has not moved
}



int main(void)
{
    RUN_TEST(test_init_goes_to_the_start_of_the_pattern);
    RUN_TEST(test_switches_off_is_the_plain_engine);
    RUN_TEST(test_skip_turns_that_share_of_steps_into_rests);
    RUN_TEST(test_shuffle_picks_each_step_from_the_range);
    RUN_TEST(test_with_no_random_numbers_nothing_is_left_to_chance);
    RUN_TEST(test_a_one_shot_still_ends_after_a_cycle);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
