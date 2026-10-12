/**
 * @file   test_engine_cartesian.c
 * @brief  Host unit tests for the Cartesian engine (core/engine_cartesian.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "engine_cartesian.h"
#include "panel.h"
#include "test_harness.h"

#define POISON_BYTE (0xA5)
#define MODE_BYTE   (8U) // Where the mode switches are in the raw bytes

// The engine's mode switches: x in bits 3 and 4, y in bits 5 and 6
#define MODE(x_setting, y_setting) ((uint8_t)(((x_setting) << 3U) | ((y_setting) << 5U)))

// Step n holds note value 15 - n
static uint8_t g_raw[PANEL_RAW_BYTE_COUNT] =
{
    0xFEU, 0xDCU, 0xBAU, 0x98U, 0x76U, 0x54U, 0x32U, 0x10U, 0x00U
};

/**
 * @brief Inputs for the descending panel with the mode switches set.
 */
static engine_inputs_t make_inputs(uint8_t mode)
{
    engine_inputs_t in;

    g_raw[MODE_BYTE] = mode;
    in.p_raw_steps   = g_raw;
    in.p_address     = NULL;
    in.p_prng        = NULL;
    in.offset.depth      = 0U;
    in.offset.b_negative = false;
    return in;
}



/**
 * @brief Asks for a run of steps from a fresh start and checks each.
 */
static void expect_steps(uint8_t mode, const uint8_t *p_steps, size_t count)
{
    const engine_inputs_t    in = make_inputs(mode);
    engine_cartesian_state_t state;

    engine_cartesian_init(&state);
    for (size_t i = 0U; i < count; i++)
    {
        engine_step_t played;

        (void)memset(&played, POISON_BYTE, sizeof(played));
        TEST_ASSERT(engine_cartesian_step(&state, &in, &played));
        TEST_ASSERT_EQUAL(p_steps[i], played.step);
        TEST_ASSERT_EQUAL(15U - p_steps[i], played.note);
        TEST_ASSERT(!played.b_tie);
    }
}



static void test_init_goes_to_step_zero(void)
{
    engine_cartesian_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    engine_cartesian_init(&state);
    TEST_ASSERT_EQUAL(0, state.x);
    TEST_ASSERT_EQUAL(0, state.y);
    TEST_ASSERT_EQUAL(0, state.x_steps);
    TEST_ASSERT_EQUAL(0, state.y_steps);
}



static void test_switches_off_is_the_sixteen_steps_in_order(void)
{
    static const uint8_t steps[] =
    {
        0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U, 0U, 1U
    };

    expect_steps(MODE(0U, 0U), steps, sizeof(steps));
    expect_steps((uint8_t)(MODE(0U, 0U) | 0x87U), steps, sizeof(steps)); // Others' switches
}



static void test_x_moves_every_so_many_steps(void)
{
    static const uint8_t every_2[] =
    {
        0U, 0U, 1U, 1U, 6U, 6U, 7U, 7U, 8U, 8U, 9U, 9U, 14U, 14U, 15U, 15U, 0U
    };
    // Every fourth step, in time with y: the diagonal, four steps each
    static const uint8_t every_4[] = { 0U, 0U, 0U, 0U, 5U, 5U, 5U, 5U, 10U };

    expect_steps(MODE(1U, 0U), every_2, sizeof(every_2));
    expect_steps(MODE(3U, 0U), every_4, sizeof(every_4));
}



static void test_y_moves_every_so_many_steps(void)
{
    static const uint8_t every_3[] =
    {
        0U, 1U, 2U, 7U, 4U, 5U, 10U, 11U, 8U, 13U, 14U, 15U, 0U
    };
    static const uint8_t every_1[] = { 0U, 5U, 10U, 15U, 0U, 5U };

    expect_steps(MODE(0U, 1U), every_3, sizeof(every_3));
    expect_steps(MODE(0U, 3U), every_1, sizeof(every_1));
}



static void test_offset_moves_the_cell_round_the_sixteen_and_not_the_counters(void)
{
    engine_inputs_t          in = make_inputs(MODE(0U, 0U));
    engine_cartesian_state_t state;
    engine_step_t            played;

    // A depth of 128 is four of the sixteen: the cell below
    in.offset.depth = 128U;
    engine_cartesian_init(&state);
    for (unsigned int count = 0U; count < 18U; count++)
    {
        const unsigned int step = (count + 4U) % 16U;

        TEST_ASSERT(engine_cartesian_step(&state, &in, &played));
        TEST_ASSERT_EQUAL(step, played.step);
        TEST_ASSERT_EQUAL(15U - step, played.note);
    }

    // Eighteen steps played, so cell 2 is next: back by eight is cell 10
    in.offset.depth      = 255U;
    in.offset.b_negative = true;
    TEST_ASSERT(engine_cartesian_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(10, played.step);

    // And with no offset the path is where it would have been
    in.offset.depth = 0U;
    TEST_ASSERT(engine_cartesian_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(3, played.step);
}



static void test_init_is_the_reset(void)
{
    const engine_inputs_t    in = make_inputs(MODE(0U, 1U));
    engine_cartesian_state_t state;
    engine_step_t            played;

    engine_cartesian_init(&state);
    for (unsigned int i = 0U; i < 5U; i++)
    {
        (void)engine_cartesian_step(&state, &in, &played);
    }
    engine_cartesian_init(&state);
    TEST_ASSERT(engine_cartesian_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(0, played.step);
    TEST_ASSERT(engine_cartesian_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(1, played.step);
}



static void test_null_pointers_are_ignored(void)
{
    engine_inputs_t          in = make_inputs(0U);
    engine_cartesian_state_t state;
    engine_step_t            played;

    engine_cartesian_init(NULL); // Must not crash

    engine_cartesian_init(&state);
    (void)memset(&played, POISON_BYTE, sizeof(played));
    TEST_ASSERT(!engine_cartesian_step(NULL, &in, &played));
    TEST_ASSERT(!engine_cartesian_step(&state, NULL, &played));
    TEST_ASSERT(!engine_cartesian_step(&state, &in, NULL));

    in.p_raw_steps = NULL;
    TEST_ASSERT(!engine_cartesian_step(&state, &in, &played));

    TEST_ASSERT_EQUAL(POISON_BYTE, played.step); // Nothing written
    TEST_ASSERT_EQUAL(0, state.x_steps);         // And nothing counted
}



int main(void)
{
    RUN_TEST(test_init_goes_to_step_zero);
    RUN_TEST(test_switches_off_is_the_sixteen_steps_in_order);
    RUN_TEST(test_x_moves_every_so_many_steps);
    RUN_TEST(test_y_moves_every_so_many_steps);
    RUN_TEST(test_offset_moves_the_cell_round_the_sixteen_and_not_the_counters);
    RUN_TEST(test_init_is_the_reset);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
