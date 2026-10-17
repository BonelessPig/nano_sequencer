/**
 * @file   test_midi_input.c
 * @brief  Host tests for the target MIDI input (adapters/target/midi_input.c).
 *         The adapter's source is included directly, with the MCU register
 *         headers replaced by a fake. A byte is received by giving the fake
 *         data register the byte and calling the interrupt handler, which
 *         is an ordinary function here.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <string.h>
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "midi_input.c"           // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define RING_BYTES  (15U)   // What the adapter's ring is meant to hold
#define POISON_BYTE (0xA5)
#define MANY        (64U)   // More than the ring holds

/**
 * @brief Power-on registers, with the USART as midi_output_init() leaves its
 *        control register, and a freshly set up adapter with an empty ring.
 *        The adapter's own state outlives fake_regs_reset(), so the init is
 *        what empties it.
 */
static void start(void)
{
    fake_regs_reset();
    g_fake_ucsr0b = (uint8_t)((1U << RXEN0) | (1U << TXEN0));
    midi_input_init();
}

/**
 * @brief Has one byte arrive: the data register holds it and the handler
 *        runs, as the receive interrupt would have it.
 */
static void receive(uint8_t byte)
{
    fake_uart_receive(byte);
    fake_usart_rx_isr();
}

/**
 * @brief Has one byte arrive with a frame error against it.
 */
static void receive_damaged(uint8_t byte)
{
    g_fake_ucsr0a |= (uint8_t)(1U << FE0);
    receive(byte);
    g_fake_ucsr0a &= (uint8_t)~(1U << FE0);
}



static void test_init_turns_on_the_receive_interrupt_and_nothing_else(void)
{
    start();

    TEST_ASSERT_EQUAL((1U << RXCIE0) | (1U << RXEN0) | (1U << TXEN0), g_fake_ucsr0b);
    TEST_ASSERT_EQUAL(0, g_fake_ubrr0h);
    TEST_ASSERT_EQUAL(0, g_fake_ubrr0l);
    TEST_ASSERT_EQUAL(0, g_fake_ucsr0c);
    TEST_ASSERT_EQUAL(0, g_fake_uart_length); // Nothing sent, nothing read
}



static void test_nothing_received_gives_nothing(void)
{
    uint8_t bytes[4];

    start();
    (void)memset(bytes, POISON_BYTE, sizeof(bytes));
    TEST_ASSERT_EQUAL(0, midi_input_read(bytes, (uint8_t)sizeof(bytes)));
    TEST_ASSERT_EQUAL(POISON_BYTE, bytes[0]);
}



static void test_bytes_are_handed_over_oldest_first_and_once(void)
{
    uint8_t bytes[8];

    start();
    receive(0xFAU);
    receive(0xF8U);
    receive(0x90U);

    (void)memset(bytes, POISON_BYTE, sizeof(bytes));
    TEST_ASSERT_EQUAL(3, midi_input_read(bytes, (uint8_t)sizeof(bytes)));
    TEST_ASSERT_EQUAL(0xFA, bytes[0]);
    TEST_ASSERT_EQUAL(0xF8, bytes[1]);
    TEST_ASSERT_EQUAL(0x90, bytes[2]);
    TEST_ASSERT_EQUAL(POISON_BYTE, bytes[3]);

    TEST_ASSERT_EQUAL(0, midi_input_read(bytes, (uint8_t)sizeof(bytes)));
}



static void test_the_handler_reads_the_data_register_once_a_byte(void)
{
    start();
    receive(0xF8U);
    receive(0xF8U);

    // Each read of the fake data register takes a slot of its capture
    TEST_ASSERT_EQUAL(2, g_fake_uart_length);
}



static void test_bytes_beyond_the_most_asked_for_wait(void)
{
    uint8_t bytes[8];

    start();
    for (uint8_t i = 0U; i < 5U; i++)
    {
        receive((uint8_t)(0x10U + i));
    }

    (void)memset(bytes, POISON_BYTE, sizeof(bytes));
    TEST_ASSERT_EQUAL(2, midi_input_read(bytes, 2U));
    TEST_ASSERT_EQUAL(0x10, bytes[0]);
    TEST_ASSERT_EQUAL(0x11, bytes[1]);
    TEST_ASSERT_EQUAL(POISON_BYTE, bytes[2]);

    TEST_ASSERT_EQUAL(0, midi_input_read(bytes, 0U));

    TEST_ASSERT_EQUAL(3, midi_input_read(bytes, (uint8_t)sizeof(bytes)));
    TEST_ASSERT_EQUAL(0x12, bytes[0]);
    TEST_ASSERT_EQUAL(0x13, bytes[1]);
    TEST_ASSERT_EQUAL(0x14, bytes[2]);
}



static void test_a_byte_with_a_frame_error_is_dropped(void)
{
    uint8_t bytes[8];

    start();
    receive(0xF8U);
    receive_damaged(0xFAU);
    receive(0xFCU);

    // The damaged one was still taken out of the data register
    TEST_ASSERT_EQUAL(3, g_fake_uart_length);

    TEST_ASSERT_EQUAL(2, midi_input_read(bytes, (uint8_t)sizeof(bytes)));
    TEST_ASSERT_EQUAL(0xF8, bytes[0]);
    TEST_ASSERT_EQUAL(0xFC, bytes[1]);
}



static void test_a_byte_with_no_room_is_dropped_and_the_rest_kept(void)
{
    uint8_t bytes[MANY];

    start();
    for (uint8_t i = 0U; i < MANY; i++)
    {
        receive(i);
    }

    TEST_ASSERT_EQUAL(RING_BYTES, midi_input_read(bytes, (uint8_t)sizeof(bytes)));
    for (uint8_t i = 0U; i < RING_BYTES; i++)
    {
        TEST_ASSERT_EQUAL(i, bytes[i]); // The first ones to come, in order
    }

    // And there is room again
    receive(0x77U);
    TEST_ASSERT_EQUAL(1, midi_input_read(bytes, (uint8_t)sizeof(bytes)));
    TEST_ASSERT_EQUAL(0x77, bytes[0]);
}



static void test_the_ring_goes_round(void)
{
    uint8_t bytes[8];
    uint8_t next_in  = 0U;
    uint8_t next_out = 0U;

    start();
    // Seven in and seven out, twenty times: far more than the ring's length
    for (unsigned int round = 0U; round < 20U; round++)
    {
        for (unsigned int i = 0U; i < 7U; i++)
        {
            receive(next_in);
            next_in++;
        }
        TEST_ASSERT_EQUAL(7, midi_input_read(bytes, (uint8_t)sizeof(bytes)));
        for (unsigned int i = 0U; i < 7U; i++)
        {
            TEST_ASSERT_EQUAL(next_out, bytes[i]);
            next_out++;
        }
    }
}



static void test_init_forgets_what_was_waiting(void)
{
    uint8_t bytes[8];

    start();
    receive(0xF8U);
    receive(0xF8U);

    midi_input_init();
    TEST_ASSERT_EQUAL(0, midi_input_read(bytes, (uint8_t)sizeof(bytes)));
}



static void test_a_null_pointer_takes_nothing(void)
{
    uint8_t bytes[8];

    start();
    receive(0xFAU);

    TEST_ASSERT_EQUAL(0, midi_input_read(NULL, 8U));
    // It is still there
    TEST_ASSERT_EQUAL(1, midi_input_read(bytes, (uint8_t)sizeof(bytes)));
    TEST_ASSERT_EQUAL(0xFA, bytes[0]);
}



int main(void)
{
    RUN_TEST(test_init_turns_on_the_receive_interrupt_and_nothing_else);
    RUN_TEST(test_nothing_received_gives_nothing);
    RUN_TEST(test_bytes_are_handed_over_oldest_first_and_once);
    RUN_TEST(test_the_handler_reads_the_data_register_once_a_byte);
    RUN_TEST(test_bytes_beyond_the_most_asked_for_wait);
    RUN_TEST(test_a_byte_with_a_frame_error_is_dropped);
    RUN_TEST(test_a_byte_with_no_room_is_dropped_and_the_rest_kept);
    RUN_TEST(test_the_ring_goes_round);
    RUN_TEST(test_init_forgets_what_was_waiting);
    RUN_TEST(test_a_null_pointer_takes_nothing);
    return TEST_RESULT();
}
