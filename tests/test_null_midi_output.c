/**
 * @file   test_null_midi_output.c
 * @brief  Host tests for the MIDI output of the debug and release builds
 *         (adapters/target/null_midi_output.c). It includes no register
 *         header of its own; the fake one is here so the tests can show
 *         that nothing reaches the UART.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h"
#include "null_midi_output.c"     // The code under test, found via -Iadapters/target
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



static void test_init_does_not_touch_the_usart(void)
{
    fake_regs_reset();

    midi_output_init();
    expect_usart_untouched();
}



static void test_there_is_always_the_most_room(void)
{
    static const uint8_t message[] = { 0x90U, 0x18U, 0x64U };

    fake_regs_reset();
    midi_output_init();

    TEST_ASSERT_EQUAL(255, midi_output_room());
    midi_output_write(message, (uint8_t)sizeof(message));
    TEST_ASSERT_EQUAL(255, midi_output_room()); // Nothing was kept
    expect_usart_untouched();
}



static void test_bytes_are_discarded(void)
{
    static const uint8_t message[] = { 0xFAU, 0xF8U, 0x90U, 0x18U, 0x64U };
    uint8_t              many[255];

    for (unsigned int i = 0U; i < sizeof(many); i++)
    {
        many[i] = 0xF8U;
    }

    fake_regs_reset();
    midi_output_init();

    midi_output_write(message, (uint8_t)sizeof(message));
    midi_output_write(many, (uint8_t)sizeof(many));
    midi_output_write(message, 0U);
    midi_output_write(NULL, 3U);
    expect_usart_untouched();
}



static void test_poll_sends_nothing(void)
{
    static const uint8_t message[] = { 0x80U, 0x18U, 0x00U };

    fake_regs_reset();
    midi_output_init();

    midi_output_write(message, (uint8_t)sizeof(message));
    for (unsigned int i = 0U; i < 10U; i++)
    {
        midi_output_poll();
    }
    expect_usart_untouched(); // Not even the status register was read
}



int main(void)
{
    RUN_TEST(test_init_does_not_touch_the_usart);
    RUN_TEST(test_there_is_always_the_most_room);
    RUN_TEST(test_bytes_are_discarded);
    RUN_TEST(test_poll_sends_nothing);
    return TEST_RESULT();
}
