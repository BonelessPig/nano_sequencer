/**
 * @file   test_serial_logger.c
 * @brief  Host tests for the target serial logger (adapters/target/serial_logger.c).
 *         The adapter's source is included directly, with the MCU register
 *         header replaced by a fake, so the exact bytes it would send over the
 *         UART can be compared with the expected text. The logger only queues
 *         a message; the tests call log_poll() to have it sent.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdio.h>
#include <string.h>
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "serial_logger.c"        // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define EXPECTED_CAPACITY (256U)
#define DRAIN_POLLS       (1000U) // Far more polls than the queue has bytes

/**
 * @brief Polls the logger until everything it has queued must have been sent.
 */
static void drain(void)
{
    for (unsigned int i = 0U; i < DRAIN_POLLS; i++)
    {
        log_poll();
    }
}



/**
 * @brief Checks that the bytes captured from the UART so far, without any
 *        further polling, are exactly p_expected.
 */
static void expect_uart_so_far(const char *p_expected)
{
    const size_t expected_length = strlen(p_expected);

    TEST_ASSERT_EQUAL(expected_length, g_fake_uart_length);
    if (expected_length == g_fake_uart_length)
    {
        TEST_ASSERT_EQUAL(0, memcmp(p_expected, g_fake_uart, expected_length));
    }
}



/**
 * @brief Sends everything queued, then checks that the bytes captured from
 *        the UART are exactly p_expected.
 */
static void expect_uart(const char *p_expected)
{
    drain();
    expect_uart_so_far(p_expected);
}



static void test_init_programs_the_usart(void)
{
    TEST_ASSERT_EQUAL(STATUS_OK, logger_init(LOGLVL_DEBUG));

    // 115200 baud in double-speed mode: 16 MHz / (8 * 115200) - 1 = 16.4, nearest is 16
    TEST_ASSERT_EQUAL(0, g_fake_ubrr0h);
    TEST_ASSERT_EQUAL(16, g_fake_ubrr0l);
    TEST_ASSERT_EQUAL(0x02, g_fake_ucsr0a & 0xDFU); // Double speed on; ignore the buffer-empty flag
    TEST_ASSERT_EQUAL(0x18, g_fake_ucsr0b); // Receiver and transmitter enabled
    TEST_ASSERT_EQUAL(0x06, g_fake_ucsr0c); // 8 data bits, no parity, 1 stop bit
}



static void test_decimal_has_no_padding_or_sign(void)
{
    static const uint16_t values[]   = { 0U, 9U, 10U, 99U, 100U, 255U, 1000U, 9999U, 10000U, 65535U };
    char                  expected[EXPECTED_CAPACITY];

    for (size_t i = 0U; i < (sizeof(values) / sizeof(values[0])); i++)
    {
        fake_uart_clear();
        write_decimal(values[i]);
        (void)snprintf(expected, sizeof(expected), "%u", (unsigned int)values[i]);
        expect_uart(expected);
    }
}



static void test_step_note_text_matches_the_printf_format(void)
{
    char expected[EXPECTED_CAPACITY];

    (void)logger_init(LOGLVL_DEBUG);
    // Every step and note the sequencer can produce, plus the 8-bit extremes
    for (unsigned int step = 0U; step <= 255U; step += (step < 16U) ? 1U : 239U)
    {
        for (unsigned int note = 0U; note <= 255U; note += (note < 16U) ? 1U : 239U)
        {
            fake_uart_clear();
            log_step_note((uint8_t)step, (uint8_t)note);
            (void)snprintf(expected, sizeof(expected), "Step %d Note = %d\r\n", (int)step, (int)note);
            expect_uart(expected);
        }
    }
}



static void test_step_rest_text_matches_the_printf_format(void)
{
    char expected[EXPECTED_CAPACITY];

    (void)logger_init(LOGLVL_DEBUG);
    for (unsigned int step = 0U; step <= 255U; step += (step < 16U) ? 1U : 239U)
    {
        fake_uart_clear();
        log_step_rest((uint8_t)step);
        (void)snprintf(expected, sizeof(expected), "Step %d Rest\r\n", (int)step);
        expect_uart(expected);
    }
}



static void test_error_text_matches_the_printf_format(void)
{
    static const port_status_t statuses[] =
    {
        STATUS_OK, ERR_GENERAL, ERR_INVALID_PARAM, ERR_BUSY, ERR_TIMEOUT, ERR_NOT_SUPPORTED, ERR_UNKNOWN
    };
    char expected[EXPECTED_CAPACITY];

    (void)logger_init(LOGLVL_DEBUG);
    for (size_t i = 0U; i < (sizeof(statuses) / sizeof(statuses[0])); i++)
    {
        fake_uart_clear();
        log_error(LOG_ERROR_INIT, statuses[i]);
        (void)snprintf(expected, sizeof(expected), "Initialization failed, status code = %d\r\n", (int)statuses[i]);
        expect_uart(expected);

        fake_uart_clear();
        log_error(LOG_ERROR_STEP_READ, statuses[i]);
        (void)snprintf(expected, sizeof(expected), "Step note read failed, status code = %d\r\n", (int)statuses[i]);
        expect_uart(expected);

        fake_uart_clear();
        log_error(LOG_ERROR_TEMPO_READ, statuses[i]);
        (void)snprintf(expected, sizeof(expected), "ADC read failed for delay channel, status code = %d\r\n", (int)statuses[i]);
        expect_uart(expected);
    }
}



static void test_unknown_error_id_sends_nothing(void)
{
    (void)logger_init(LOGLVL_DEBUG);
    fake_uart_clear();
    log_error((log_error_id_t)99, ERR_GENERAL);
    expect_uart("");
}



static void test_level_filters_messages(void)
{
    // OFF: nothing at all
    (void)logger_init(LOGLVL_OFF);
    fake_uart_clear();
    log_step_note(1U, 2U);
    log_error(LOG_ERROR_INIT, ERR_GENERAL);
    expect_uart("");

    // ERROR: errors pass, debug-level step notes and rests do not
    (void)logger_init(LOGLVL_ERROR);
    fake_uart_clear();
    log_step_note(1U, 2U);
    log_step_rest(3U);
    expect_uart("");
    log_error(LOG_ERROR_INIT, ERR_GENERAL);
    expect_uart("Initialization failed, status code = 1\r\n");

    // FATAL is less verbose than ERROR, so errors are dropped too
    (void)logger_init(LOGLVL_FATAL);
    fake_uart_clear();
    log_error(LOG_ERROR_INIT, ERR_GENERAL);
    expect_uart("");

    // DEBUG and TRACE: both kinds pass
    (void)logger_init(LOGLVL_TRACE);
    fake_uart_clear();
    log_step_note(1U, 2U);
    log_step_rest(3U);
    log_error(LOG_ERROR_STEP_READ, ERR_INVALID_PARAM);
    expect_uart("Step 1 Note = 2\r\nStep 3 Rest\r\n"
                "Step note read failed, status code = 2\r\n");
}



static void test_off_is_never_an_enabled_message_level(void)
{
    // No message is sent at level OFF today; this pins the rule for when one might be
    (void)logger_init(LOGLVL_TRACE);
    TEST_ASSERT(!is_level_enabled(LOGLVL_OFF));

    (void)logger_init(LOGLVL_OFF);
    TEST_ASSERT(!is_level_enabled(LOGLVL_OFF));
}



static void test_nothing_is_sent_until_polled(void)
{
    (void)logger_init(LOGLVL_DEBUG);
    fake_uart_clear();
    log_step_note(1U, 2U);
    log_error(LOG_ERROR_INIT, ERR_GENERAL);

    expect_uart_so_far("");
}



static void test_poll_sends_one_byte_at_a_time(void)
{
    static const char line[] = "Step 15 Note = 9\r\n";
    char              expected[EXPECTED_CAPACITY];

    (void)logger_init(LOGLVL_DEBUG);
    fake_uart_clear();
    log_step_note(15U, 9U);

    for (size_t sent = 1U; sent < sizeof(line); sent++)
    {
        log_poll();
        (void)memcpy(expected, line, sent);
        expected[sent] = '\0';
        expect_uart_so_far(expected);
    }

    // The queue is empty now: further polls send nothing
    log_poll();
    expect_uart_so_far(line);
}



static void test_poll_sends_nothing_while_the_transmit_buffer_is_full(void)
{
    (void)logger_init(LOGLVL_DEBUG);
    fake_uart_clear();
    log_step_note(15U, 9U);
    g_fake_uart_busy_polls = 3U;

    // It looks at the flag once per poll and comes straight back
    log_poll();
    log_poll();
    log_poll();
    TEST_ASSERT_EQUAL(0, g_fake_uart_busy_polls);
    expect_uart_so_far("");

    log_poll();
    expect_uart_so_far("S");
}



static void test_poll_with_nothing_queued_leaves_the_usart_alone(void)
{
    (void)logger_init(LOGLVL_DEBUG);
    fake_uart_clear();
    g_fake_uart_busy_polls = 5U;
    log_poll();

    TEST_ASSERT_EQUAL(5, g_fake_uart_busy_polls); // The status register was not read
    expect_uart_so_far("");
    g_fake_uart_busy_polls = 0U;
}



static void test_messages_are_sent_in_the_order_they_were_logged(void)
{
    (void)logger_init(LOGLVL_DEBUG);
    fake_uart_clear();
    log_step_note(3U, 4U);
    log_error(LOG_ERROR_TEMPO_READ, ERR_TIMEOUT);
    log_step_note(4U, 5U);

    expect_uart("Step 3 Note = 4\r\n"
                "ADC read failed for delay channel, status code = 4\r\n"
                "Step 4 Note = 5\r\n");
}



static void test_a_message_that_does_not_fit_is_dropped_whole(void)
{
    (void)logger_init(LOGLVL_DEBUG);
    fake_uart_clear();

    // Six 19-byte lines fill 114 of the 128 bytes; the seventh cannot fit
    for (uint8_t note = 0U; note < 7U; note++)
    {
        log_step_note(15U, (uint8_t)(10U + note));
    }
    // Nor can a shorter one: 17 bytes into the 14 that are left
    log_step_note(1U, 2U);

    // Five bytes sent makes room for exactly one more 19-byte line
    for (unsigned int i = 0U; i < 5U; i++)
    {
        log_poll();
    }
    log_step_note(14U, 13U);

    // Only whole lines come out, with nothing of the dropped ones
    expect_uart("Step 15 Note = 10\r\n"
                "Step 15 Note = 11\r\n"
                "Step 15 Note = 12\r\n"
                "Step 15 Note = 13\r\n"
                "Step 15 Note = 14\r\n"
                "Step 15 Note = 15\r\n"
                "Step 14 Note = 13\r\n");
}



static void test_the_queue_keeps_working_after_it_wraps(void)
{
    char expected[EXPECTED_CAPACITY];

    (void)logger_init(LOGLVL_DEBUG);
    // Far more bytes than the queue holds, a line at a time
    for (unsigned int i = 0U; i < 100U; i++)
    {
        const unsigned int step = i % 16U;

        fake_uart_clear();
        log_step_note((uint8_t)step, (uint8_t)(15U - step));
        (void)snprintf(expected, sizeof(expected), "Step %u Note = %u\r\n", step, 15U - step);
        expect_uart(expected);
    }
}



static void test_init_empties_the_queue(void)
{
    (void)logger_init(LOGLVL_DEBUG);
    fake_uart_clear();
    log_step_note(1U, 2U);

    (void)logger_init(LOGLVL_DEBUG);
    expect_uart("");
}



static void test_flush_sends_everything_and_waits_for_the_transmitter(void)
{
    (void)logger_init(LOGLVL_DEBUG);
    fake_uart_clear();
    log_error(LOG_ERROR_INIT, ERR_TIMEOUT);
    log_step_note(1U, 2U);
    g_fake_uart_busy_polls = 25U;
    log_flush();

    TEST_ASSERT_EQUAL(0, g_fake_uart_busy_polls); // It polled until the flag came up
    expect_uart_so_far("Initialization failed, status code = 4\r\nStep 1 Note = 2\r\n");
}



static void test_flush_with_nothing_queued_returns_at_once(void)
{
    (void)logger_init(LOGLVL_DEBUG);
    fake_uart_clear();
    log_flush();

    expect_uart_so_far("");
}



int main(void)
{
    RUN_TEST(test_init_programs_the_usart);
    RUN_TEST(test_decimal_has_no_padding_or_sign);
    RUN_TEST(test_step_note_text_matches_the_printf_format);
    RUN_TEST(test_step_rest_text_matches_the_printf_format);
    RUN_TEST(test_error_text_matches_the_printf_format);
    RUN_TEST(test_unknown_error_id_sends_nothing);
    RUN_TEST(test_level_filters_messages);
    RUN_TEST(test_off_is_never_an_enabled_message_level);
    RUN_TEST(test_nothing_is_sent_until_polled);
    RUN_TEST(test_poll_sends_one_byte_at_a_time);
    RUN_TEST(test_poll_sends_nothing_while_the_transmit_buffer_is_full);
    RUN_TEST(test_poll_with_nothing_queued_leaves_the_usart_alone);
    RUN_TEST(test_messages_are_sent_in_the_order_they_were_logged);
    RUN_TEST(test_a_message_that_does_not_fit_is_dropped_whole);
    RUN_TEST(test_the_queue_keeps_working_after_it_wraps);
    RUN_TEST(test_init_empties_the_queue);
    RUN_TEST(test_flush_sends_everything_and_waits_for_the_transmitter);
    RUN_TEST(test_flush_with_nothing_queued_returns_at_once);
    return TEST_RESULT();
}
