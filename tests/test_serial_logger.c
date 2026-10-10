/**
 * @file   test_serial_logger.c
 * @brief  Host tests for the target serial logger (adapters/target/serial_logger.c).
 *         The adapter's source is included directly, with the MCU register
 *         header replaced by a fake, so the exact bytes it would send over the
 *         UART can be compared with the expected text.
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

#define EXPECTED_CAPACITY (128U)

/**
 * @brief Checks that the bytes captured from the UART are exactly p_expected.
 */
static void expect_uart(const char *p_expected)
{
    const size_t expected_length = strlen(p_expected);

    TEST_ASSERT_EQUAL(expected_length, g_fake_uart_length);
    if (expected_length == g_fake_uart_length)
    {
        TEST_ASSERT_EQUAL(0, memcmp(p_expected, g_fake_uart, expected_length));
    }
}



static void test_init_programs_the_usart(void)
{
    TEST_ASSERT_EQUAL(STATUS_OK, serial_init(LOGLVL_DEBUG));

    // 16 MHz / (16 * 9600) - 1 = 103
    TEST_ASSERT_EQUAL(0, g_fake_ubrr0h);
    TEST_ASSERT_EQUAL(103, g_fake_ubrr0l);
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

    (void)serial_init(LOGLVL_DEBUG);
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



static void test_error_text_matches_the_printf_format(void)
{
    static const port_status_t statuses[] =
    {
        STATUS_OK, ERR_GENERAL, ERR_INVALID_PARAM, ERR_BUSY, ERR_TIMEOUT, ERR_NOT_SUPPORTED, ERR_UNKNOWN
    };
    char expected[EXPECTED_CAPACITY];

    (void)serial_init(LOGLVL_DEBUG);
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
    (void)serial_init(LOGLVL_DEBUG);
    fake_uart_clear();
    log_error((log_error_id_t)99, ERR_GENERAL);
    expect_uart("");
}



static void test_level_filters_messages(void)
{
    // OFF: nothing at all
    (void)serial_init(LOGLVL_OFF);
    fake_uart_clear();
    log_step_note(1U, 2U);
    log_error(LOG_ERROR_INIT, ERR_GENERAL);
    expect_uart("");

    // ERROR: errors pass, debug-level step notes do not
    (void)serial_init(LOGLVL_ERROR);
    fake_uart_clear();
    log_step_note(1U, 2U);
    expect_uart("");
    log_error(LOG_ERROR_INIT, ERR_GENERAL);
    expect_uart("Initialization failed, status code = 1\r\n");

    // FATAL is less verbose than ERROR, so errors are dropped too
    (void)serial_init(LOGLVL_FATAL);
    fake_uart_clear();
    log_error(LOG_ERROR_INIT, ERR_GENERAL);
    expect_uart("");

    // DEBUG and TRACE: both kinds pass
    (void)serial_init(LOGLVL_TRACE);
    fake_uart_clear();
    log_step_note(1U, 2U);
    log_error(LOG_ERROR_STEP_READ, ERR_INVALID_PARAM);
    expect_uart("Step 1 Note = 2\r\nStep note read failed, status code = 2\r\n");
}



static void test_off_is_never_an_enabled_message_level(void)
{
    // No message is sent at level OFF today; this pins the rule for when one might be
    (void)serial_init(LOGLVL_TRACE);
    TEST_ASSERT(!is_level_enabled(LOGLVL_OFF));

    (void)serial_init(LOGLVL_OFF);
    TEST_ASSERT(!is_level_enabled(LOGLVL_OFF));
}



static void test_write_waits_while_the_transmit_buffer_is_full(void)
{
    (void)serial_init(LOGLVL_DEBUG);
    fake_uart_clear();
    g_fake_uart_busy_polls = 25U;
    write_char('A');

    TEST_ASSERT_EQUAL(0, g_fake_uart_busy_polls); // It polled until the flag came up
    expect_uart("A");                             // And sent the byte once, afterwards

    // A whole message whose first byte has to wait still arrives intact
    fake_uart_clear();
    g_fake_uart_busy_polls = 3U;
    log_step_note(15U, 9U);
    expect_uart("Step 15 Note = 9\r\n");
}



int main(void)
{
    RUN_TEST(test_init_programs_the_usart);
    RUN_TEST(test_decimal_has_no_padding_or_sign);
    RUN_TEST(test_step_note_text_matches_the_printf_format);
    RUN_TEST(test_error_text_matches_the_printf_format);
    RUN_TEST(test_unknown_error_id_sends_nothing);
    RUN_TEST(test_level_filters_messages);
    RUN_TEST(test_off_is_never_an_enabled_message_level);
    RUN_TEST(test_write_waits_while_the_transmit_buffer_is_full);
    return TEST_RESULT();
}
