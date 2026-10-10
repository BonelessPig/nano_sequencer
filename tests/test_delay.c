/**
 * @file   test_delay.c
 * @brief  Host tests for the target delay (adapters/target/delay.c). The
 *         avr-gcc delay builtin does not exist on a PC, so this file defines a
 *         function of the same name that adds up the cycles asked for.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <stdint.h>
#include "delay.c" // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define CYCLES_PER_MS_AT_16_MHZ (16000UL)

static uint32_t g_delay_calls  = 0U; // Calls to the delay builtin
static uint64_t g_delay_cycles = 0U; // Total clock cycles asked for

void __builtin_avr_delay_cycles(unsigned long cycles)
{
    g_delay_calls++;
    g_delay_cycles += cycles;
}



/**
 * @brief Runs one delay and checks how many clock cycles it burned.
 */
static void expect_delay(uint16_t ms)
{
    g_delay_calls  = 0U;
    g_delay_cycles = 0U;
    delay_wait_ms(ms);

    TEST_ASSERT_EQUAL(ms, g_delay_calls); // One builtin call per millisecond
    TEST_ASSERT(((uint64_t)ms * CYCLES_PER_MS_AT_16_MHZ) == g_delay_cycles);
}



static void test_one_millisecond_is_16000_cycles(void)
{
    TEST_ASSERT_EQUAL(CYCLES_PER_MS_AT_16_MHZ, CLOCK_CYCLES_PER_MS);
    expect_delay(1U);
}



static void test_zero_returns_at_once(void)
{
    expect_delay(0U);
}



static void test_delay_scales_with_the_request(void)
{
    static const uint16_t requests[] = { 2U, 100U, 255U, 1000U, UINT16_MAX };

    for (size_t i = 0U; i < (sizeof(requests) / sizeof(requests[0])); i++)
    {
        expect_delay(requests[i]);
    }
}



int main(void)
{
    RUN_TEST(test_one_millisecond_is_16000_cycles);
    RUN_TEST(test_zero_returns_at_once);
    RUN_TEST(test_delay_scales_with_the_request);
    return TEST_RESULT();
}
