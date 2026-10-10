/**
 * @file   test_register_init.c
 * @brief  Host tests for the target pin and ADC setup
 *         (adapters/target/register_init.c), compiled against fake registers.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "register_init.c"        // The code under test, found via -Iadapters/target
#include "test_harness.h"

static void test_init_reports_success(void)
{
    fake_regs_reset();
    TEST_ASSERT_EQUAL(STATUS_OK, register_init());
}



static void test_output_pins_are_set_from_all_inputs(void)
{
    fake_regs_reset(); // Every direction bit 0 (input), as after a reset
    (void)register_init();

    TEST_ASSERT_EQUAL(0x20, g_fake_ddrb); // PB5
    TEST_ASSERT_EQUAL(0x02, g_fake_ddrc); // PC1
    TEST_ASSERT_EQUAL(0x0D, g_fake_ddrd); // PD0, PD2 (load), PD3 (clock)
}



static void test_input_pins_are_cleared_and_other_bits_kept(void)
{
    fake_regs_reset();
    g_fake_ddrb = 0xFFU;
    g_fake_ddrc = 0xFFU;
    g_fake_ddrd = 0xFFU;
    (void)register_init();

    TEST_ASSERT_EQUAL(0xFF, g_fake_ddrb);
    TEST_ASSERT_EQUAL(0xFE, g_fake_ddrc); // PC0 made an input
    TEST_ASSERT_EQUAL(0xEF, g_fake_ddrd); // PD4 (shift register data) made an input
}



static void test_adc_is_enabled_with_prescaler_128(void)
{
    fake_regs_reset();
    g_fake_adcsra = 0x38U; // Stale bits a bootloader might leave must not survive
    (void)register_init();

    TEST_ASSERT_EQUAL(0x87, g_fake_adcsra); // ADEN, ADPS2..0; no conversion started
}



static void test_output_levels_are_left_alone(void)
{
    fake_regs_reset();
    g_fake_portb = 0x5AU;
    g_fake_portc = 0xA5U;
    g_fake_portd = 0x3CU;
    (void)register_init();

    TEST_ASSERT_EQUAL(0x5A, g_fake_portb);
    TEST_ASSERT_EQUAL(0xA5, g_fake_portc);
    TEST_ASSERT_EQUAL(0x3C, g_fake_portd);
}



int main(void)
{
    RUN_TEST(test_init_reports_success);
    RUN_TEST(test_output_pins_are_set_from_all_inputs);
    RUN_TEST(test_input_pins_are_cleared_and_other_bits_kept);
    RUN_TEST(test_adc_is_enabled_with_prescaler_128);
    RUN_TEST(test_output_levels_are_left_alone);
    return TEST_RESULT();
}
