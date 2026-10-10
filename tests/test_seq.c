/**
 * @file   test_seq.c
 * @brief  Host unit tests for the sequencer core (core/seq.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "seq.h"
#include "test_harness.h"

// Fills every byte of an object so tests can prove a field really was written
#define POISON_BYTE (0xA5)

/**
 * @brief Inputs with everything valid, all step bits 0 and tempo 0.
 */
static seq_inputs_t make_valid_inputs(void)
{
    seq_inputs_t in;

    (void)memset(&in, 0, sizeof(in));
    in.b_steps_valid = true;
    in.b_tempo_valid = true;
    return in;
}



static void test_layout_constants(void)
{
    // 16 steps of 4 bits fill exactly 8 bytes, with no partial byte
    TEST_ASSERT_EQUAL(16, SEQ_STEP_COUNT);
    TEST_ASSERT_EQUAL(8, SEQ_RAW_BYTE_COUNT);
    TEST_ASSERT_EQUAL(0, SEQ_BITS_PER_BYTE % SEQ_NOTE_BITS);
    TEST_ASSERT_EQUAL(SEQ_STEP_COUNT * SEQ_NOTE_BITS, SEQ_RAW_BYTE_COUNT * SEQ_BITS_PER_BYTE);
    TEST_ASSERT_EQUAL(15, SEQ_NOTE_MAX);
}



static void test_init_clears_tempo(void)
{
    seq_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    seq_init(&state);
    TEST_ASSERT_EQUAL(0, state.last_tempo_raw);
}



static void test_init_starts_at_step_zero(void)
{
    seq_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    seq_init(&state);
    TEST_ASSERT_EQUAL(0, state.current_step);
}



/**
 * @brief Inputs with everything valid and step n holding the note value n.
 */
static seq_inputs_t make_counting_inputs(void)
{
    seq_inputs_t in = make_valid_inputs();

    for (uint8_t byte_index = 0U; byte_index < SEQ_RAW_BYTE_COUNT; byte_index++)
    {
        const uint8_t even_step = (uint8_t)(byte_index * 2U);
        in.raw_steps[byte_index] = (uint8_t)((uint8_t)(even_step << SEQ_NOTE_BITS) | (uint8_t)(even_step + 1U));
    }
    return in;
}



static void test_first_tick_plays_step_zero(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.raw_steps[0] = 0xC3U; // step 0 = 0xC, step 1 = 0x3
    (void)memset(&out, POISON_BYTE, sizeof(out));
    seq_tick(&state, &in, &out);

    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(0xC, out.note);
}



static void test_step_advances_once_per_tick_and_wraps(void)
{
    seq_state_t  state;
    seq_inputs_t in = make_counting_inputs();

    seq_init(&state);
    // Three times round the pattern: each tick plays the next step, and its note.
    // Step n holds the note value n, so a wrong byte or nibble shows up as a wrong number
    for (unsigned int tick = 0U; tick < (3U * SEQ_STEP_COUNT); tick++)
    {
        seq_outputs_t      out;
        const unsigned int expected_step = tick % SEQ_STEP_COUNT;

        seq_tick(&state, &in, &out);
        TEST_ASSERT_EQUAL(expected_step, out.step);
        TEST_ASSERT_EQUAL(expected_step, out.note);
        TEST_ASSERT(out.b_note_valid);
        TEST_ASSERT_EQUAL((expected_step + 1U) % SEQ_STEP_COUNT, state.current_step);
    }
}



static void test_note_comes_from_this_ticks_inputs(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.raw_steps[0] = 0x12U; // step 0 = 1, step 1 = 2
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(1, out.note);

    // Step 1 is changed on the panel just before its turn: the new value plays
    in.raw_steps[0] = 0x19U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT_EQUAL(9, out.note);
}



static void test_step_advances_when_steps_are_invalid(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    seq_tick(&state, &in, &out); // Step 0
    seq_tick(&state, &in, &out); // Step 1

    // A failed read still takes its turn: step 2 plays as note 0
    in.b_steps_valid = false;
    (void)memset(&out, POISON_BYTE, sizeof(out));
    seq_tick(&state, &in, &out);
    TEST_ASSERT(!out.b_note_valid);
    TEST_ASSERT_EQUAL(2, out.step);
    TEST_ASSERT_EQUAL(0, out.note);

    // So the pattern is where it would have been once the reads work again
    in.b_steps_valid = true;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(3, out.step);
    TEST_ASSERT_EQUAL(3, out.note);
}



static void test_step_advances_when_tempo_is_invalid(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.b_tempo_valid = false;
    seq_tick(&state, &in, &out);
    seq_tick(&state, &in, &out);

    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT_EQUAL(1, out.note);
}



static void test_notes_decode_high_nibble_first(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.raw_steps[0] = 0xA5U; // step 0 = 0xA, step 1 = 0x5
    in.raw_steps[1] = 0x0FU; // step 2 = 0x0, step 3 = 0xF
    // One step per tick: the high nibble of each byte plays before the low one
    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_note_valid);
    TEST_ASSERT_EQUAL(0xA, out.note);
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(0x5, out.note);
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(0x0, out.note);
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(0xF, out.note);
}



static void test_each_step_is_independent(void)
{
    // Set one step at a time to the maximum; every other step must play as 0
    for (uint8_t target = 0U; target < SEQ_STEP_COUNT; target++)
    {
        seq_state_t   state;
        seq_inputs_t  in = make_valid_inputs();
        const uint8_t byte_index = (uint8_t)(target / 2U);
        const bool    b_high     = (0U == (target % 2U));

        seq_init(&state);
        in.raw_steps[byte_index] = b_high ? 0xF0U : 0x0FU;

        for (uint8_t step = 0U; step < SEQ_STEP_COUNT; step++)
        {
            seq_outputs_t out;

            seq_tick(&state, &in, &out);
            TEST_ASSERT_EQUAL(step, out.step);
            TEST_ASSERT_EQUAL((step == target) ? SEQ_NOTE_MAX : 0U, out.note);
        }
    }
}



static void test_invalid_steps_give_a_zero_note(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    (void)memset(in.raw_steps, 0xFF, sizeof(in.raw_steps));
    (void)memset(&out, POISON_BYTE, sizeof(out));
    in.b_steps_valid = false;
    seq_tick(&state, &in, &out);

    TEST_ASSERT(!out.b_note_valid);
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(0, out.note);
}



static void test_delay_is_tempo_divided_by_four(void)
{
    static const uint16_t tempo_raw[]   = { 0U, 1U, 3U, 4U, 7U, 8U, 512U, 1020U, 1023U };
    static const uint16_t expected_ms[] = { 0U, 0U, 0U, 1U, 1U, 2U, 128U, 255U,  255U  };
    seq_state_t state;

    seq_init(&state);
    for (size_t i = 0U; i < (sizeof(tempo_raw) / sizeof(tempo_raw[0])); i++)
    {
        seq_inputs_t  in = make_valid_inputs();
        seq_outputs_t out;

        in.tempo_raw = tempo_raw[i];
        seq_tick(&state, &in, &out);
        TEST_ASSERT_EQUAL(expected_ms[i], out.delay_ms);
    }
}



static void test_delay_is_zero_before_any_valid_tempo(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.b_tempo_valid = false;
    in.tempo_raw     = 800U; // Must be ignored: the reading is flagged invalid
    seq_tick(&state, &in, &out);

    TEST_ASSERT_EQUAL(0, out.delay_ms);
}



static void test_invalid_tempo_reuses_last_valid(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = 400U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(100, out.delay_ms);

    // A failed read keeps the previous delay, for as many ticks as it lasts
    in.b_tempo_valid = false;
    in.tempo_raw     = 0U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(100, out.delay_ms);
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(100, out.delay_ms);

    // The next good read takes over again
    in.b_tempo_valid = true;
    in.tempo_raw     = 40U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(10, out.delay_ms);
}



static void test_steps_and_tempo_validity_are_independent(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.raw_steps[0]  = 0x12U;
    in.tempo_raw     = 200U;
    in.b_steps_valid = false;
    seq_tick(&state, &in, &out);
    TEST_ASSERT(!out.b_note_valid);
    TEST_ASSERT_EQUAL(50, out.delay_ms); // Tempo still applies when steps fail

    in.b_steps_valid = true;
    in.b_tempo_valid = false;
    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_note_valid);
    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT_EQUAL(2, out.note); // The step still decodes when tempo fails
    TEST_ASSERT_EQUAL(50, out.delay_ms);
}



static void test_tick_does_not_change_inputs(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_inputs_t  in_before;
    seq_outputs_t out;

    seq_init(&state);
    in.raw_steps[3] = 0x9CU;
    in.tempo_raw    = 123U;
    in_before = in;
    seq_tick(&state, &in, &out);

    TEST_ASSERT_EQUAL(0, memcmp(in_before.raw_steps, in.raw_steps, sizeof(in.raw_steps)));
    TEST_ASSERT_EQUAL(in_before.tempo_raw, in.tempo_raw);
    TEST_ASSERT(in_before.b_steps_valid == in.b_steps_valid);
    TEST_ASSERT(in_before.b_tempo_valid == in.b_tempo_valid);
}



static void test_null_pointers_are_ignored(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(NULL); // Must not crash

    seq_init(&state);
    in.tempo_raw = 400U;
    (void)memset(&out, POISON_BYTE, sizeof(out));

    seq_tick(NULL, &in, &out);
    TEST_ASSERT_EQUAL(POISON_BYTE, out.note); // Outputs untouched

    seq_tick(&state, NULL, &out);
    TEST_ASSERT_EQUAL(POISON_BYTE, out.note);

    seq_tick(&state, &in, NULL);
    TEST_ASSERT_EQUAL(0, state.last_tempo_raw); // State untouched
    TEST_ASSERT_EQUAL(0, state.current_step);
}



int main(void)
{
    RUN_TEST(test_layout_constants);
    RUN_TEST(test_init_clears_tempo);
    RUN_TEST(test_init_starts_at_step_zero);
    RUN_TEST(test_first_tick_plays_step_zero);
    RUN_TEST(test_step_advances_once_per_tick_and_wraps);
    RUN_TEST(test_note_comes_from_this_ticks_inputs);
    RUN_TEST(test_step_advances_when_steps_are_invalid);
    RUN_TEST(test_step_advances_when_tempo_is_invalid);
    RUN_TEST(test_notes_decode_high_nibble_first);
    RUN_TEST(test_each_step_is_independent);
    RUN_TEST(test_invalid_steps_give_a_zero_note);
    RUN_TEST(test_delay_is_tempo_divided_by_four);
    RUN_TEST(test_delay_is_zero_before_any_valid_tempo);
    RUN_TEST(test_invalid_tempo_reuses_last_valid);
    RUN_TEST(test_steps_and_tempo_validity_are_independent);
    RUN_TEST(test_tick_does_not_change_inputs);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
