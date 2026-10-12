/**
 * @file   test_seq_gate.c
 * @brief  Host unit tests for the sequencer core (core/seq.c): the gate, the
 *         clock output and the pitch CV.
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

static void test_gate_opens_with_a_pitch_for_its_length(void)
{
    // 150 BPM: a step is 100 ticks and 12 pulses are exactly 50
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT); // Every step a pitch
    (void)memset(&out, POISON_BYTE, sizeof(out));
    seq_tick(&state, &in, &out); // Step 0 begins
    TEST_ASSERT(out.b_gate);     // The gate opens on the tick the step begins
    TEST_ASSERT(out.b_clock_out);
    run_and_trace(&state, &in, 99U, &gate, &clock_out);
    TEST_ASSERT(!gate.b_level);

    // Sixteen whole steps: one gate each, open for half of it
    gate.rises      = 0U;
    gate.high_ticks = 0U;
    run_and_trace(&state, &in, 1600U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(16, gate.rises);
    TEST_ASSERT_EQUAL(16U * 50U, gate.high_ticks);
}



static void test_gate_length_is_counted_in_pulses(void)
{
    // At 150 BPM pulse n of a step falls 25 * n / 6 ticks into it, rounded up
    static const uint8_t      lengths[]        = { 1U, 6U, 18U, 23U };
    static const unsigned int expected_ticks[] = { 5U, 25U, 75U, 96U };

    for (size_t i = 0U; i < (sizeof(lengths) / sizeof(lengths[0])); i++)
    {
        seq_state_t   state;
        seq_inputs_t  in = make_valid_inputs();
        level_trace_t gate      = { 0U, 0U, false };
        level_trace_t clock_out = { 0U, 0U, false };

        seq_init(&state);
        in.tempo_raw   = TEMPO_RAW_150_BPM;
        in.gate.length = lengths[i];
        (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT);
        run_and_trace(&state, &in, 100U, &gate, &clock_out); // All of step 0
        TEST_ASSERT(!gate.b_level);
        gate.high_ticks = 0U;
        gate.rises      = 0U;
        run_and_trace(&state, &in, 100U, &gate, &clock_out); // All of step 1

        TEST_ASSERT_EQUAL(1, gate.rises);
        TEST_ASSERT_EQUAL(expected_ticks[i], gate.high_ticks);
        TEST_ASSERT(!gate.b_level); // Closed before step 2
    }
}



static void test_gate_stays_closed_for_a_rest_and_an_unreadable_step(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw    = TEMPO_RAW_150_BPM;
    in.raw_steps[0] = 0x50U; // Step 0 a pitch, step 1 a rest
    in.raw_steps[1] = 0x55U; // Steps 2 and 3 pitches

    run_and_trace(&state, &in, 100U, &gate, &clock_out); // Step 0
    TEST_ASSERT_EQUAL(1, gate.rises);
    run_and_trace(&state, &in, 100U, &gate, &clock_out); // Step 1, a rest
    TEST_ASSERT_EQUAL(1, gate.rises);

    in.b_steps_valid = false;
    run_and_trace(&state, &in, 100U, &gate, &clock_out); // Step 2, not readable
    TEST_ASSERT_EQUAL(1, gate.rises);

    in.b_steps_valid = true;
    run_and_trace(&state, &in, 100U, &gate, &clock_out); // Step 3
    TEST_ASSERT_EQUAL(2, gate.rises);
    TEST_ASSERT_EQUAL(50U + 50U, gate.high_ticks);
}



static void test_a_whole_step_gate_ties_notes_and_a_rest_ends_it(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw    = TEMPO_RAW_150_BPM;
    in.gate.length  = SEQ_PULSES_PER_STEP;
    in.raw_steps[0] = 0x55U; // Steps 0 to 2 are pitches, step 3 a rest
    in.raw_steps[1] = 0x50U;
    in.raw_steps[2] = 0x50U; // Step 4 a pitch again

    // Three notes are one gate, 300 ticks long
    run_and_trace(&state, &in, 300U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(1, gate.rises);
    TEST_ASSERT_EQUAL(300, gate.high_ticks);
    TEST_ASSERT(gate.b_level);

    // The rest closes it on the tick step 3 begins
    run_and_trace(&state, &in, 1U, &gate, &clock_out);
    TEST_ASSERT(!gate.b_level);
    run_and_trace(&state, &in, 99U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(300, gate.high_ticks);

    run_and_trace(&state, &in, 1U, &gate, &clock_out); // Step 4
    TEST_ASSERT(gate.b_level);
    TEST_ASSERT_EQUAL(2, gate.rises);
}



static void test_gate_length_is_read_when_the_step_begins(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT);
    run_and_trace(&state, &in, 10U, &gate, &clock_out);

    // Changed while step 0 is held: that note keeps the length it began with
    in.gate.length = 0U;
    run_and_trace(&state, &in, 90U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(50, gate.high_ticks);

    // And step 1 takes the new one: it never opens
    run_and_trace(&state, &in, 100U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(1, gate.rises);
    TEST_ASSERT_EQUAL(50, gate.high_ticks);
}



static void test_gate_of_a_late_step_counts_from_when_it_was_due(void)
{
    // 250 BPM is 60 ticks a step, a pulse every 2.5 ticks. Step 1 is due on
    // tick 60 and its gate should close 30 ticks later, on tick 90
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT);
    in.elapsed_ticks = 0U;
    seq_tick(&state, &in, &out); // Tick 0: step 0

    // The loop stalls and comes back on tick 80, 20 ticks (8 pulses) late
    in.elapsed_ticks = 80U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(1, out.step);
    TEST_ASSERT(out.b_gate);
    TEST_ASSERT_EQUAL(4, state.gate.pulses_left); // 12 less the 8 already gone
    TEST_ASSERT(out.b_clock_out);

    in.elapsed_ticks = 9U;
    seq_tick(&state, &in, &out); // Tick 89
    TEST_ASSERT(out.b_gate);
    in.elapsed_ticks = 1U;
    seq_tick(&state, &in, &out); // Tick 90
    TEST_ASSERT(!out.b_gate);
    TEST_ASSERT(!out.b_clock_out);

    // A stall longer than the gate: the note is begun, but its gate has gone
    in.elapsed_ticks = 70U;
    seq_tick(&state, &in, &out); // Tick 160: step 2, due on tick 120
    TEST_ASSERT(out.b_step_started);
    TEST_ASSERT_EQUAL(2, out.step);
    TEST_ASSERT(!out.b_gate);
    TEST_ASSERT(!out.b_clock_out);
}



static void test_clock_out_is_high_for_the_first_half_of_every_step(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs(); // Every step a rest
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    run_and_trace(&state, &in, 100U, &gate, &clock_out); // Step 0
    TEST_ASSERT_EQUAL(1, clock_out.rises);
    TEST_ASSERT_EQUAL(50, clock_out.high_ticks);
    TEST_ASSERT(!clock_out.b_level);

    run_and_trace(&state, &in, 1U, &gate, &clock_out); // It rises as step 1 begins
    TEST_ASSERT(clock_out.b_level);
    run_and_trace(&state, &in, 1599U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(17, clock_out.rises);
    TEST_ASSERT_EQUAL(17U * 50U, clock_out.high_ticks);

    // It does not depend on a note: the gate never opened
    TEST_ASSERT_EQUAL(0, gate.rises);
}



static void test_clock_out_runs_on_when_a_one_shot_has_finished(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw          = TEMPO_RAW_150_BPM;
    in.gate.length        = SEQ_PULSES_PER_STEP; // Tied, so only the end closes it
    in.address.last_step  = 1U;
    in.address.b_one_shot = true;
    (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT);

    // Two steps play as one gate, which ends when the third falls due
    run_and_trace(&state, &in, 199U, &gate, &clock_out);
    TEST_ASSERT(gate.b_level);
    run_and_trace(&state, &in, 800U, &gate, &clock_out);
    TEST_ASSERT_EQUAL(1, gate.rises);
    TEST_ASSERT_EQUAL(200, gate.high_ticks);

    // The clock output kept its beat through all ten step lengths
    TEST_ASSERT_EQUAL(10, clock_out.rises);
    TEST_ASSERT_EQUAL(500, clock_out.high_ticks);
}



static void test_reset_does_not_cut_a_held_note(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    level_trace_t gate      = { 0U, 0U, false };
    level_trace_t clock_out = { 0U, 0U, false };

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    (void)memset(in.raw_steps, 0x11, PANEL_STEP_BYTE_COUNT);
    run_and_trace(&state, &in, 10U, &gate, &clock_out);

    in.b_reset = true;
    run_and_trace(&state, &in, 1U, &gate, &clock_out);
    in.b_reset = false;
    run_and_trace(&state, &in, 88U, &gate, &clock_out);

    TEST_ASSERT_EQUAL(50, gate.high_ticks);
    TEST_ASSERT_EQUAL(1, clock_out.rises);
    TEST_ASSERT_EQUAL(50, clock_out.high_ticks);
}



static void test_cv_is_set_by_each_note_on_the_tick_its_gate_opens(void)
{
    // Steps 1 to 4 hold note values 1 to 4: pitches 0 to 3 in the chromatic scale
    static const uint16_t expected_cv[] = { 0U, 83U, 167U, 250U };
    seq_state_t           state;
    seq_inputs_t          in = make_counting_inputs();
    seq_outputs_t         out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_150_BPM;
    (void)ticks_to_next_step(&state, &in, &out); // Step 0, a rest

    for (size_t i = 0U; i < (sizeof(expected_cv) / sizeof(expected_cv[0])); i++)
    {
        (void)ticks_to_next_step(&state, &in, &out);
        TEST_ASSERT(out.b_gate); // The same tick: the pitch is there as the note starts
        TEST_ASSERT_EQUAL(expected_cv[i], out.cv);
    }

    // The scale and root come first: note value 5 in D major is 9 semitones up
    in.note_map.scale = NOTE_MAP_SCALE_MAJOR;
    in.note_map.root  = 2U;
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(9, out.semitone);
    TEST_ASSERT_EQUAL(750, out.cv);
}



static void test_cv_is_given_on_every_tick_and_holds_after_the_gate_closes(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw    = TEMPO_RAW_150_BPM;
    in.raw_steps[0] = 0xD0U; // Step 0 plays pitch 12, an octave up; the rest are rests

    // 100 ticks a step: through the gate, past its end and through step 1's rest
    for (unsigned int tick = 0U; tick < 199U; tick++)
    {
        (void)memset(&out, POISON_BYTE, sizeof(out));
        seq_tick(&state, &in, &out);
        TEST_ASSERT_EQUAL(CV_PER_OCTAVE, out.cv);
    }
    TEST_ASSERT(!out.b_gate);
}



static void test_cv_is_zero_until_the_first_note(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs(); // Every step a rest
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw = TEMPO_RAW_250_BPM;
    for (unsigned int step = 0U; step < 3U; step++)
    {
        (void)ticks_to_next_step(&state, &in, &out);
        TEST_ASSERT(out.b_rest);
        TEST_ASSERT_EQUAL(0, out.cv);
    }
}



static void test_cv_holds_through_a_rest_an_unreadable_step_and_a_reset(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw    = TEMPO_RAW_250_BPM;
    in.raw_steps[0] = 0x70U; // Step 0 plays pitch 6; step 1 is a rest
    in.raw_steps[1] = 0xD0U; // Step 2 plays pitch 12

    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(500, out.cv);

    (void)ticks_to_next_step(&state, &in, &out); // The rest
    TEST_ASSERT(out.b_rest);
    TEST_ASSERT_EQUAL(500, out.cv);

    in.b_steps_valid = false; // Step 2 cannot be read
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT(!out.b_note_valid);
    TEST_ASSERT_EQUAL(500, out.cv);

    // A reset sends the pattern back, and the pitch stays until a note plays
    in.b_steps_valid = true;
    in.raw_steps[0]  = 0x00U;
    in.b_reset       = true;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(500, out.cv);
    (void)ticks_to_next_step(&state, &in, &out); // Step 0 again, now a rest
    TEST_ASSERT_EQUAL(0, out.step);
    TEST_ASSERT_EQUAL(500, out.cv);
}



static void test_cv_holds_when_a_one_shot_has_finished(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw          = TEMPO_RAW_250_BPM;
    in.raw_steps[0]       = 0xD0U; // Step 0 plays pitch 12
    in.address.last_step  = 0U;
    in.address.b_one_shot = true;

    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(CV_PER_OCTAVE, out.cv);

    // Steps keep falling due and none begins
    for (unsigned int tick = 0U; tick < 200U; tick++)
    {
        seq_tick(&state, &in, &out);
        TEST_ASSERT(!out.b_step_started);
        TEST_ASSERT_EQUAL(CV_PER_OCTAVE, out.cv);
    }
}



static void test_calibration_table_is_read_when_the_step_begins(void)
{
    seq_state_t   state;
    seq_inputs_t  in = make_valid_inputs();
    seq_outputs_t out;

    seq_init(&state);
    in.tempo_raw    = TEMPO_RAW_250_BPM;
    in.raw_steps[0] = 0xDDU; // Steps 0 and 1 both play pitch 12

    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(CV_PER_OCTAVE, out.cv);

    // A table changed mid-note is not heard until the next note
    in.pitch_cal.cv[1] = 1017U;
    seq_tick(&state, &in, &out);
    TEST_ASSERT_EQUAL(CV_PER_OCTAVE, out.cv);
    (void)ticks_to_next_step(&state, &in, &out);
    TEST_ASSERT_EQUAL(1017, out.cv);
}



int main(void)
{
    RUN_TEST(test_gate_opens_with_a_pitch_for_its_length);
    RUN_TEST(test_gate_length_is_counted_in_pulses);
    RUN_TEST(test_gate_stays_closed_for_a_rest_and_an_unreadable_step);
    RUN_TEST(test_a_whole_step_gate_ties_notes_and_a_rest_ends_it);
    RUN_TEST(test_gate_length_is_read_when_the_step_begins);
    RUN_TEST(test_gate_of_a_late_step_counts_from_when_it_was_due);
    RUN_TEST(test_clock_out_is_high_for_the_first_half_of_every_step);
    RUN_TEST(test_clock_out_runs_on_when_a_one_shot_has_finished);
    RUN_TEST(test_reset_does_not_cut_a_held_note);
    RUN_TEST(test_cv_is_set_by_each_note_on_the_tick_its_gate_opens);
    RUN_TEST(test_cv_is_given_on_every_tick_and_holds_after_the_gate_closes);
    RUN_TEST(test_cv_is_zero_until_the_first_note);
    RUN_TEST(test_cv_holds_through_a_rest_an_unreadable_step_and_a_reset);
    RUN_TEST(test_cv_holds_when_a_one_shot_has_finished);
    RUN_TEST(test_calibration_table_is_read_when_the_step_begins);
    return TEST_RESULT();
}
