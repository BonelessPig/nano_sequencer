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

    TEST_ASSERT_EQUAL(0x02, g_fake_ddrb); // PB1 (shift register load)
    TEST_ASSERT_EQUAL(0x02, g_fake_ddrc); // PC1
    TEST_ASSERT_EQUAL(0x00, g_fake_ddrd); // Nothing: PD0 and PD1 are the UART's, PD2 to PD7 are free
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
    TEST_ASSERT_EQUAL(0xFF, g_fake_ddrd); // Port D is not touched
}



static void test_adc_is_enabled_with_prescaler_128(void)
{
    fake_regs_reset();
    g_fake_adcsra = 0x38U; // Stale bits a bootloader might leave must not survive
    (void)register_init();

    TEST_ASSERT_EQUAL(0x87, g_fake_adcsra); // ADEN, ADPS2..0; no conversion started
}



static void test_load_line_idles_high_and_other_levels_are_left_alone(void)
{
    // Once with every other level low and once with them high
    fake_regs_reset();
    (void)register_init();
    TEST_ASSERT_EQUAL(0x02, g_fake_portb); // PB1 high: the chain in shift mode
    TEST_ASSERT_EQUAL(0x00, g_fake_portc);
    TEST_ASSERT_EQUAL(0x00, g_fake_portd);

    fake_regs_reset();
    g_fake_portb = 0x58U;
    g_fake_portc = 0xA5U;
    g_fake_portd = 0x3CU;
    (void)register_init();
    TEST_ASSERT_EQUAL(0x5A, g_fake_portb);
    TEST_ASSERT_EQUAL(0xA5, g_fake_portc);
    TEST_ASSERT_EQUAL(0x3C, g_fake_portd);
}



static void test_load_line_is_high_before_it_becomes_an_output(void)
{
    // Made an output while its level was still low, the pin would pull the
    // load line low for a moment
    fake_regs_reset();
    (void)register_init();

    TEST_ASSERT(g_fake_b_ddrb_accessed);
    TEST_ASSERT(0U != (g_fake_portb_at_first_ddrb & FAKE_SHIFT_LOAD_MASK));
}



int main(void)
{
    RUN_TEST(test_init_reports_success);
    RUN_TEST(test_output_pins_are_set_from_all_inputs);
    RUN_TEST(test_input_pins_are_cleared_and_other_bits_kept);
    RUN_TEST(test_adc_is_enabled_with_prescaler_128);
    RUN_TEST(test_load_line_idles_high_and_other_levels_are_left_alone);
    RUN_TEST(test_load_line_is_high_before_it_becomes_an_output);
    return TEST_RESULT();
}
