/**
 * @file   test_seq_engines.c
 * @brief  Host unit tests for the sequencer core (core/seq.c): the choice
 *         of engine, ties and the step lights.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "seq.h"
#include "test_harness.h"
#include "seq_fixture.h"

/**
 * @brief Sets one step's four switches in the inputs.
 */
static void set_step(seq_inputs_t *p_in, uint8_t step, uint8_t value)
{
    const uint8_t byte_index = (uint8_t)(step / 2U);
    const uint8_t shift      = (0U == (step % 2U)) ? 4U : 0U;

    p_in->raw_steps[byte_index] =
        (uint8_t)((p_in->raw_steps[byte_index] & (uint8_t)~(uint8_t)(0x0FU << shift))
                  | (uint8_t)(value << shift));
}



/**
 * @brief Ticks the sequencer until a step begins and gives its number, or
 *        255 if none did.
 */
static uint8_t next_step(seq_state_t *p_state, const seq_inputs_t *p_in)
{
    seq_outputs_t out;

    return (0U != ticks_to_next_step(p_state, p_in, &out)) ? out.step : (uint8_t)255U;
}



static void test_mode_switches_choose_the_engine(void)
{
    // Stage 0 lasts two steps, repeating its note, which only the stage
    // engine acts on: it plays 0 0 1, and the plain engine 0 1 2
    static const uint8_t stage_modes[] = { 0x01U, 0x09U, 0xF9U };
    static const uint8_t plain_modes[] = { 0x00U, 0x06U, 0x07U, 0xF8U };

    for (size_t i = 0U; i < (sizeof(stage_modes) / sizeof(stage_modes[0])); i++)
    {
        seq_state_t  state;
        seq_inputs_t in = make_counting_inputs();

        seq_init(&state);
        in.tempo_raw = TEMPO_RAW_250_BPM;
        set_step(&in, 8U, 0x05U);
        in.raw_steps[PANEL_STEP_BYTE_COUNT] = stage_modes[i];
        TEST_ASSERT_EQUAL(0, next_step(&state, &in));
        TEST_ASSERT_EQUAL(0, next_step(&state, &in));
        TEST_ASSERT_EQUAL(1, next_step(&state, &in));
    }

    for (size_t i = 0U; i < (sizeof(plain_modes) / sizeof(plain_modes[0])); i++)
    {
        seq_state_t  state;
        seq_inputs_t in = make_counting_inputs();

        seq_init(&state);
        in.tempo_raw = TEMPO_RAW_250_BPM;
        set_step(&in, 8U, 0x05U);
        in.raw_steps[PANEL_STEP_BYTE_COUNT] = plain_modes[i];
        TEST_ASSERT_EQUAL(0, next_step(&state, &in));
        TEST_ASSERT_EQUAL(1, next_step(&state, &in));
        TEST_ASSERT_EQUAL(2, next_step(&state, &in));
    }
}



static void test_mode_two_is_the_klee_engine(void)
{
    // Stages 0 and 2 loaded, with values 3 and 4. The register moves a
    // stage a step: the active pair sits on 3 + 4, then on stages 1 and 3
    // (0 + 0, a rest), and after sixteen steps it is back
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    set_step(&in, 0U, 0x0BU);
    set_step(&in, 2U, 0x0CU);
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x02U;

    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT(!out.b_rest);
    TEST_ASSERT_EQUAL(6, out.semitone); // Note value 7 in the chromatic scale
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT(out.b_rest);
    for (unsigned int i = 0U; i < 15U; i++)
    {
        TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    }
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(6, out.semitone);

    // A reset loads the register again
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(1, out.step);
    in.b_reset = true;
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(6, out.semitone);
}



static void test_modes_three_to_five_are_the_chance_cartesian_and_euclidean_engines(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    // Chance, shuffling: the steps the generator's seed picks (see
    // test_engine_chance.c), which a reset does not start again
    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x23U;
    TEST_ASSERT_EQUAL(7, next_step(&state, &in));
    TEST_ASSERT_EQUAL(6, next_step(&state, &in));
    in.b_reset = true;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
    in.b_reset = false;

    // Cartesian, with y moving on every step: the diagonal
    seq_init(&state);
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x64U;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
    TEST_ASSERT_EQUAL(5, next_step(&state, &in));
    TEST_ASSERT_EQUAL(10, next_step(&state, &in));

    // Euclidean, one beat to the bar: step 0 sounds and step 1 is a rest
    seq_init(&state);
    (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT);
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x05U;
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT(!out.b_rest);
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT(out.b_rest);
}



static void test_the_engine_is_chosen_from_the_last_readable_panel(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    set_step(&in, 8U, 0x05U);
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x01U;
    in.b_steps_valid = false;
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT(0U != ticks_to_next_step(&state, &in, &out));
    TEST_ASSERT_EQUAL(0, out.step); // The stage engine's second step of stage 0
    TEST_ASSERT(!out.b_note_valid);
}



static void test_a_change_of_engine_starts_its_pattern_from_the_beginning(void)
{
    seq_state_t  state;
    seq_inputs_t in = make_counting_inputs();

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
    TEST_ASSERT_EQUAL(1, next_step(&state, &in));
    TEST_ASSERT_EQUAL(2, next_step(&state, &in));

    // A switch that chooses nothing new changes nothing
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x06U;
    TEST_ASSERT_EQUAL(3, next_step(&state, &in));

    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x01U;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
    TEST_ASSERT_EQUAL(1, next_step(&state, &in));

    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x00U;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
    TEST_ASSERT_EQUAL(1, next_step(&state, &in));

    // And the stage engine was sent back too, not left where it was
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x01U;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
}



static void test_reset_sends_the_chosen_engine_back(void)
{
    seq_state_t  state;
    seq_inputs_t in = make_counting_inputs();

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    set_step(&in, 9U, 0x07U); // Stage 1 lasts four steps
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x01U;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
    TEST_ASSERT_EQUAL(1, next_step(&state, &in));
    TEST_ASSERT_EQUAL(1, next_step(&state, &in));

    in.b_reset = true;
    TEST_ASSERT_EQUAL(0, next_step(&state, &in));
    in.b_reset = false;
    TEST_ASSERT_EQUAL(1, next_step(&state, &in));
}



static void test_a_tied_note_is_held_into_the_next_step(void)
{
    // 150 BPM: a step is 100 ticks. Stage 0 holds its note for three steps
    // and the other stages are rests, so one cycle is ten steps
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    set_step(&in, 0U, 0x05U);
    set_step(&in, 8U, 0x0AU);
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x01U;
    run_and_trace(&state, &in, 1000U, &gate, &clock_out);

    // Two whole steps and the half step of the gate length, as one gate;
    // the clock output still pulses on every step
    TEST_ASSERT_EQUAL(1, gate.rises);
    TEST_ASSERT_EQUAL(250, gate.high_ticks);
    TEST_ASSERT_EQUAL(10, clock_out.rises);
}



static void test_a_tie_does_not_shorten_a_longer_gate(void)
{
    // A gate of 30 pulses is longer than a step, so it ties by itself and
    // the engine's tie has nothing to add: stage 0 holds for two steps and
    // the rest that follows ends the gate on the boundary
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw   = TEMPO_RAW_150_BPM;
    in.gate.length = 30U;
    set_step(&in, 0U, 0x05U);
    set_step(&in, 8U, 0x09U);
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x01U;
    run_and_trace(&state, &in, 1U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(30, state.gate.pulses_left);
    run_and_trace(&state, &in, 899U, &gate, &clock_out);

    TEST_ASSERT_EQUAL(1, gate.rises);
    TEST_ASSERT_EQUAL(200, gate.high_ticks);
}



static void test_step_lights_flash_for_the_first_half_of_each_step(void)
{
    // 150 BPM: a step is 100 ticks and the clock output is high for 50
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    seq_init(&state);
    TEST_ASSERT_EQUAL(0, state.leds);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    for (unsigned int step = 0U; step < 18U; step++)
    {
        for (unsigned int tick = 0U; tick < 100U; tick++)
        {
            (void)memset(&out, POISON_BYTE, sizeof(out));
            seq_tick(&state, &in, &out);
            // Step 0 is a rest and is lit all the same
            TEST_ASSERT_EQUAL((tick < 50U) ? (1U << (step % 16U)) : 0U, out.leds);
        }
    }
}



static void test_step_lights_show_nothing_for_a_step_that_does_not_begin(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_counting_inputs();
    seq_outputs_t out;

    // A one-shot of two steps: the clock output goes on, the lights do not
    seq_init(&state);
    in.tempo_raw          = TEMPO_RAW_150_BPM;
    in.address.last_step  = 1U;
    in.address.b_one_shot = true;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(0x0001, out.leds);
    for (unsigned int tick = 0U; tick < 100U; tick++)
    {
        seq_tick(&state, &in, &out);
    }
    TEST_ASSERT_EQUAL(0x0002, out.leds);
    for (unsigned int tick = 0U; tick < 100U; tick++)
    {
        seq_tick(&state, &in, &out);
    }
    TEST_ASSERT(out.b_clock_out);
    TEST_ASSERT_EQUAL(0, out.leds);

    // Stopped: nothing, and a start begins with nothing left over
    seq_init(&state);
    in.address.b_one_shot = false;
    seq_tick(&state, &in, &out);
    in.b_run_button = true;
    for (unsigned int tick = 0U; tick < 20U; tick++)
    {
        seq_tick(&state, &in, &out);
    }
    TEST_ASSERT_EQUAL(TRANSPORT_STOPPED, out.transport);
    TEST_ASSERT_EQUAL(0, out.leds);
}



static void test_step_lights_show_the_klee_register(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    // Stages 0 and 2 loaded: both lit, then stages 1 and 3
    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    set_step(&in, 0U, 0x0BU);
    set_step(&in, 2U, 0x0CU);
    in.raw_steps[PANEL_STEP_BYTE_COUNT] = 0x02U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(0x0005, out.leds);
    for (unsigned int tick = 0U; tick < 100U; tick++)
    {
        seq_tick(&state, &in, &out);
    }
    TEST_ASSERT_EQUAL(0x000A, out.leds);
}



int main(void)
{
    RUN_TEST(test_mode_switches_choose_the_engine);
    RUN_TEST(test_mode_two_is_the_klee_engine);
    RUN_TEST(test_modes_three_to_five_are_the_chance_cartesian_and_euclidean_engines);
    RUN_TEST(test_the_engine_is_chosen_from_the_last_readable_panel);
    RUN_TEST(test_a_change_of_engine_starts_its_pattern_from_the_beginning);
    RUN_TEST(test_reset_sends_the_chosen_engine_back);
    RUN_TEST(test_a_tied_note_is_held_into_the_next_step);
    RUN_TEST(test_a_tie_does_not_shorten_a_longer_gate);
    RUN_TEST(test_step_lights_flash_for_the_first_half_of_each_step);
    RUN_TEST(test_step_lights_show_nothing_for_a_step_that_does_not_begin);
    RUN_TEST(test_step_lights_show_the_klee_register);
    return TEST_RESULT();
}
