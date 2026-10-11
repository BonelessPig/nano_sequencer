/**
 * @file   test_reset_cause.c
 * @brief  Host tests for the target reset cause adapter
 *         (adapters/target/reset_cause.c), compiled against fake registers.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "reset_cause.c"          // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define ALL_FLAGS (0x0FU) // Power-on, external, brown-out and watchdog



static void test_each_flag_is_reported_in_its_own_bit(void)
{
    // MCUSR bit 0 is power-on, 1 external, 2 brown-out, 3 watchdog, which is
    // the order the port gives them in
    for (uint8_t flags = 0U; flags <= ALL_FLAGS; flags++)
    {
        fake_regs_reset();
        g_fake_mcusr = flags;
        TEST_ASSERT_EQUAL(flags, reset_cause_read());
    }
}



static void test_reading_clears_the_flags(void)
{
    fake_regs_reset();
    g_fake_mcusr = 0x02U;

    TEST_ASSERT_EQUAL(0x02, reset_cause_read());
    TEST_ASSERT_EQUAL(0, g_fake_mcusr);
    TEST_ASSERT_EQUAL(0, reset_cause_read()); // Nothing since
}



static void test_bits_that_are_not_reset_flags_are_left_out(void)
{
    fake_regs_reset();
    g_fake_mcusr = 0xF5U;

    TEST_ASSERT_EQUAL(0x05, reset_cause_read());
    TEST_ASSERT_EQUAL(0, g_fake_mcusr);
}



static void test_no_other_register_is_touched(void)
{
    fake_regs_reset();
    g_fake_mcusr = ALL_FLAGS;
    g_fake_sreg  = 0x80U;
    (void)reset_cause_read();

    TEST_ASSERT_EQUAL(0x80, g_fake_sreg);
    TEST_ASSERT_EQUAL(0, g_fake_eeprom_reads);
    TEST_ASSERT_EQUAL(0, g_fake_ddrb | g_fake_ddrd | g_fake_portb | g_fake_portd);
}



int main(void)
{
    RUN_TEST(test_each_flag_is_reported_in_its_own_bit);
    RUN_TEST(test_reading_clears_the_flags);
    RUN_TEST(test_bits_that_are_not_reset_flags_are_left_out);
    RUN_TEST(test_no_other_register_is_touched);
    return TEST_RESULT();
}
