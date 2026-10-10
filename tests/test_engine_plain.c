/**
 * @file   test_engine_plain.c
 * @brief  Host unit tests for the plain engine (core/engine_plain.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "engine_plain.h"
#include "panel.h"
#include "test_harness.h"

#define POISON_BYTE (0xA5)

// Step n holds note value 15 - n, so the step and the note are never the same
static const uint8_t g_descending[PANEL_RAW_BYTE_COUNT] =
{
    0xFEU, 0xDCU, 0xBAU, 0x98U, 0x76U, 0x54U, 0x32U, 0x10U
};

static address_config_t g_address; // The controls the inputs point at

/**
 * @brief Inputs for the descending panel, looping forward over a range.
 */
static engine_inputs_t make_inputs(uint8_t first_step, uint8_t last_step)
{
    engine_inputs_t in;

    (void)memset(&g_address, 0, sizeof(g_address));
    g_address.direction  = ADDRESS_FORWARD;
    g_address.first_step = first_step;
    g_address.last_step  = last_step;
    g_address.b_one_shot = false;

    in.p_raw_steps = g_descending;
    in.p_address   = &g_address;
    return in;
}



static void test_init_goes_to_the_start_of_the_pattern(void)
{
    engine_plain_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    engine_plain_init(&state);
    TEST_ASSERT_EQUAL(0, state.address.position);
    TEST_ASSERT(!state.address.b_finished);
}



static void test_steps_play_in_order_with_their_panel_values(void)
{
    const engine_inputs_t in = make_inputs(0U, 15U);
    engine_plain_state_t  state;

    engine_plain_init(&state);
    for (unsigned int count = 0U; count < (2U * PANEL_STEP_COUNT); count++)
    {
        const unsigned int expected_step = count % PANEL_STEP_COUNT;
        engine_step_t      played;

        (void)memset(&played, POISON_BYTE, sizeof(played));
        TEST_ASSERT(engine_plain_step(&state, &in, &played));
        TEST_ASSERT_EQUAL(expected_step, played.step);
        TEST_ASSERT_EQUAL(15U - expected_step, played.note);
    }
}



static void test_addressing_controls_are_followed(void)
{
    static const uint8_t  expected_steps[] = { 6U, 5U, 4U, 6U };
    const engine_inputs_t in = make_inputs(4U, 6U);
    engine_plain_state_t  state;

    g_address.direction = ADDRESS_REVERSE;
    engine_plain_init(&state);
    for (size_t i = 0U; i < (sizeof(expected_steps) / sizeof(expected_steps[0])); i++)
    {
        engine_step_t played;

        TEST_ASSERT(engine_plain_step(&state, &in, &played));
        TEST_ASSERT_EQUAL(expected_steps[i], played.step);
        TEST_ASSERT_EQUAL(15U - expected_steps[i], played.note);
    }
}



static void test_note_is_read_from_the_panel_as_it_is_now(void)
{
    uint8_t              raw[PANEL_RAW_BYTE_COUNT];
    engine_inputs_t      in = make_inputs(0U, 15U);
    engine_plain_state_t state;
    engine_step_t        played;

    (void)memcpy(raw, g_descending, sizeof(raw));
    in.p_raw_steps = raw;
    engine_plain_init(&state);
    (void)engine_plain_step(&state, &in, &played);

    // Step 1 is changed while step 0 plays: the new value is what plays
    raw[0] = 0xF3U;
    TEST_ASSERT(engine_plain_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(1, played.step);
    TEST_ASSERT_EQUAL(3, played.note);
}



static void test_finished_one_shot_gives_no_step(void)
{
    const engine_inputs_t in = make_inputs(4U, 5U);
    engine_plain_state_t  state;
    engine_step_t         played;

    g_address.b_one_shot = true;
    engine_plain_init(&state);
    TEST_ASSERT(engine_plain_step(&state, &in, &played));
    TEST_ASSERT(engine_plain_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(5, played.step);

    (void)memset(&played, POISON_BYTE, sizeof(played));
    TEST_ASSERT(!engine_plain_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(POISON_BYTE, played.step); // Not written
    TEST_ASSERT_EQUAL(POISON_BYTE, played.note);

    // Init is the reset: the pattern plays again from its start
    engine_plain_init(&state);
    TEST_ASSERT(engine_plain_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(4, played.step);
    TEST_ASSERT_EQUAL(11, played.note);
}



static void test_null_pointers_are_ignored(void)
{
    engine_inputs_t      in = make_inputs(0U, 15U);
    engine_plain_state_t state;
    engine_step_t        played;

    engine_plain_init(NULL); // Must not crash

    engine_plain_init(&state);
    (void)memset(&played, POISON_BYTE, sizeof(played));
    TEST_ASSERT(!engine_plain_step(NULL, &in, &played));
    TEST_ASSERT(!engine_plain_step(&state, NULL, &played));
    TEST_ASSERT(!engine_plain_step(&state, &in, NULL));

    in.p_address = NULL;
    TEST_ASSERT(!engine_plain_step(&state, &in, &played));

    in = make_inputs(0U, 15U);
    in.p_raw_steps = NULL;
    TEST_ASSERT(!engine_plain_step(&state, &in, &played));

    TEST_ASSERT_EQUAL(POISON_BYTE, played.step); // Nothing written
    TEST_ASSERT_EQUAL(0, state.address.position); // And the pattern has not moved
}



int main(void)
{
    RUN_TEST(test_init_goes_to_the_start_of_the_pattern);
    RUN_TEST(test_steps_play_in_order_with_their_panel_values);
    RUN_TEST(test_addressing_controls_are_followed);
    RUN_TEST(test_note_is_read_from_the_panel_as_it_is_now);
    RUN_TEST(test_finished_one_shot_gives_no_step);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
