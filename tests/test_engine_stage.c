/**
 * @file   test_engine_stage.c
 * @brief  Host unit tests for the stage engine (core/engine_stage.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "engine_stage.h"
#include "panel.h"
#include "test_harness.h"

#define POISON_BYTE (0xA5)

#define STAGE_COUNT (8U)

// A stage's control: its gate mode in the high two switches, its length
// less one in the low two
#define CONTROL(mode, length) ((uint8_t)(((mode) << 2U) | ((length) - 1U)))
#define SINGLE (0U)
#define REPEAT (1U)
#define HOLD   (2U)
#define REST   (3U)

static uint8_t          g_raw[PANEL_RAW_BYTE_COUNT]; // The panel the inputs point at
static address_config_t g_address;                   // The controls they point at

/**
 * @brief Sets one step's four switches on the panel.
 */
static void set_step(uint8_t step, uint8_t value)
{
    const uint8_t byte_index = (uint8_t)(step / 2U);
    const uint8_t shift      = (0U == (step % 2U)) ? 4U : 0U;

    g_raw[byte_index] = (uint8_t)((g_raw[byte_index] & (uint8_t)~(uint8_t)(0x0FU << shift))
                                  | (uint8_t)(value << shift));
}



/**
 * @brief Inputs for a panel whose stage n holds note value 15 - n, with
 *        every control off, looping forward over a range.
 */
static engine_inputs_t make_inputs(uint8_t first_step, uint8_t last_step)
{
    engine_inputs_t in;

    (void)memset(g_raw, 0, sizeof(g_raw));
    for (uint8_t stage = 0U; stage < STAGE_COUNT; stage++)
    {
        set_step(stage, (uint8_t)(15U - stage));
    }

    (void)memset(&g_address, 0, sizeof(g_address));
    g_address.direction  = ADDRESS_FORWARD;
    g_address.first_step = first_step;
    g_address.last_step  = last_step;
    g_address.b_one_shot = false;

    in.p_raw_steps = g_raw;
    in.p_address   = &g_address;
    in.p_prng      = NULL;
    in.offset.depth      = 0U;
    in.offset.b_negative = false;
    return in;
}



/**
 * @brief Asks for one step and checks all of it.
 */
static void expect_step(engine_stage_state_t *p_state, const engine_inputs_t *p_in,
                        uint8_t step, uint8_t note, bool b_tie)
{
    engine_step_t played;

    (void)memset(&played, POISON_BYTE, sizeof(played));
    TEST_ASSERT(engine_stage_step(p_state, p_in, &played));
    TEST_ASSERT_EQUAL(step, played.step);
    TEST_ASSERT_EQUAL(note, played.note);
    TEST_ASSERT_EQUAL(b_tie, played.b_tie);
}



static void test_init_goes_to_the_start_of_the_pattern(void)
{
    engine_stage_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    engine_stage_init(&state);
    TEST_ASSERT_EQUAL(0, state.address.position);
    TEST_ASSERT(!state.address.b_finished);
    TEST_ASSERT_EQUAL(0, state.stage);
    TEST_ASSERT_EQUAL(0, state.length);
    TEST_ASSERT_EQUAL(0, state.played);
}



static void test_controls_all_off_is_a_plain_pattern_of_eight_steps(void)
{
    const engine_inputs_t in = make_inputs(0U, 15U);
    engine_stage_state_t  state;

    engine_stage_init(&state);
    for (unsigned int count = 0U; count < (2U * STAGE_COUNT); count++)
    {
        const uint8_t stage = (uint8_t)(count % STAGE_COUNT);

        expect_step(&state, &in, stage, (uint8_t)(15U - stage), false);
    }
}



static void test_a_stage_lasts_its_length_in_steps(void)
{
    const engine_inputs_t in = make_inputs(0U, 7U);
    engine_stage_state_t  state;

    set_step(8U, CONTROL(REPEAT, 4U));
    set_step(9U, CONTROL(REPEAT, 2U));
    set_step(10U, CONTROL(REPEAT, 3U));
    engine_stage_init(&state);

    // Repeat: the note plays again on every step, tied to nothing
    for (unsigned int i = 0U; i < 4U; i++)
    {
        expect_step(&state, &in, 0U, 15U, false);
    }
    for (unsigned int i = 0U; i < 2U; i++)
    {
        expect_step(&state, &in, 1U, 14U, false);
    }
    for (unsigned int i = 0U; i < 3U; i++)
    {
        expect_step(&state, &in, 2U, 13U, false);
    }
    expect_step(&state, &in, 3U, 12U, false);
}



static void test_single_plays_the_first_step_only(void)
{
    const engine_inputs_t in = make_inputs(0U, 7U);
    engine_stage_state_t  state;

    set_step(8U, CONTROL(SINGLE, 3U));
    engine_stage_init(&state);
    expect_step(&state, &in, 0U, 15U, false);
    expect_step(&state, &in, 0U, 0U, false);
    expect_step(&state, &in, 0U, 0U, false);
    expect_step(&state, &in, 1U, 14U, false);
}



static void test_hold_ties_every_step_but_the_last(void)
{
    const engine_inputs_t in = make_inputs(0U, 7U);
    engine_stage_state_t  state;

    set_step(8U, CONTROL(HOLD, 3U));
    set_step(9U, CONTROL(HOLD, 1U));
    engine_stage_init(&state);
    expect_step(&state, &in, 0U, 15U, true);
    expect_step(&state, &in, 0U, 15U, true);
    expect_step(&state, &in, 0U, 15U, false);

    // A hold of one step has nothing to tie to
    expect_step(&state, &in, 1U, 14U, false);
    expect_step(&state, &in, 2U, 13U, false);
}



static void test_rest_plays_nothing_for_the_whole_stage(void)
{
    const engine_inputs_t in = make_inputs(0U, 7U);
    engine_stage_state_t  state;

    set_step(8U, CONTROL(REST, 2U));
    engine_stage_init(&state);
    expect_step(&state, &in, 0U, 0U, false);
    expect_step(&state, &in, 0U, 0U, false);
    expect_step(&state, &in, 1U, 14U, false);
}



static void test_a_note_value_of_zero_is_a_rest_in_any_mode(void)
{
    const engine_inputs_t in = make_inputs(0U, 7U);
    engine_stage_state_t  state;

    set_step(0U, 0U);
    set_step(8U, CONTROL(HOLD, 2U));
    engine_stage_init(&state);
    expect_step(&state, &in, 0U, 0U, true); // The tie means nothing for a rest
    expect_step(&state, &in, 0U, 0U, false);
}



static void test_length_is_read_once_and_the_rest_on_every_step(void)
{
    const engine_inputs_t in = make_inputs(0U, 7U);
    engine_stage_state_t  state;

    set_step(8U, CONTROL(REPEAT, 3U));
    engine_stage_init(&state);
    expect_step(&state, &in, 0U, 15U, false);

    // Shortened, set to hold and given another note while it plays: the
    // stage keeps its three steps, and the rest is heard at once
    set_step(8U, CONTROL(HOLD, 1U));
    set_step(0U, 9U);
    expect_step(&state, &in, 0U, 9U, true);
    expect_step(&state, &in, 0U, 9U, false);
    expect_step(&state, &in, 1U, 14U, false);
}



static void test_addressing_controls_are_followed_as_stages(void)
{
    static const uint8_t reversed[]  = { 6U, 5U, 4U, 6U };
    static const uint8_t wrapped[]   = { 6U, 7U, 0U, 1U, 6U };
    static const uint8_t folded[]    = { 1U, 2U, 1U };
    static const uint8_t pendulum[]  = { 0U, 1U, 2U, 1U, 0U };
    engine_inputs_t      in = make_inputs(4U, 6U);
    engine_stage_state_t state;

    g_address.direction = ADDRESS_REVERSE;
    engine_stage_init(&state);
    for (size_t i = 0U; i < (sizeof(reversed) / sizeof(reversed[0])); i++)
    {
        expect_step(&state, &in, reversed[i], (uint8_t)(15U - reversed[i]), false);
    }

    // A first stage above the last runs round through stage 7
    in = make_inputs(6U, 1U);
    engine_stage_init(&state);
    for (size_t i = 0U; i < (sizeof(wrapped) / sizeof(wrapped[0])); i++)
    {
        expect_step(&state, &in, wrapped[i], (uint8_t)(15U - wrapped[i]), false);
    }

    // Steps 9 and 10 are taken as stages 1 and 2
    in = make_inputs(9U, 10U);
    engine_stage_init(&state);
    for (size_t i = 0U; i < (sizeof(folded) / sizeof(folded[0])); i++)
    {
        expect_step(&state, &in, folded[i], (uint8_t)(15U - folded[i]), false);
    }

    in = make_inputs(0U, 2U);
    g_address.direction = ADDRESS_PENDULUM;
    engine_stage_init(&state);
    for (size_t i = 0U; i < (sizeof(pendulum) / sizeof(pendulum[0])); i++)
    {
        expect_step(&state, &in, pendulum[i], (uint8_t)(15U - pendulum[i]), false);
    }
}



static void test_offset_moves_the_stage_when_it_begins_and_holds_it(void)
{
    engine_inputs_t      in = make_inputs(0U, 7U);
    engine_stage_state_t state;

    // Eight stages, so the fullest offset is four: stage 0 plays as stage
    // 4, with stage 4's length and mode
    set_step(12U, CONTROL(HOLD, 2U));
    in.offset.depth = 255U;
    engine_stage_init(&state);
    expect_step(&state, &in, 4U, 11U, true);

    // The offset going away does not move a stage that has begun
    in.offset.depth = 0U;
    expect_step(&state, &in, 4U, 11U, false);

    // And the pattern is where it would have been: stage 1 is next
    expect_step(&state, &in, 1U, 14U, false);

    // Stage 2, back by four, is stage 6
    in.offset.depth      = 255U;
    in.offset.b_negative = true;
    expect_step(&state, &in, 6U, 9U, false);
}



static void test_offset_stays_within_a_range_round_through_stage_seven(void)
{
    // Stages 6, 7, 0 and 1: four places, and a depth of 128 is one of them
    static const uint8_t expected[] = { 7U, 0U, 1U, 6U, 7U };
    engine_inputs_t      in = make_inputs(6U, 1U);
    engine_stage_state_t state;

    in.offset.depth = 128U;
    engine_stage_init(&state);
    for (size_t i = 0U; i < (sizeof(expected) / sizeof(expected[0])); i++)
    {
        expect_step(&state, &in, expected[i], (uint8_t)(15U - expected[i]), false);
    }
}



static void test_finished_one_shot_gives_no_step(void)
{
    const engine_inputs_t in = make_inputs(4U, 5U);
    engine_stage_state_t  state;
    engine_stage_state_t  before;
    engine_step_t         played;

    g_address.b_one_shot = true;
    set_step(13U, CONTROL(REPEAT, 2U));
    engine_stage_init(&state);
    expect_step(&state, &in, 4U, 11U, false);
    expect_step(&state, &in, 5U, 10U, false);
    expect_step(&state, &in, 5U, 10U, false); // The last stage plays in full

    before = state;
    (void)memset(&played, POISON_BYTE, sizeof(played));
    TEST_ASSERT(!engine_stage_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(POISON_BYTE, played.step); // Not written
    TEST_ASSERT_EQUAL(POISON_BYTE, played.note);
    TEST_ASSERT_EQUAL(0, memcmp(&before, &state, sizeof(state))); // Nor moved on

    // Init is the reset: the pattern plays again from its start
    engine_stage_init(&state);
    expect_step(&state, &in, 4U, 11U, false);
}



static void test_null_pointers_are_ignored(void)
{
    engine_inputs_t      in = make_inputs(0U, 7U);
    engine_stage_state_t state;
    engine_step_t        played;

    engine_stage_init(NULL); // Must not crash

    engine_stage_init(&state);
    (void)memset(&played, POISON_BYTE, sizeof(played));
    TEST_ASSERT(!engine_stage_step(NULL, &in, &played));
    TEST_ASSERT(!engine_stage_step(&state, NULL, &played));
    TEST_ASSERT(!engine_stage_step(&state, &in, NULL));

    in.p_address = NULL;
    TEST_ASSERT(!engine_stage_step(&state, &in, &played));

    in = make_inputs(0U, 7U);
    in.p_raw_steps = NULL;
    TEST_ASSERT(!engine_stage_step(&state, &in, &played));

    TEST_ASSERT_EQUAL(POISON_BYTE, played.step); // Nothing written
    TEST_ASSERT_EQUAL(0, state.address.position); // And the pattern has not moved
    TEST_ASSERT_EQUAL(0, state.played);
}



int main(void)
{
    RUN_TEST(test_init_goes_to_the_start_of_the_pattern);
    RUN_TEST(test_controls_all_off_is_a_plain_pattern_of_eight_steps);
    RUN_TEST(test_a_stage_lasts_its_length_in_steps);
    RUN_TEST(test_single_plays_the_first_step_only);
    RUN_TEST(test_hold_ties_every_step_but_the_last);
    RUN_TEST(test_rest_plays_nothing_for_the_whole_stage);
    RUN_TEST(test_a_note_value_of_zero_is_a_rest_in_any_mode);
    RUN_TEST(test_length_is_read_once_and_the_rest_on_every_step);
    RUN_TEST(test_addressing_controls_are_followed_as_stages);
    RUN_TEST(test_offset_moves_the_stage_when_it_begins_and_holds_it);
    RUN_TEST(test_offset_stays_within_a_range_round_through_stage_seven);
    RUN_TEST(test_finished_one_shot_gives_no_step);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
