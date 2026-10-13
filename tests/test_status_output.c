/**
 * @file   test_status_output.c
 * @brief  Host tests for the target status light
 *         (adapters/target/status_output.c), compiled against fake registers.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "status_output.c"        // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define STATUS_PIN (0x08U) // PD3

static void test_lit_sets_the_status_pin_and_no_other(void)
{
    fake_regs_reset();
    status_output_write(true);
    TEST_ASSERT_EQUAL(STATUS_PIN, g_fake_portd);

    status_output_write(true); // Already high: no change
    TEST_ASSERT_EQUAL(STATUS_PIN, g_fake_portd);
}



static void test_dark_clears_the_status_pin_and_no_other(void)
{
    fake_regs_reset();
    g_fake_portd = 0xFFU;
    status_output_write(false);
    TEST_ASSERT_EQUAL(0xFFU & ~STATUS_PIN, g_fake_portd);

    status_output_write(false); // Already low: no change
    TEST_ASSERT_EQUAL(0xFFU & ~STATUS_PIN, g_fake_portd);

    status_output_write(true);
    TEST_ASSERT_EQUAL(0xFF, g_fake_portd);
}



static void test_nothing_but_the_port_d_level_is_touched(void)
{
    fake_regs_reset();
    status_output_write(true);
    status_output_write(false);

    TEST_ASSERT_EQUAL(0, g_fake_portd);
    TEST_ASSERT_EQUAL(0, g_fake_ddrd); // The direction is register_init()'s job
    TEST_ASSERT_EQUAL(0, g_fake_portb);
    TEST_ASSERT_EQUAL(0, g_fake_ddrb);
    TEST_ASSERT_EQUAL(0, g_fake_portc);
    TEST_ASSERT_EQUAL(0, g_fake_ddrc);
}



int main(void)
{
    RUN_TEST(test_lit_sets_the_status_pin_and_no_other);
    RUN_TEST(test_dark_clears_the_status_pin_and_no_other);
    RUN_TEST(test_nothing_but_the_port_d_level_is_touched);
    return TEST_RESULT();
}
