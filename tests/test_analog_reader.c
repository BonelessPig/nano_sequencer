/**
 * @file   test_analog_reader.c
 * @brief  Host tests for the target tempo input (adapters/target/analog_reader.c),
 *         compiled against fake registers. The fake ADC finishes a conversion
 *         after a set number of polls, so both the normal path and the
 *         timeout can be driven.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "analog_reader.c"        // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define ADC_ENABLED     (0x87U)   // ADCSRA as register_init leaves it
#define ADMUX_TEMPO     (0x46U)   // AVcc reference, channel 6
#define UNTOUCHED_VALUE (0xBEEFU) // Marks a result variable the adapter must not write

/**
 * @brief Fake registers at power-on state, with the ADC enabled and idle.
 */
static void start_adc(void)
{
    fake_regs_reset();
    g_fake_adcsra = ADC_ENABLED;
}



static void test_read_returns_the_conversion_result(void)
{
    static const uint16_t readings[] = { 0U, 1U, 512U, 1023U };

    for (size_t i = 0U; i < (sizeof(readings) / sizeof(readings[0])); i++)
    {
        uint16_t value = UNTOUCHED_VALUE;

        start_adc();
        g_fake_adc = readings[i];

        TEST_ASSERT_EQUAL(STATUS_OK, tempo_input_read(&value));
        TEST_ASSERT_EQUAL(readings[i], value);
    }
}



static void test_read_selects_channel_6_with_avcc_reference(void)
{
    uint16_t value = 0U;

    start_adc();
    g_fake_admux = 0xFFU; // Stale bits must not survive
    (void)tempo_input_read(&value);

    TEST_ASSERT_EQUAL(ADMUX_TEMPO, g_fake_admux);
}



static void test_read_keeps_the_adc_configuration(void)
{
    uint16_t value = 0U;

    start_adc();
    (void)tempo_input_read(&value);

    TEST_ASSERT_EQUAL(ADC_ENABLED, g_fake_adcsra); // Start bit cleared itself; the rest unchanged
}



static void test_read_waits_for_a_slow_conversion(void)
{
    uint16_t value = UNTOUCHED_VALUE;

    start_adc();
    g_fake_adc            = 700U;
    g_fake_adc_busy_polls = 50U;

    TEST_ASSERT_EQUAL(STATUS_OK, tempo_input_read(&value));
    TEST_ASSERT_EQUAL(700, value);
    TEST_ASSERT_EQUAL(0, g_fake_adc_busy_polls); // It kept polling until the end
}



static void test_conversion_finishing_on_the_last_poll_succeeds(void)
{
    uint16_t value = UNTOUCHED_VALUE;

    start_adc();
    g_fake_adc            = 300U;
    g_fake_adc_busy_polls = (uint16_t)(ADC_TIMEOUT_LOOPS - 1U);

    TEST_ASSERT_EQUAL(STATUS_OK, tempo_input_read(&value));
    TEST_ASSERT_EQUAL(300, value);
}



static void test_conversion_one_poll_too_slow_times_out(void)
{
    uint16_t value = UNTOUCHED_VALUE;

    start_adc();
    g_fake_adc            = 300U;
    g_fake_adc_busy_polls = (uint16_t)ADC_TIMEOUT_LOOPS;

    TEST_ASSERT_EQUAL(ERR_TIMEOUT, tempo_input_read(&value));
    TEST_ASSERT_EQUAL(UNTOUCHED_VALUE, value);
    TEST_ASSERT_EQUAL(0, g_fake_adc_busy_polls); // Exactly the poll budget was spent
}



static void test_stuck_conversion_gives_up_after_the_poll_budget(void)
{
    uint16_t value = UNTOUCHED_VALUE;

    start_adc();
    g_fake_adc_busy_polls = UINT16_MAX; // Far longer than the adapter will wait

    TEST_ASSERT_EQUAL(ERR_TIMEOUT, tempo_input_read(&value));
    TEST_ASSERT_EQUAL(UNTOUCHED_VALUE, value);
    TEST_ASSERT_EQUAL(UINT16_MAX - ADC_TIMEOUT_LOOPS, g_fake_adc_busy_polls);
}



static void test_null_destination_is_rejected_without_starting_a_conversion(void)
{
    start_adc();
    g_fake_admux = 0x11U;

    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, tempo_input_read(NULL));
    TEST_ASSERT_EQUAL(0x11, g_fake_admux);
    TEST_ASSERT_EQUAL(ADC_ENABLED, g_fake_adcsra);
}



static void test_channel_range_is_checked(void)
{
    uint16_t value = UNTOUCHED_VALUE;

    start_adc();
    g_fake_adc = 42U;

    // Channel 7 is the highest the chip has
    TEST_ASSERT_EQUAL(STATUS_OK, read_analog_value(&value, ADC_CHANNEL_MAX));
    TEST_ASSERT_EQUAL(42, value);
    TEST_ASSERT_EQUAL(0x47, g_fake_admux);

    start_adc();
    value        = UNTOUCHED_VALUE;
    g_fake_admux = 0x11U;
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, read_analog_value(&value, (uint8_t)(ADC_CHANNEL_MAX + 1U)));
    TEST_ASSERT_EQUAL(UNTOUCHED_VALUE, value);
    TEST_ASSERT_EQUAL(0x11, g_fake_admux);
}



int main(void)
{
    RUN_TEST(test_read_returns_the_conversion_result);
    RUN_TEST(test_read_selects_channel_6_with_avcc_reference);
    RUN_TEST(test_read_keeps_the_adc_configuration);
    RUN_TEST(test_read_waits_for_a_slow_conversion);
    RUN_TEST(test_conversion_finishing_on_the_last_poll_succeeds);
    RUN_TEST(test_conversion_one_poll_too_slow_times_out);
    RUN_TEST(test_stuck_conversion_gives_up_after_the_poll_budget);
    RUN_TEST(test_null_destination_is_rejected_without_starting_a_conversion);
    RUN_TEST(test_channel_range_is_checked);
    return TEST_RESULT();
}
