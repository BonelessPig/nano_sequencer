/**
 * @file   test_crc8.c
 * @brief  Host unit tests for the checksum (core/crc8.c). The expected values
 *         were worked out apart from the firmware, in Python, by a routine
 *         that gives the published check value for this CRC.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include "crc8.h"
#include "test_harness.h"

static const uint8_t g_check_text[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };



static void test_the_standard_check_value(void)
{
    TEST_ASSERT_EQUAL(0xF4, crc8(0U, g_check_text, (uint8_t)sizeof(g_check_text)));
}



static void test_single_bytes(void)
{
    static const uint8_t zero    = 0x00U;
    static const uint8_t one     = 0x01U;
    static const uint8_t top_bit = 0x80U;
    static const uint8_t all     = 0xFFU;

    TEST_ASSERT_EQUAL(0x00, crc8(0U, &zero, 1U));
    TEST_ASSERT_EQUAL(0x07, crc8(0U, &one, 1U));     // The polynomial itself
    TEST_ASSERT_EQUAL(0x89, crc8(0U, &top_bit, 1U));
    TEST_ASSERT_EQUAL(0xF3, crc8(0U, &all, 1U));
}



static void test_another_start_value_gives_another_checksum(void)
{
    TEST_ASSERT_EQUAL(0x8D, crc8(1U,g_check_text, (uint8_t)sizeof(g_check_text)));
    TEST_ASSERT_EQUAL(0xFB, crc8(0xFFU, g_check_text, (uint8_t)sizeof(g_check_text)));
}



static void test_a_run_can_be_summed_in_parts(void)
{
    for (uint8_t split = 0U; split <= (uint8_t)sizeof(g_check_text); split++)
    {
        const uint8_t first = crc8(0U, g_check_text, split);
        const uint8_t rest  = (uint8_t)(sizeof(g_check_text) - split);

        TEST_ASSERT_EQUAL(0xF4, crc8(first, &g_check_text[split], rest));
    }
}



static void test_no_bytes_gives_the_start_value(void)
{
    TEST_ASSERT_EQUAL(0x00, crc8(0U, g_check_text, 0U));
    TEST_ASSERT_EQUAL(0x5A, crc8(0x5AU, g_check_text, 0U));
    TEST_ASSERT_EQUAL(0x5A, crc8(0x5AU, NULL, 9U));
}



int main(void)
{
    RUN_TEST(test_the_standard_check_value);
    RUN_TEST(test_single_bytes);
    RUN_TEST(test_another_start_value_gives_another_checksum);
    RUN_TEST(test_a_run_can_be_summed_in_parts);
    RUN_TEST(test_no_bytes_gives_the_start_value);
    return TEST_RESULT();
}
