/**
 * @file   test_level_output.c
 * @brief  Host tests for the target level output
 *         (adapters/target/level_output.c), compiled against fake registers.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "level_output.c"         // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define FAST_PWM_8_BIT   (0x03U) // WGM01 and WGM00 in TCCR0A
#define OUTPUT_ON_PIN    (0x80U) // COM0A1 in TCCR0A
#define CLOCK_DIVIDE_64  (0x03U) // CS01 and CS00 in TCCR0B

static void test_init_starts_the_timer_with_its_output_off_the_pin(void)
{
    fake_regs_reset();
    g_fake_ocr0a  = 0xFFU;
    g_fake_tccr0a = 0xFFU;
    level_output_init();

    TEST_ASSERT_EQUAL(FAST_PWM_8_BIT, g_fake_tccr0a);
    TEST_ASSERT_EQUAL(CLOCK_DIVIDE_64, g_fake_tccr0b);
    TEST_ASSERT_EQUAL(0, g_fake_ocr0a);
}



static void test_a_level_puts_the_timer_on_the_pin_with_that_compare_value(void)
{
    static const uint8_t levels[] = { 1U, 2U, 120U, 254U, 255U };

    for (size_t i = 0U; i < (sizeof(levels) / sizeof(levels[0])); i++)
    {
        fake_regs_reset();
        level_output_init();
        level_output_write(levels[i]);
        TEST_ASSERT_EQUAL(levels[i], g_fake_ocr0a);
        TEST_ASSERT_EQUAL(FAST_PWM_8_BIT | OUTPUT_ON_PIN, g_fake_tccr0a);
    }
}



static void test_level_0_takes_the_timer_off_the_pin(void)
{
    fake_regs_reset();
    level_output_init();
    level_output_write(200U);
    level_output_write(0U);

    // The compare value is left: the pin is the port's now, which is low
    TEST_ASSERT_EQUAL(FAST_PWM_8_BIT, g_fake_tccr0a);
    TEST_ASSERT_EQUAL(200, g_fake_ocr0a);

    // And comes back as it went
    level_output_write(7U);
    TEST_ASSERT_EQUAL(FAST_PWM_8_BIT | OUTPUT_ON_PIN, g_fake_tccr0a);
    TEST_ASSERT_EQUAL(7, g_fake_ocr0a);
}



static void test_the_same_level_again_changes_nothing(void)
{
    fake_regs_reset();
    level_output_init();
    for (unsigned int i = 0U; i < 3U; i++)
    {
        level_output_write(99U);
        TEST_ASSERT_EQUAL(99, g_fake_ocr0a);
        TEST_ASSERT_EQUAL(FAST_PWM_8_BIT | OUTPUT_ON_PIN, g_fake_tccr0a);
    }
}



static void test_nothing_but_the_timer_is_touched(void)
{
    fake_regs_reset();
    level_output_init();
    level_output_write(128U);
    level_output_write(0U);

    // The pin's direction and level are register_init()'s job
    TEST_ASSERT_EQUAL(0, g_fake_portd);
    TEST_ASSERT_EQUAL(0, g_fake_ddrd);
    TEST_ASSERT_EQUAL(CLOCK_DIVIDE_64, g_fake_tccr0b);
    TEST_ASSERT_EQUAL(0, g_fake_tccr2a);
    TEST_ASSERT_EQUAL(0, g_fake_tccr2b);
}



int main(void)
{
    RUN_TEST(test_init_starts_the_timer_with_its_output_off_the_pin);
    RUN_TEST(test_a_level_puts_the_timer_on_the_pin_with_that_compare_value);
    RUN_TEST(test_level_0_takes_the_timer_off_the_pin);
    RUN_TEST(test_the_same_level_again_changes_nothing);
    RUN_TEST(test_nothing_but_the_timer_is_touched);
    return TEST_RESULT();
}
