/**
 * @file   test_run_input.c
 * @brief  Host tests for the target run button
 *         (adapters/target/run_input.c), compiled against fake registers.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "run_input.c"            // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define RUN_PIN (0x80U) // PD7

static void test_a_low_pin_is_the_button_held_down(void)
{
    fake_regs_reset();
    g_fake_pind = (uint8_t)(0xFFU & ~RUN_PIN);
    TEST_ASSERT(run_input_read());

    g_fake_pind = 0x00U;
    TEST_ASSERT(run_input_read());
}



static void test_a_high_pin_is_the_button_let_go(void)
{
    fake_regs_reset();
    g_fake_pind = (uint8_t)RUN_PIN; // The pull-up, with every other pin low
    TEST_ASSERT(!run_input_read());

    g_fake_pind = 0xFFU;
    TEST_ASSERT(!run_input_read());
}



static void test_nothing_is_written(void)
{
    fake_regs_reset();
    (void)run_input_read();

    TEST_ASSERT_EQUAL(0, g_fake_portd);
    TEST_ASSERT_EQUAL(0, g_fake_ddrd); // The direction is register_init()'s job
    TEST_ASSERT_EQUAL(0, g_fake_pind);
    TEST_ASSERT_EQUAL(0, g_fake_portb);
    TEST_ASSERT_EQUAL(0, g_fake_ddrb);
}



int main(void)
{
    RUN_TEST(test_a_low_pin_is_the_button_held_down);
    RUN_TEST(test_a_high_pin_is_the_button_let_go);
    RUN_TEST(test_nothing_is_written);
    return TEST_RESULT();
}
