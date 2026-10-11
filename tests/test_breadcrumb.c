/**
 * @file   test_breadcrumb.c
 * @brief  Host tests for the target breadcrumb adapter
 *         (adapters/target/breadcrumb.c). That its byte survives a reset is
 *         a matter of where the linker puts it, which only the emulated
 *         board can show; these check what the two functions do with it.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "breadcrumb.c"           // The code under test, found via -Iadapters/target
#include "test_harness.h"



static void test_the_place_last_written_is_read_back(void)
{
    static const uint8_t places[] = { 1U, 5U, 0U, 255U, 2U };

    for (size_t i = 0U; i < (sizeof(places) / sizeof(places[0])); i++)
    {
        breadcrumb_write(places[i]);
        TEST_ASSERT_EQUAL(places[i], breadcrumb_read());
        TEST_ASSERT_EQUAL(places[i], breadcrumb_read()); // Reading does not clear it
    }
}



static void test_no_register_is_touched(void)
{
    fake_regs_reset();
    g_fake_mcusr = 0x08U;
    breadcrumb_write(3U);
    (void)breadcrumb_read();
    fake_wdt_sync();

    TEST_ASSERT_EQUAL(0x08, g_fake_mcusr); // The reset cause is not its business
    TEST_ASSERT_EQUAL(0, g_fake_wdtcsr_write_count);
    TEST_ASSERT_EQUAL(0, g_fake_wdr_count);
    TEST_ASSERT_EQUAL(0, g_fake_sreg);
    TEST_ASSERT_EQUAL(0, g_fake_portb);
    TEST_ASSERT_EQUAL(0, g_fake_portd);
}



int main(void)
{
    RUN_TEST(test_the_place_last_written_is_read_back);
    RUN_TEST(test_no_register_is_touched);
    return TEST_RESULT();
}
