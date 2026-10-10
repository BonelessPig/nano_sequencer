/**
 * @file   test_timebase.c
 * @brief  Host tests for the target timebase (adapters/target/timebase.c).
 *         The fake register header turns the interrupt handler into an
 *         ordinary function, so a test makes a tick happen by calling it.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "timebase.c"             // The code under test, found via -Iadapters/target
#include "test_harness.h"

/**
 * @brief Fresh registers and a freshly started timebase.
 */
static void start_timebase(void)
{
    fake_regs_reset();
    timebase_init();
}



/**
 * @brief Makes the tick interrupt happen a number of times.
 */
static void run_ticks(unsigned int count)
{
    for (unsigned int i = 0U; i < count; i++)
    {
        fake_timer2_compa_isr();
    }
}



static void test_init_sets_timer2_for_one_interrupt_per_millisecond(void)
{
    start_timebase();

    // 16 MHz / 64 = 250 kHz; counting 0 to 249 and starting again is 1000 Hz
    TEST_ASSERT_EQUAL(250, TIMER_COUNTS_PER_TICK);
    TEST_ASSERT_EQUAL(249, g_fake_ocr2a);
    TEST_ASSERT_EQUAL(0x02, g_fake_tccr2a); // Clear on compare match (WGM21)
    TEST_ASSERT_EQUAL(0x04, g_fake_tccr2b); // System clock / 64 (CS22)
    TEST_ASSERT_EQUAL(0x02, g_fake_timsk2); // Compare match A interrupt (OCIE2A)
    TEST_ASSERT_EQUAL(0, g_fake_tcnt2);     // Counting from zero
}



static void test_init_enables_interrupts_after_the_timer_is_ready(void)
{
    start_timebase();

    TEST_ASSERT_EQUAL(0x80, g_fake_sreg);
    // When the status register was written the timer already had its period,
    // its interrupt and its clock
    TEST_ASSERT_EQUAL(249, g_fake_ocr2a_at_sreg);
    TEST_ASSERT_EQUAL(0x02, g_fake_timsk2_at_sreg);
    TEST_ASSERT_EQUAL(0x04, g_fake_tccr2b_at_sreg);
}



static void test_init_keeps_the_other_status_bits(void)
{
    fake_regs_reset();
    g_fake_sreg = 0x01U; // The carry flag, say
    timebase_init();

    TEST_ASSERT_EQUAL(0x81, g_fake_sreg);
}



static void test_no_ticks_have_elapsed_after_init(void)
{
    start_timebase();
    TEST_ASSERT_EQUAL(0, timebase_elapsed_ticks());
}



static void test_each_interrupt_is_one_tick(void)
{
    start_timebase();

    run_ticks(1U);
    TEST_ASSERT_EQUAL(1, timebase_elapsed_ticks());
    run_ticks(3U);
    TEST_ASSERT_EQUAL(3, timebase_elapsed_ticks());
}



static void test_ticks_are_reported_once(void)
{
    start_timebase();

    run_ticks(5U);
    TEST_ASSERT_EQUAL(5, timebase_elapsed_ticks());
    TEST_ASSERT_EQUAL(0, timebase_elapsed_ticks()); // Already consumed
}



static void test_a_late_caller_loses_no_ticks_up_to_255(void)
{
    start_timebase();

    run_ticks(255U);
    TEST_ASSERT_EQUAL(255, timebase_elapsed_ticks());
}



static void test_elapsed_is_right_across_the_counter_wrap(void)
{
    start_timebase();

    run_ticks(250U);
    TEST_ASSERT_EQUAL(250, timebase_elapsed_ticks());
    run_ticks(10U); // The one-byte counter goes 250 -> 4
    TEST_ASSERT_EQUAL(10, timebase_elapsed_ticks());
    TEST_ASSERT_EQUAL(4, g_tick_count);
}



static void test_init_forgets_earlier_ticks(void)
{
    start_timebase();
    run_ticks(7U);

    timebase_init();
    TEST_ASSERT_EQUAL(0, timebase_elapsed_ticks());
}



int main(void)
{
    RUN_TEST(test_init_sets_timer2_for_one_interrupt_per_millisecond);
    RUN_TEST(test_init_enables_interrupts_after_the_timer_is_ready);
    RUN_TEST(test_init_keeps_the_other_status_bits);
    RUN_TEST(test_no_ticks_have_elapsed_after_init);
    RUN_TEST(test_each_interrupt_is_one_tick);
    RUN_TEST(test_ticks_are_reported_once);
    RUN_TEST(test_a_late_caller_loses_no_ticks_up_to_255);
    RUN_TEST(test_elapsed_is_right_across_the_counter_wrap);
    RUN_TEST(test_init_forgets_earlier_ticks);
    return TEST_RESULT();
}
