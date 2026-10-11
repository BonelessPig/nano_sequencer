/**
 * @file   test_midi_output.c
 * @brief  Host tests for the target MIDI output (adapters/target/midi_output.c).
 *         The adapter's source is included directly, with the MCU register
 *         header replaced by a fake, so the exact bytes it would send over
 *         the UART can be compared with what was written. The adapter only
 *         queues bytes; the tests call midi_output_poll() to have them sent.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <string.h>
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "midi_output.c"          // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define QUEUE_BYTES (32U)  // What the adapter's queue is meant to hold
#define DRAIN_POLLS (100U) // Far more polls than the queue has bytes
#define MANY_BYTES  (255U) // The most one write can be given

/**
 * @brief Power-on registers and a freshly set up adapter with an empty
 *        queue. The adapter's own state outlives fake_regs_reset(), so the
 *        init is what empties it.
 */
static void start(void)
{
    fake_regs_reset();
    midi_output_init();
}



/**
 * @brief Polls the adapter until everything it has queued must have been sent.
 */
static void drain(void)
{
    for (unsigned int i = 0U; i < DRAIN_POLLS; i++)
    {
        midi_output_poll();
    }
}



/**
 * @brief Checks that the bytes captured from the UART so far, without any
 *        further polling, are exactly the given ones.
 */
static void expect_sent_so_far(const uint8_t *p_expected, size_t count)
{
    TEST_ASSERT_EQUAL(count, g_fake_uart_length);
    if (count == g_fake_uart_length)
    {
        TEST_ASSERT_EQUAL(0, memcmp(p_expected, g_fake_uart, count));
    }
}



/**
 * @brief Sends everything queued, then checks that the bytes captured from
 *        the UART are exactly the given ones.
 */
static void expect_sent(const uint8_t *p_expected, size_t count)
{
    drain();
    expect_sent_so_far(p_expected, count);
}



/**
 * @brief Fills a buffer with bytes that differ from their neighbours, so a
 *        byte out of place shows.
 */
static void fill_pattern(uint8_t *p_bytes, size_t count, uint8_t first)
{
    for (size_t i = 0U; i < count; i++)
    {
        p_bytes[i] = (uint8_t)(first + (i * 7U));
    }
}



static void test_init_programs_the_usart(void)
{
    start();

    // 31250 baud in double-speed mode: 16 MHz / (8 * 31250) - 1 = 63 exactly
    TEST_ASSERT_EQUAL(0, g_fake_ubrr0h);
    TEST_ASSERT_EQUAL(63, g_fake_ubrr0l);
    TEST_ASSERT_EQUAL(0x02, g_fake_ucsr0a & 0xDFU); // Double speed on; ignore the buffer-empty flag
    TEST_ASSERT_EQUAL(0x18, g_fake_ucsr0b); // Receiver and transmitter enabled, no interrupts
    TEST_ASSERT_EQUAL(0x06, g_fake_ucsr0c); // 8 data bits, no parity, 1 stop bit
    TEST_ASSERT_EQUAL(0, g_fake_uart_length); // And nothing was sent
}



static void test_an_empty_queue_has_room_for_32_bytes(void)
{
    start();

    TEST_ASSERT_EQUAL(QUEUE_BYTES, midi_output_room());
}



static void test_room_is_the_free_space_at_every_fill_level(void)
{
    uint8_t bytes[QUEUE_BYTES];

    fill_pattern(bytes, sizeof(bytes), 1U);
    start();

    // Filling, a byte at a time
    for (unsigned int queued = 0U; queued < QUEUE_BYTES; queued++)
    {
        TEST_ASSERT_EQUAL(QUEUE_BYTES - queued, midi_output_room());
        midi_output_write(&bytes[queued], 1U);
    }
    TEST_ASSERT_EQUAL(0, midi_output_room());

    // And emptying: each byte sent is a byte of room
    for (unsigned int sent = 1U; sent <= QUEUE_BYTES; sent++)
    {
        midi_output_poll();
        TEST_ASSERT_EQUAL(sent, midi_output_room());
    }
    expect_sent_so_far(bytes, sizeof(bytes));
}



static void test_nothing_is_sent_until_polled(void)
{
    static const uint8_t message[] = { 0x90U, 0x18U, 0x64U };

    start();
    midi_output_write(message, (uint8_t)sizeof(message));

    expect_sent_so_far(message, 0U);
    TEST_ASSERT_EQUAL(QUEUE_BYTES - 3U, midi_output_room());
}



static void test_poll_sends_one_byte_at_a_time_in_the_order_written(void)
{
    static const uint8_t bytes[] = { 0xFAU, 0xF8U, 0x90U, 0x18U, 0x64U, 0x80U, 0x18U, 0x00U };

    start();
    midi_output_write(&bytes[0], 2U); // Start and a clock
    midi_output_write(&bytes[2], 3U); // Note On
    midi_output_write(&bytes[5], 3U); // Note Off

    for (size_t sent = 1U; sent <= sizeof(bytes); sent++)
    {
        midi_output_poll();
        expect_sent_so_far(bytes, sent);
    }

    // The queue is empty now: further polls send nothing
    midi_output_poll();
    expect_sent_so_far(bytes, sizeof(bytes));
    TEST_ASSERT_EQUAL(QUEUE_BYTES, midi_output_room());
}



static void test_poll_sends_nothing_while_the_transmit_buffer_is_full(void)
{
    static const uint8_t message[] = { 0x90U, 0x18U, 0x64U };

    start();
    midi_output_write(message, (uint8_t)sizeof(message));
    g_fake_uart_busy_polls = 3U;

    // It looks at the flag once per poll and comes straight back
    midi_output_poll();
    midi_output_poll();
    midi_output_poll();
    TEST_ASSERT_EQUAL(0, g_fake_uart_busy_polls);
    expect_sent_so_far(message, 0U);
    TEST_ASSERT_EQUAL(QUEUE_BYTES - 3U, midi_output_room()); // Still queued

    midi_output_poll();
    expect_sent_so_far(message, 1U);
    TEST_ASSERT_EQUAL(QUEUE_BYTES - 2U, midi_output_room());
}



static void test_poll_with_nothing_queued_leaves_the_usart_alone(void)
{
    start();
    g_fake_uart_busy_polls = 5U;
    midi_output_poll();

    TEST_ASSERT_EQUAL(5, g_fake_uart_busy_polls); // The status register was not read
    TEST_ASSERT_EQUAL(0, g_fake_uart_length);
    g_fake_uart_busy_polls = 0U;
}



static void test_a_write_of_exactly_the_room_is_all_queued(void)
{
    uint8_t bytes[QUEUE_BYTES];

    fill_pattern(bytes, sizeof(bytes), 3U);
    start();
    midi_output_write(bytes, (uint8_t)sizeof(bytes));

    TEST_ASSERT_EQUAL(0, midi_output_room());
    expect_sent(bytes, sizeof(bytes));
}



static void test_bytes_beyond_the_room_are_dropped(void)
{
    uint8_t bytes[MANY_BYTES];

    fill_pattern(bytes, sizeof(bytes), 5U);
    start();

    // Into an empty queue: the first 32 are kept
    midi_output_write(bytes, (uint8_t)sizeof(bytes));
    TEST_ASSERT_EQUAL(0, midi_output_room());

    // Into a full one: nothing is, and what is queued is not disturbed
    midi_output_write(&bytes[100], 8U);
    TEST_ASSERT_EQUAL(0, midi_output_room());
    expect_sent(bytes, QUEUE_BYTES);
}



static void test_a_write_that_partly_fits_keeps_its_first_bytes(void)
{
    uint8_t first[QUEUE_BYTES - 5U];
    uint8_t second[8];
    uint8_t expected[QUEUE_BYTES];

    fill_pattern(first, sizeof(first), 11U);
    fill_pattern(second, sizeof(second), 200U);
    (void)memcpy(expected, first, sizeof(first));
    (void)memcpy(&expected[sizeof(first)], second, 5U);

    start();
    midi_output_write(first, (uint8_t)sizeof(first));
    TEST_ASSERT_EQUAL(5, midi_output_room());
    midi_output_write(second, (uint8_t)sizeof(second)); // Eight into five

    TEST_ASSERT_EQUAL(0, midi_output_room());
    expect_sent(expected, sizeof(expected));
}



static void test_a_null_pointer_queues_nothing(void)
{
    static const uint8_t message[] = { 0xF8U };

    start();
    midi_output_write(message, 1U);
    midi_output_write(NULL, 3U);
    midi_output_write(NULL, 0U);

    TEST_ASSERT_EQUAL(QUEUE_BYTES - 1U, midi_output_room());
    expect_sent(message, sizeof(message));
}



static void test_a_count_of_zero_queues_nothing(void)
{
    static const uint8_t message[] = { 0xFCU };

    start();
    midi_output_write(message, 0U);

    TEST_ASSERT_EQUAL(QUEUE_BYTES, midi_output_room());
    expect_sent(message, 0U);
}



static void test_the_queue_keeps_working_after_it_wraps(void)
{
    uint8_t bytes[7];

    start();
    // Far more bytes than the queue holds, seven at a time: the place the
    // next byte goes passes the end of the ring many times, at every offset
    for (unsigned int i = 0U; i < 100U; i++)
    {
        fill_pattern(bytes, sizeof(bytes), (uint8_t)i);
        fake_uart_clear();
        midi_output_write(bytes, (uint8_t)sizeof(bytes));
        TEST_ASSERT_EQUAL(QUEUE_BYTES - sizeof(bytes), midi_output_room());
        expect_sent(bytes, sizeof(bytes));
        TEST_ASSERT_EQUAL(QUEUE_BYTES, midi_output_room());
    }
}



static void test_a_full_queue_that_straddles_the_end_is_sent_in_order(void)
{
    uint8_t lead[20];
    uint8_t bytes[QUEUE_BYTES];
    uint8_t more[20];
    uint8_t expected[QUEUE_BYTES + sizeof(more)];

    fill_pattern(lead, sizeof(lead), 9U);
    fill_pattern(bytes, sizeof(bytes), 40U);
    fill_pattern(more, sizeof(more), 77U);
    (void)memcpy(expected, bytes, sizeof(bytes));
    (void)memcpy(&expected[sizeof(bytes)], more, sizeof(more));

    // Move the start of the queue to its twentieth place
    start();
    midi_output_write(lead, (uint8_t)sizeof(lead));
    drain();
    fake_uart_clear();

    // Fill it from there: 12 bytes to the end and 20 from the beginning
    midi_output_write(bytes, (uint8_t)sizeof(bytes));
    TEST_ASSERT_EQUAL(0, midi_output_room());

    // Send 20, which takes the start past the end, and top it up again
    for (unsigned int i = 0U; i < sizeof(more); i++)
    {
        midi_output_poll();
    }
    TEST_ASSERT_EQUAL(sizeof(more), midi_output_room());
    midi_output_write(more, (uint8_t)sizeof(more));
    TEST_ASSERT_EQUAL(0, midi_output_room());

    expect_sent(expected, sizeof(expected));
    TEST_ASSERT_EQUAL(QUEUE_BYTES, midi_output_room());
}



static void test_init_empties_the_queue(void)
{
    static const uint8_t message[] = { 0x90U, 0x18U, 0x64U };

    start();
    midi_output_write(message, (uint8_t)sizeof(message));

    midi_output_init();
    TEST_ASSERT_EQUAL(QUEUE_BYTES, midi_output_room());
    expect_sent(message, 0U);
}



int main(void)
{
    RUN_TEST(test_init_programs_the_usart);
    RUN_TEST(test_an_empty_queue_has_room_for_32_bytes);
    RUN_TEST(test_room_is_the_free_space_at_every_fill_level);
    RUN_TEST(test_nothing_is_sent_until_polled);
    RUN_TEST(test_poll_sends_one_byte_at_a_time_in_the_order_written);
    RUN_TEST(test_poll_sends_nothing_while_the_transmit_buffer_is_full);
    RUN_TEST(test_poll_with_nothing_queued_leaves_the_usart_alone);
    RUN_TEST(test_a_write_of_exactly_the_room_is_all_queued);
    RUN_TEST(test_bytes_beyond_the_room_are_dropped);
    RUN_TEST(test_a_write_that_partly_fits_keeps_its_first_bytes);
    RUN_TEST(test_a_null_pointer_queues_nothing);
    RUN_TEST(test_a_count_of_zero_queues_nothing);
    RUN_TEST(test_the_queue_keeps_working_after_it_wraps);
    RUN_TEST(test_a_full_queue_that_straddles_the_end_is_sent_in_order);
    RUN_TEST(test_init_empties_the_queue);
    return TEST_RESULT();
}
