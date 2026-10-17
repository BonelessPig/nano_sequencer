/**
 * @file   test_app_midi_in.c
 * @brief  Host tests for the application loop (app/app.c) against the fake
 *         ports in adapters/host: MIDI in.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "app.h"
#include "host_ports.h"
#include "midi_input_port.h"
#include "seq.h"
#include "test_harness.h"
#include "app_fixture.h"

#define POISON_BYTE     (0xA5)
#define CLOCKS_PER_STEP (6U)
#define BYTES_PER_TICK  (8U) // The most the app takes from the input in a tick

/**
 * @brief Has one byte arrive at the MIDI input, for the next tick to read.
 */
static void receive(uint8_t byte)
{
    host_receive_midi(&byte, 1U);
}

/**
 * @brief Has a number of MIDI clocks arrive, six ticks apart, the last of
 *        them read and applied by the time this returns.
 */
static void receive_clocks(unsigned int count)
{
    for (unsigned int i = 0U; i < count; i++)
    {
        receive(MIDI_CLOCK);
        run_ticks(6U);
    }
}



static void test_the_midi_input_fake_hands_over_what_was_received_in_order(void)
{
    static const uint8_t first[]  = { 0xFAU, 0xF8U, 0x90U };
    static const uint8_t second[] = { 0x3CU, 0x64U };
    uint8_t              bytes[HOST_MIDI_IN_MAX + 8U];
    uint8_t              many[HOST_MIDI_IN_MAX + 8U];

    // The port is called here directly: this is the fake's own behaviour
    host_reset();
    (void)memset(bytes, POISON_BYTE, sizeof(bytes));
    TEST_ASSERT_EQUAL(0, midi_input_read(bytes, 8U));
    TEST_ASSERT_EQUAL(POISON_BYTE, bytes[0]);
    TEST_ASSERT_EQUAL(1, host_midi_input_read_count());

    host_receive_midi(first, (uint8_t)sizeof(first));
    host_receive_midi(second, (uint8_t)sizeof(second));
    host_receive_midi(NULL, 3U);
    host_receive_midi(first, 0U);

    // No more than asked for, and nothing through a null pointer
    TEST_ASSERT_EQUAL(0, midi_input_read(NULL, 8U));
    TEST_ASSERT_EQUAL(2, midi_input_read(bytes, 2U));
    TEST_ASSERT_EQUAL(0xFA, bytes[0]);
    TEST_ASSERT_EQUAL(0xF8, bytes[1]);
    TEST_ASSERT_EQUAL(POISON_BYTE, bytes[2]);

    // The rest waits, and is handed over once
    TEST_ASSERT_EQUAL(3, midi_input_read(bytes, 8U));
    TEST_ASSERT_EQUAL(0x90, bytes[0]);
    TEST_ASSERT_EQUAL(0x3C, bytes[1]);
    TEST_ASSERT_EQUAL(0x64, bytes[2]);
    TEST_ASSERT_EQUAL(0, midi_input_read(bytes, 8U));
    TEST_ASSERT_EQUAL(5, host_midi_input_read_count());

    // What does not fit is dropped
    for (unsigned int i = 0U; i < sizeof(many); i++)
    {
        many[i] = (uint8_t)i;
    }
    host_receive_midi(many, (uint8_t)sizeof(many));
    TEST_ASSERT_EQUAL(HOST_MIDI_IN_MAX, midi_input_read(bytes, (uint8_t)sizeof(bytes)));
    TEST_ASSERT_EQUAL(0, bytes[0]);
    TEST_ASSERT_EQUAL(HOST_MIDI_IN_MAX - 1U, bytes[HOST_MIDI_IN_MAX - 1U]);

    // And host_reset() empties it
    host_receive_midi(first, (uint8_t)sizeof(first));
    host_reset();
    TEST_ASSERT_EQUAL(0, midi_input_read(bytes, 8U));
}



static void test_the_midi_input_is_read_once_on_every_tick(void)
{
    start_app();
    TEST_ASSERT_EQUAL(0, host_midi_input_read_count());
    run_ticks(25U);
    TEST_ASSERT_EQUAL(25, host_midi_input_read_count());

    // A pass with no tick reads nothing
    host_set_elapsed_ticks(0U);
    app_run_once();
    TEST_ASSERT_EQUAL(25, host_midi_input_read_count());

    // In calibration mode too, so that nothing piles up
    start_calibration(0U, CAL_TRIM_CENTRE);
    run_ticks(10U);
    TEST_ASSERT_EQUAL(10, host_midi_input_read_count());
}



static void test_a_midi_clock_takes_the_pattern_over_at_six_clocks_a_step(void)
{
    start_app();
    host_set_tempo(SYNC_RAW_ONE_TO_ONE, STATUS_OK);

    // The internal clock to begin with: step 0, and its clock pulse over
    run_ticks(60U);
    TEST_ASSERT_EQUAL(1, host_event_count());

    // The first clock is an edge: read on the next tick and applied on the
    // one after, the change of clock logged and the next step begun there
    receive(MIDI_CLOCK);
    run_ticks(1U);
    TEST_ASSERT_EQUAL(1, host_event_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(3, host_event_count());
    expect_event(1U, HOST_EVENT_TRANSPORT, (uint16_t)TRANSPORT_EXTERNAL, 0U);
    expect_event(2U, HOST_EVENT_STEP_NOTE, 1U, 13U);
    run_ticks(4U);

    for (uint16_t step = 2U; step < 5U; step++)
    {
        // Five more only count
        receive_clocks(CLOCKS_PER_STEP - 1U);
        TEST_ASSERT_EQUAL(step + 1U, host_event_count());

        // And the sixth is the step
        receive_clocks(1U);
        TEST_ASSERT_EQUAL(step + 2U, host_event_count());
        expect_event((uint16_t)(step + 1U), HOST_EVENT_STEP_NOTE, step, (uint16_t)(14U - step));
        TEST_ASSERT_EQUAL(step + 1U, host_gate_rise_count());
    }

    // With no clocks nothing more begins
    run_ticks(500U);
    TEST_ASSERT_EQUAL(6, host_event_count());
}



static void test_a_midi_stop_and_start_work_the_pattern_and_are_logged(void)
{
    start_app();
    host_set_tempo(SYNC_RAW_ONE_TO_ONE, STATUS_OK);
    run_ticks(10U);
    TEST_ASSERT_EQUAL(1, host_event_count());
    TEST_ASSERT(host_gate_level());

    receive(MIDI_STOP);
    run_ticks(2U);
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(1U, HOST_EVENT_TRANSPORT, (uint16_t)TRANSPORT_STOPPED, 0U);
    TEST_ASSERT(!host_gate_level());
    run_ticks(300U);
    TEST_ASSERT_EQUAL(2, host_event_count());

    // A Start is on the MIDI clock at once, and waits for it
    receive(MIDI_START);
    run_ticks(2U);
    TEST_ASSERT_EQUAL(3, host_event_count());
    expect_event(2U, HOST_EVENT_TRANSPORT, (uint16_t)TRANSPORT_EXTERNAL, 0U);
    run_ticks(300U);
    TEST_ASSERT_EQUAL(3, host_event_count());
    TEST_ASSERT_EQUAL(1, host_gate_rise_count());

    // The clock after the Start begins the pattern from its first step
    receive_clocks(1U);
    TEST_ASSERT_EQUAL(4, host_event_count());
    expect_event(3U, HOST_EVENT_STEP_NOTE, 0U, 14U);
    receive_clocks(CLOCKS_PER_STEP - 1U);
    TEST_ASSERT_EQUAL(4, host_event_count());
    receive_clocks(1U);
    TEST_ASSERT_EQUAL(5, host_event_count());
    expect_event(4U, HOST_EVENT_STEP_NOTE, 1U, 13U);
}



static void test_the_bytes_of_a_note_change_nothing(void)
{
    static const uint8_t note_on[]  = { 0x90U, 0x3CU, 0x64U };
    static const uint8_t note_off[] = { 0x80U, 0x3CU, 0x00U };

    start_app();
    run_ticks(10U);

    host_receive_midi(note_on, (uint8_t)sizeof(note_on));
    run_ticks(30U);
    host_receive_midi(note_off, (uint8_t)sizeof(note_off));
    run_ticks(TICKS_PER_STEP);

    // Steps 0 and 1 at the tempo, and no change of transport
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(0U, HOST_EVENT_STEP_NOTE, 0U, 14U);
    expect_event(1U, HOST_EVENT_STEP_NOTE, 1U, 13U);
}



static void test_bytes_beyond_a_ticks_worth_are_read_on_the_next(void)
{
    static const uint8_t filler[BYTES_PER_TICK] = { 0U };

    // A Stop that is the last byte of a tick's worth is read on that tick
    // and applied on the next
    start_app();
    run_ticks(10U);
    host_receive_midi(filler, BYTES_PER_TICK - 1U);
    receive(MIDI_STOP);
    run_ticks(2U);
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(1U, HOST_EVENT_TRANSPORT, (uint16_t)TRANSPORT_STOPPED, 0U);

    // One byte later and it waits a tick more
    start_app();
    run_ticks(10U);
    host_receive_midi(filler, BYTES_PER_TICK);
    receive(MIDI_STOP);
    run_ticks(2U);
    TEST_ASSERT_EQUAL(1, host_event_count());
    run_ticks(1U);
    TEST_ASSERT_EQUAL(2, host_event_count());
    expect_event(1U, HOST_EVENT_TRANSPORT, (uint16_t)TRANSPORT_STOPPED, 0U);
}



int main(void)
{
    RUN_TEST(test_the_midi_input_fake_hands_over_what_was_received_in_order);
    RUN_TEST(test_the_midi_input_is_read_once_on_every_tick);
    RUN_TEST(test_a_midi_clock_takes_the_pattern_over_at_six_clocks_a_step);
    RUN_TEST(test_a_midi_stop_and_start_work_the_pattern_and_are_logged);
    RUN_TEST(test_the_bytes_of_a_note_change_nothing);
    RUN_TEST(test_bytes_beyond_a_ticks_worth_are_read_on_the_next);
    return TEST_RESULT();
}
