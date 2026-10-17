/**
 * @file   test_null_midi_input.c
 * @brief  Host tests for the MIDI input of the debug and release builds
 *         (adapters/target/null_midi_input.c). It includes no register
 *         header of its own; the fake one is here so the tests can show
 *         that nothing touches the UART.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <string.h>
#include "fake_atmega328p_regs.h"
#include "null_midi_input.c"      // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define POISON_BYTE (0xA5)

/**
 * @brief Checks that the USART is still as it was after reset: not set up,
 *        not enabled, and its data register never read.
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
    midi_input_init();
    expect_usart_untouched();
}



static void test_nothing_is_ever_received(void)
{
    uint8_t bytes[8];

    fake_regs_reset();
    midi_input_init();
    (void)memset(bytes, POISON_BYTE, sizeof(bytes));

    TEST_ASSERT_EQUAL(0, midi_input_read(bytes, (uint8_t)sizeof(bytes)));
    TEST_ASSERT_EQUAL(0, midi_input_read(bytes, 0U));
    TEST_ASSERT_EQUAL(0, midi_input_read(NULL, 8U));
    TEST_ASSERT_EQUAL(POISON_BYTE, bytes[0]);
    expect_usart_untouched();
}



int main(void)
{
    RUN_TEST(test_init_does_not_touch_the_usart);
    RUN_TEST(test_nothing_is_ever_received);
    return TEST_RESULT();
}
