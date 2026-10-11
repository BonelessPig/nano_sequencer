/**
 * @file   test_watchdog.c
 * @brief  Host tests for the target watchdog adapter
 *         (adapters/target/watchdog.c), compiled against fake registers. The
 *         fake keeps every value written to the control register, in order,
 *         and counts the reset instruction.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "watchdog.c"             // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define OPEN_FOR_CHANGE (0x18U) // WDCE and WDE
#define RESET_AT_250_MS (0x0CU) // WDE and WDP2: no interrupt, 256 ms



static void test_init_opens_the_register_then_sets_the_timeout(void)
{
    fake_regs_reset();
    watchdog_init();
    fake_wdt_sync();

    // Exactly two writes, the second being the one the first allows
    TEST_ASSERT_EQUAL(2, g_fake_wdtcsr_write_count);
    TEST_ASSERT_EQUAL(OPEN_FOR_CHANGE, g_fake_wdtcsr_writes[0]);
    TEST_ASSERT_EQUAL(RESET_AT_250_MS, g_fake_wdtcsr_writes[1]);
    TEST_ASSERT_EQUAL(RESET_AT_250_MS, g_fake_wdtcsr);
}



static void test_init_starts_the_count_again_before_it_changes_anything(void)
{
    fake_regs_reset();
    watchdog_init();
    fake_wdt_sync();

    TEST_ASSERT_EQUAL(1, g_fake_wdr_count);
    TEST_ASSERT_EQUAL(1, g_fake_wdr_count_at_first_wdtcsr_write);
}



static void test_init_touches_nothing_else(void)
{
    fake_regs_reset();
    g_fake_mcusr = 0x08U; // A watchdog reset has just happened
    g_fake_sreg  = 0x00U;
    watchdog_init();

    TEST_ASSERT_EQUAL(0x08, g_fake_mcusr); // The reset cause is still to be read
    TEST_ASSERT_EQUAL(0x00, g_fake_sreg);  // Interrupts are not switched on here
    TEST_ASSERT_EQUAL(0, g_fake_portb);
    TEST_ASSERT_EQUAL(0, g_fake_portd);
}



static void test_kick_is_one_reset_instruction_and_no_register_write(void)
{
    fake_regs_reset();
    watchdog_init();
    fake_wdt_sync();

    for (uint16_t kicks = 1U; kicks <= 3U; kicks++)
    {
        watchdog_kick();
        fake_wdt_sync();
        TEST_ASSERT_EQUAL(1U + kicks, g_fake_wdr_count);
    }
    TEST_ASSERT_EQUAL(2, g_fake_wdtcsr_write_count);
    TEST_ASSERT_EQUAL(RESET_AT_250_MS, g_fake_wdtcsr);
}



int main(void)
{
    RUN_TEST(test_init_opens_the_register_then_sets_the_timeout);
    RUN_TEST(test_init_starts_the_count_again_before_it_changes_anything);
    RUN_TEST(test_init_touches_nothing_else);
    RUN_TEST(test_kick_is_one_reset_instruction_and_no_register_write);
    return TEST_RESULT();
}
