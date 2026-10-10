/**
 * @file   test_panel.c
 * @brief  Host unit tests for the front panel layout (core/panel.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "panel.h"
#include "test_harness.h"

static void test_layout_constants(void)
{
    // 16 steps of 4 bits fill exactly 8 bytes, with no partial byte
    TEST_ASSERT_EQUAL(16, PANEL_STEP_COUNT);
    TEST_ASSERT_EQUAL(8, PANEL_RAW_BYTE_COUNT);
    TEST_ASSERT_EQUAL(0, PANEL_BITS_PER_BYTE % PANEL_VALUE_BITS);
    TEST_ASSERT_EQUAL(PANEL_STEP_COUNT * PANEL_VALUE_BITS,
                      PANEL_RAW_BYTE_COUNT * PANEL_BITS_PER_BYTE);
    TEST_ASSERT_EQUAL(15, PANEL_VALUE_MAX);
}



static void test_values_decode_high_nibble_first(void)
{
    uint8_t raw[PANEL_RAW_BYTE_COUNT];

    (void)memset(raw, 0, sizeof(raw));
    raw[0] = 0xA5U; // step 0 = 0xA, step 1 = 0x5
    raw[1] = 0x0FU; // step 2 = 0x0, step 3 = 0xF
    raw[7] = 0x3CU; // step 14 = 0x3, step 15 = 0xC

    TEST_ASSERT_EQUAL(0xA, panel_step_value(raw, 0U));
    TEST_ASSERT_EQUAL(0x5, panel_step_value(raw, 1U));
    TEST_ASSERT_EQUAL(0x0, panel_step_value(raw, 2U));
    TEST_ASSERT_EQUAL(0xF, panel_step_value(raw, 3U));
    TEST_ASSERT_EQUAL(0x3, panel_step_value(raw, 14U));
    TEST_ASSERT_EQUAL(0xC, panel_step_value(raw, 15U));
}



static void test_each_step_is_independent(void)
{
    // Set one step at a time to the maximum; every other step must read as 0
    for (uint8_t target = 0U; target < PANEL_STEP_COUNT; target++)
    {
        uint8_t       raw[PANEL_RAW_BYTE_COUNT];
        const uint8_t byte_index = (uint8_t)(target / 2U);
        const bool    b_high     = (0U == (target % 2U));

        (void)memset(raw, 0, sizeof(raw));
        raw[byte_index] = b_high ? 0xF0U : 0x0FU;

        for (uint8_t step = 0U; step < PANEL_STEP_COUNT; step++)
        {
            TEST_ASSERT_EQUAL((step == target) ? PANEL_VALUE_MAX : 0U,
                              panel_step_value(raw, step));
        }
    }
}



static void test_step_beyond_the_last_wraps_round(void)
{
    uint8_t raw[PANEL_RAW_BYTE_COUNT];

    (void)memset(raw, 0, sizeof(raw));
    raw[0] = 0x9BU; // step 0 = 9, step 1 = 0xB

    TEST_ASSERT_EQUAL(9, panel_step_value(raw, 16U));
    TEST_ASSERT_EQUAL(0xB, panel_step_value(raw, 17U));
    TEST_ASSERT_EQUAL(0, panel_step_value(raw, 255U)); // Step 15
}



static void test_null_raw_bytes_give_zero(void)
{
    TEST_ASSERT_EQUAL(0, panel_step_value(NULL, 0U));
}



int main(void)
{
    RUN_TEST(test_layout_constants);
    RUN_TEST(test_values_decode_high_nibble_first);
    RUN_TEST(test_each_step_is_independent);
    RUN_TEST(test_step_beyond_the_last_wraps_round);
    RUN_TEST(test_null_raw_bytes_give_zero);
    return TEST_RESULT();
}
