/**
 * @file   test_null_logger.c
 * @brief  Host tests for the release build's logger (adapters/target/null_logger.c).
 *         It includes no register header of its own; the fake one is here so
 *         the tests can show that nothing reaches the UART.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h"
#include "null_logger.c"          // The code under test, found via -Iadapters/target
#include "test_harness.h"

/**
 * @brief Checks that the USART is still as it was after reset: not set up,
 *        not enabled, and nothing sent.
 */
static void expect_usart_untouched(void)
{
    TEST_ASSERT_EQUAL(0, g_fake_ubrr0h);
    TEST_ASSERT_EQUAL(0, g_fake_ubrr0l);
    TEST_ASSERT_EQUAL(0, g_fake_ucsr0a);
    TEST_ASSERT_EQUAL(0, g_fake_ucsr0b);
    TEST_ASSERT_EQUAL(0, g_fake_ucsr0c);
    TEST_ASSERT_EQUAL(0, g_fake_uart_length);
}



static void test_init_succeeds_at_every_level_without_touching_the_usart(void)
{
    fake_regs_reset();

    TEST_ASSERT_EQUAL(STATUS_OK, logger_init(LOGLVL_OFF));
    TEST_ASSERT_EQUAL(STATUS_OK, logger_init(LOGLVL_DEBUG));
    TEST_ASSERT_EQUAL(STATUS_OK, logger_init(LOGLVL_TRACE));
    expect_usart_untouched();
}



static void test_messages_are_discarded(void)
{
    fake_regs_reset();
    (void)logger_init(LOGLVL_TRACE);

    log_step_note(15U, 9U);
    log_error(LOG_ERROR_INIT, ERR_GENERAL);
    log_error(LOG_ERROR_STEP_READ, ERR_INVALID_PARAM);
    log_error(LOG_ERROR_TEMPO_READ, ERR_TIMEOUT);
    expect_usart_untouched();
}



static void test_poll_and_flush_send_nothing(void)
{
    fake_regs_reset();
    (void)logger_init(LOGLVL_TRACE);

    log_step_note(15U, 9U);
    log_poll();
    log_flush(); // Must return: there is nothing to wait for
    expect_usart_untouched();
}



int main(void)
{
    RUN_TEST(test_init_succeeds_at_every_level_without_touching_the_usart);
    RUN_TEST(test_messages_are_discarded);
    RUN_TEST(test_poll_and_flush_send_nothing);
    return TEST_RESULT();
}
