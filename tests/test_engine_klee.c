/**
 * @file   test_engine_klee.c
 * @brief  Host unit tests for the Klee engine (core/engine_klee.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "engine_klee.h"
#include "panel.h"
#include "test_harness.h"

#define POISON_BYTE (0xA5)

#define STAGE_COUNT (16U)
#define MODE_BYTE   (8U)    // Where the mode switches are in the raw bytes
#define PATTERN     (0x08U) // A stage's pattern switch; its value is the rest
#define LOAD        (0x08U) // The engine's mode switches
#define INVERT      (0x10U)
#define MERGE       (0x20U)
#define CHANCE(n)   ((uint8_t)((n) << 6U)) // Turns in eight of the bit coming round
#define SEED        (0xACE1U)

static uint8_t g_raw[PANEL_RAW_BYTE_COUNT]; // The panel the inputs point at

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
 * @brief Inputs for a panel with every switch off and no addressing
 *        controls, which the engine does not use.
 */
static engine_inputs_t make_inputs(void)
{
    engine_inputs_t in;

    (void)memset(g_raw, 0, sizeof(g_raw));
    in.p_raw_steps = g_raw;
    in.p_address   = NULL;
    in.p_prng      = NULL;
    in.offset.depth      = 0U;
    in.offset.b_negative = false;
    return in;
}



/**
 * @brief Asks for one step and checks its number and note value.
 */
static void expect_step(engine_klee_state_t *p_state, const engine_inputs_t *p_in,
                        uint8_t step, uint8_t note)
{
    engine_step_t played;

    (void)memset(&played, POISON_BYTE, sizeof(played));
    TEST_ASSERT(engine_klee_step(p_state, p_in, &played));
    TEST_ASSERT_EQUAL(step, played.step);
    TEST_ASSERT_EQUAL(note, played.note);
}



static void test_init_leaves_a_load_due(void)
{
    engine_klee_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    engine_klee_init(&state);
    TEST_ASSERT_EQUAL(0, state.stages);
    TEST_ASSERT_EQUAL(0, state.shown);
    TEST_ASSERT_EQUAL(0, state.position);
    TEST_ASSERT(!state.b_loaded);
    TEST_ASSERT(!state.b_load_switch);
}



static void test_first_step_loads_the_pattern_switches(void)
{
    const engine_inputs_t in = make_inputs();
    engine_klee_state_t   state;

    set_step(0U, PATTERN);
    set_step(5U, PATTERN | 3U);
    set_step(15U, PATTERN);
    set_step(7U, 7U); // A value alone loads nothing
    engine_klee_init(&state);
    expect_step(&state, &in, 0U, 3U);
    TEST_ASSERT_EQUAL(0x8021, state.stages);
    TEST_ASSERT(state.b_loaded);
}



static void test_one_bit_goes_round_playing_each_value(void)
{
    // Stage n holds the value n modulo 8, and one bit is loaded at stage 0
    const engine_inputs_t in = make_inputs();
    engine_klee_state_t   state;

    for (uint8_t stage = 0U; stage < STAGE_COUNT; stage++)
    {
        set_step(stage, (uint8_t)(stage % 8U));
    }
    set_step(0U, PATTERN);
    engine_klee_init(&state);
    for (unsigned int count = 0U; count < (2U * STAGE_COUNT); count++)
    {
        const uint8_t stage = (uint8_t)(count % STAGE_COUNT);

        expect_step(&state, &in, stage, (uint8_t)(stage % 8U));
        TEST_ASSERT_EQUAL(1U << stage, state.stages);
    }
}



static void test_the_note_is_the_sum_under_the_active_stages(void)
{
    // Bits at stages 0 and 1, over values 1 2 4 0 0 ...: 3, then 6, then 4
    const engine_inputs_t in = make_inputs();
    engine_klee_state_t   state;

    set_step(0U, PATTERN | 1U);
    set_step(1U, PATTERN | 2U);
    set_step(2U, 4U);
    engine_klee_init(&state);
    expect_step(&state, &in, 0U, 3U);
    expect_step(&state, &in, 1U, 6U);
    expect_step(&state, &in, 2U, 4U);
    expect_step(&state, &in, 3U, 0U); // Nothing under them: a rest
}



static void test_the_sum_is_limited_to_the_highest_note(void)
{
    const engine_inputs_t in = make_inputs();
    engine_klee_state_t   state;

    // Fifteen exactly, then sixteen, then all sixteen stages at 7
    set_step(0U, PATTERN | 7U);
    set_step(1U, PATTERN | 7U);
    set_step(2U, PATTERN | 1U);
    engine_klee_init(&state);
    expect_step(&state, &in, 0U, 15U);

    set_step(2U, PATTERN | 2U);
    engine_klee_init(&state);
    expect_step(&state, &in, 0U, 15U);

    (void)memset(g_raw, 0xFF, PANEL_STEP_BYTE_COUNT);
    engine_klee_init(&state);
    expect_step(&state, &in, 0U, 15U);
    expect_step(&state, &in, 1U, 15U);
}



static void test_values_are_read_on_every_step_and_the_pattern_only_at_a_load(void)
{
    const engine_inputs_t in = make_inputs();
    engine_klee_state_t   state;

    set_step(0U, PATTERN | 1U);
    set_step(1U, 2U);
    engine_klee_init(&state);
    expect_step(&state, &in, 0U, 1U);

    // A new value where the bit now is plays at once; a pattern switch
    // turned on, and the loaded one turned off, change nothing
    set_step(1U, 5U);
    set_step(0U, 1U);
    set_step(9U, PATTERN | 7U);
    expect_step(&state, &in, 1U, 5U);
    TEST_ASSERT_EQUAL(0x0002, state.stages);
}



static void test_moving_the_load_switch_either_way_loads(void)
{
    const engine_inputs_t in = make_inputs();
    engine_klee_state_t   state;

    set_step(0U, PATTERN | 1U);
    set_step(4U, 6U);
    engine_klee_init(&state);
    expect_step(&state, &in, 0U, 1U);
    expect_step(&state, &in, 1U, 0U);
    expect_step(&state, &in, 2U, 0U);

    // On: the pattern as it is now, from the start
    set_step(4U, PATTERN | 6U);
    g_raw[MODE_BYTE] = LOAD;
    expect_step(&state, &in, 0U, 7U);
    TEST_ASSERT_EQUAL(0x0011, state.stages);

    // Left on, it is not a load
    expect_step(&state, &in, 1U, 0U);

    // Off again is one
    g_raw[MODE_BYTE] = 0U;
    expect_step(&state, &in, 0U, 7U);
    expect_step(&state, &in, 1U, 0U);
}



static void test_a_load_switch_on_at_the_start_is_not_a_second_load(void)
{
    const engine_inputs_t in = make_inputs();
    engine_klee_state_t   state;

    set_step(0U, PATTERN | 1U);
    g_raw[MODE_BYTE] = LOAD;
    engine_klee_init(&state);
    expect_step(&state, &in, 0U, 1U);
    expect_step(&state, &in, 1U, 0U);
    TEST_ASSERT(state.b_load_switch);
}



static void test_invert_turns_the_pattern_over_as_it_comes_round(void)
{
    // One bit at stage 0 and every value 1, so the note is the number of
    // active stages: one for sixteen steps, then one more each step as
    // zeros come round as ones
    const engine_inputs_t in = make_inputs();
    engine_klee_state_t   state;

    (void)memset(g_raw, 0x11, PANEL_STEP_BYTE_COUNT);
    set_step(0U, PATTERN | 1U);
    g_raw[MODE_BYTE] = INVERT;
    engine_klee_init(&state);
    expect_step(&state, &in, 0U, 1U);
    TEST_ASSERT_EQUAL(0x0001, state.stages);
    expect_step(&state, &in, 1U, 2U);
    TEST_ASSERT_EQUAL(0x0003, state.stages);
    for (unsigned int i = 0U; i < 14U; i++)
    {
        TEST_ASSERT(engine_klee_step(&state, &in, &(engine_step_t){ 0U, 0U, false }));
    }
    TEST_ASSERT_EQUAL(0xFFFF, state.stages);

    // The one that was loaded comes round as a zero, and after thirty-two
    // steps in all the register is as it was loaded
    expect_step(&state, &in, 0U, 15U);
    TEST_ASSERT_EQUAL(0xFFFE, state.stages);
    for (unsigned int i = 0U; i < 15U; i++)
    {
        TEST_ASSERT(engine_klee_step(&state, &in, &(engine_step_t){ 0U, 0U, false }));
    }
    TEST_ASSERT_EQUAL(0x0000, state.stages);
    expect_step(&state, &in, 0U, 1U);
    TEST_ASSERT_EQUAL(0x0001, state.stages);
}



/**
 * @brief Loads one bit at stage 0 with the mode switches given, then moves
 *        the register on and checks it after each move.
 */
static void expect_moves(engine_inputs_t in, uint8_t mode, const uint16_t *p_stages,
                         size_t count)
{
    engine_klee_state_t state;
    engine_step_t       played;

    (void)memset(g_raw, 0, sizeof(g_raw));
    set_step(0U, PATTERN);
    g_raw[MODE_BYTE] = mode;
    engine_klee_init(&state);
    TEST_ASSERT(engine_klee_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(0x0001, state.stages);
    for (size_t i = 0U; i < count; i++)
    {
        TEST_ASSERT(engine_klee_step(&state, &in, &played));
        TEST_ASSERT_EQUAL(p_stages[i], state.stages);
    }
}



static void test_chance_turns_the_bit_coming_round_that_often(void)
{
    // The generator's first bytes from this seed have low three bits 7, 3,
    // 6, 4, 0, 5 (see test_prng.c for the bytes). A load draws nothing. A
    // move turns the bit over if its draw is among the top one, or three,
    // of eight
    static const uint16_t one_in_eight[]   = { 0x0003U, 0x0006U, 0x000CU };
    static const uint16_t three_in_eight[] =
    {
        0x0003U, 0x0006U, 0x000DU, 0x001AU, 0x0034U, 0x0069U
    };
    static const uint16_t with_invert[]    = { 0x0002U, 0x0005U };
    static const uint16_t locked[]         = { 0x0002U, 0x0004U, 0x0008U };
    engine_inputs_t       in = make_inputs();
    prng_state_t          prng;

    in.p_prng = &prng;
    prng_init(&prng, SEED);
    expect_moves(in, CHANCE(1U), one_in_eight, 3U);
    prng_init(&prng, SEED);
    expect_moves(in, CHANCE(3U), three_in_eight, 6U);

    // A turn on top of invert puts the bit back as it was
    prng_init(&prng, SEED);
    expect_moves(in, (uint8_t)(CHANCE(1U) | INVERT), with_invert, 2U);

    // No chance, or no random numbers to draw from: the pattern is locked
    prng_init(&prng, SEED);
    expect_moves(in, 0U, locked, 3U);
    in.p_prng = NULL;
    expect_moves(in, CHANCE(3U), locked, 3U);
}



static void test_merge_ties_every_step(void)
{
    const engine_inputs_t in = make_inputs();
    engine_klee_state_t   state;
    engine_step_t         played;

    set_step(0U, PATTERN | 1U);
    engine_klee_init(&state);
    TEST_ASSERT(engine_klee_step(&state, &in, &played));
    TEST_ASSERT(!played.b_tie);

    g_raw[MODE_BYTE] = MERGE;
    TEST_ASSERT(engine_klee_step(&state, &in, &played));
    TEST_ASSERT(played.b_tie);
    TEST_ASSERT_EQUAL(1, played.step); // And it is not a load
}



static void test_offset_turns_the_register_for_the_sum_and_not_the_one_kept(void)
{
    // Stage n holds the value n modulo 8, and one bit is loaded at stage 0
    engine_inputs_t     in = make_inputs();
    engine_klee_state_t state;

    for (uint8_t stage = 0U; stage < STAGE_COUNT; stage++)
    {
        set_step(stage, (uint8_t)(stage % 8U));
    }
    set_step(0U, PATTERN);
    engine_klee_init(&state);

    // A depth of 128 is four of the sixteen stages: the bit at stage 0
    // sums as stage 4, and the step given is still the count
    in.offset.depth = 128U;
    expect_step(&state, &in, 0U, 4U);
    TEST_ASSERT_EQUAL(0x0001, state.stages);
    TEST_ASSERT_EQUAL(0x0010, state.shown);
    expect_step(&state, &in, 1U, 5U);
    TEST_ASSERT_EQUAL(0x0002, state.stages);
    TEST_ASSERT_EQUAL(0x0020, state.shown);

    // Back by eight: stage 2 sums as stage 10
    in.offset.depth      = 255U;
    in.offset.b_negative = true;
    expect_step(&state, &in, 2U, 2U);
    TEST_ASSERT_EQUAL(0x0004, state.stages);
    TEST_ASSERT_EQUAL(0x0400, state.shown);

    // With no offset the register is where it would have been
    in.offset.depth = 0U;
    expect_step(&state, &in, 3U, 3U);
    TEST_ASSERT_EQUAL(0x0008, state.stages);
    TEST_ASSERT_EQUAL(0x0008, state.shown);
}



static void test_offset_brings_a_bit_round_as_it_is_whatever_invert_says(void)
{
    engine_inputs_t     in = make_inputs();
    engine_klee_state_t state;

    // One bit at stage 14, on by four with invert on: stage 2 and no other
    set_step(14U, PATTERN);
    set_step(2U, 5U);
    g_raw[MODE_BYTE] = INVERT;
    in.offset.depth  = 128U;
    engine_klee_init(&state);
    expect_step(&state, &in, 0U, 5U);
    TEST_ASSERT_EQUAL(0x4000, state.stages);
    TEST_ASSERT_EQUAL(0x0004, state.shown);
}



static void test_other_mode_switches_change_nothing(void)
{
    const engine_inputs_t in = make_inputs();
    engine_klee_state_t   state;
    engine_step_t         played;

    set_step(0U, PATTERN | 1U);
    set_step(1U, 2U);
    g_raw[MODE_BYTE] = 0x07U;
    engine_klee_init(&state);
    expect_step(&state, &in, 0U, 1U);
    TEST_ASSERT(engine_klee_step(&state, &in, &played));
    TEST_ASSERT_EQUAL(1, played.step);
    TEST_ASSERT_EQUAL(2, played.note);
    TEST_ASSERT(!played.b_tie);
    TEST_ASSERT_EQUAL(0x0002, state.stages);
}



static void test_null_pointers_are_ignored(void)
{
    engine_inputs_t     in = make_inputs();
    engine_klee_state_t state;
    engine_step_t       played;

    engine_klee_init(NULL); // Must not crash

    set_step(0U, PATTERN | 1U);
    engine_klee_init(&state);
    (void)memset(&played, POISON_BYTE, sizeof(played));
    TEST_ASSERT(!engine_klee_step(NULL, &in, &played));
    TEST_ASSERT(!engine_klee_step(&state, NULL, &played));
    TEST_ASSERT(!engine_klee_step(&state, &in, NULL));

    in.p_raw_steps = NULL;
    TEST_ASSERT(!engine_klee_step(&state, &in, &played));

    TEST_ASSERT_EQUAL(POISON_BYTE, played.step); // Nothing written
    TEST_ASSERT(!state.b_loaded);                // And nothing loaded
}



int main(void)
{
    RUN_TEST(test_init_leaves_a_load_due);
    RUN_TEST(test_first_step_loads_the_pattern_switches);
    RUN_TEST(test_one_bit_goes_round_playing_each_value);
    RUN_TEST(test_the_note_is_the_sum_under_the_active_stages);
    RUN_TEST(test_the_sum_is_limited_to_the_highest_note);
    RUN_TEST(test_values_are_read_on_every_step_and_the_pattern_only_at_a_load);
    RUN_TEST(test_moving_the_load_switch_either_way_loads);
    RUN_TEST(test_a_load_switch_on_at_the_start_is_not_a_second_load);
    RUN_TEST(test_invert_turns_the_pattern_over_as_it_comes_round);
    RUN_TEST(test_chance_turns_the_bit_coming_round_that_often);
    RUN_TEST(test_merge_ties_every_step);
    RUN_TEST(test_offset_turns_the_register_for_the_sum_and_not_the_one_kept);
    RUN_TEST(test_offset_brings_a_bit_round_as_it_is_whatever_invert_says);
    RUN_TEST(test_other_mode_switches_change_nothing);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
