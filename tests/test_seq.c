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



static void test_notes_decode_high_nibble_first(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.raw_steps[0] = 0xA5U; // step 0 = 0xA, step 1 = 0x5
    in.raw_steps[1] = 0x0FU; // step 2 = 0x0, step 3 = 0xF
    seq_tick(&state, &in, &out);

    TEST_ASSERT(out.b_notes_valid);
    TEST_ASSERT_EQUAL(0xA, out.notes[0]);
    TEST_ASSERT_EQUAL(0x5, out.notes[1]);
    TEST_ASSERT_EQUAL(0x0, out.notes[2]);
    TEST_ASSERT_EQUAL(0xF, out.notes[3]);
}



static void test_notes_decode_every_step(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    // Give step n the note value n, so a wrong byte or nibble shows up as a wrong number
    for (uint8_t byte_index = 0U; byte_index < SEQ_RAW_BYTE_COUNT; byte_index++)
    {
        const uint8_t even_step = (uint8_t)(byte_index * 2U);
        in.raw_steps[byte_index] = (uint8_t)((uint8_t)(even_step << SEQ_NOTE_BITS) | (uint8_t)(even_step + 1U));
    }
    seq_tick(&state, &in, &out);

    for (uint8_t step = 0U; step < SEQ_STEP_COUNT; step++)
    {
        TEST_ASSERT_EQUAL(step, out.notes[step]);
    }
}



static void test_each_step_is_independent(void)
{
    seq_state_t state;

    seq_init(&state);
    // Set one step at a time to the maximum; every other step must stay 0
    for (uint8_t target = 0U; target < SEQ_STEP_COUNT; target++)
    {
        seq_inputs_t  in = make_valid_inputs();
        seq_outputs_t out;
        const uint8_t byte_index = (uint8_t)(target / 2U);
        const bool    b_high     = (0U == (target % 2U));

        in.raw_steps[byte_index] = b_high ? 0xF0U : 0x0FU;
        seq_tick(&state, &in, &out);

        for (uint8_t step = 0U; step < SEQ_STEP_COUNT; step++)
        {
            TEST_ASSERT_EQUAL((step == target) ? SEQ_NOTE_MAX : 0U, out.notes[step]);
        }
    }
}



static void test_invalid_steps_give_zeroed_notes(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    (void)memset(in.raw_steps, 0xFF, sizeof(in.raw_steps));
    (void)memset(&out, POISON_BYTE, sizeof(out));
    in.b_steps_valid = false;
    seq_tick(&state, &in, &out);

    TEST_ASSERT(!out.b_notes_valid);
    for (uint8_t step = 0U; step < SEQ_STEP_COUNT; step++)
    {
        TEST_ASSERT_EQUAL(0, out.notes[step]);
    }
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
    TEST_ASSERT(!out.b_notes_valid);
    TEST_ASSERT_EQUAL(50, out.delay_ms); // Tempo still applies when steps fail

    in.b_steps_valid = true;
    in.b_tempo_valid = false;
    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_notes_valid);
    TEST_ASSERT_EQUAL(1, out.notes[0]); // Steps still decode when tempo fails
    TEST_ASSERT_EQUAL(2, out.notes[1]);
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
    TEST_ASSERT_EQUAL(POISON_BYTE, out.notes[0]); // Outputs untouched

    seq_tick(&state, NULL, &out);
    TEST_ASSERT_EQUAL(POISON_BYTE, out.notes[0]);

    seq_tick(&state, &in, NULL);
    TEST_ASSERT_EQUAL(0, state.last_tempo_raw); // State untouched
}



int main(void)
{
    RUN_TEST(test_layout_constants);
    RUN_TEST(test_init_clears_tempo);
    RUN_TEST(test_notes_decode_high_nibble_first);
    RUN_TEST(test_notes_decode_every_step);
    RUN_TEST(test_each_step_is_independent);
    RUN_TEST(test_invalid_steps_give_zeroed_notes);
    RUN_TEST(test_delay_is_tempo_divided_by_four);
    RUN_TEST(test_delay_is_zero_before_any_valid_tempo);
    RUN_TEST(test_invalid_tempo_reuses_last_valid);
    RUN_TEST(test_steps_and_tempo_validity_are_independent);
    RUN_TEST(test_tick_does_not_change_inputs);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
