/**
 * @file   test_clock_input.c
 * @brief  Host tests for the target external clock input
 *         (adapters/target/clock_input.c), compiled against fake registers.
 *         The fake flag register keeps a flag set until a one is written to
 *         it, as the hardware does.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "clock_input.c"          // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define CAPTURE_MASK (0x20U) // ICF1 in TIFR1
#define OTHER_FLAGS  (0x07U) // The timer's overflow and compare flags
#define CAPTURE_SETUP (0x82U) // TCCR1B: noise canceller, falling edge, clock / 8

static void test_init_sets_the_timer_to_capture_falling_edges(void)
{
    fake_regs_reset();
    g_fake_tccr1a = 0xFFU; // Stale bits a bootloader might leave must not survive
    g_fake_tccr1b = 0xFFU;
    clock_input_init();

    TEST_ASSERT_EQUAL(0x00, g_fake_tccr1a);          // Normal counting, no pin driven
    TEST_ASSERT_EQUAL(CAPTURE_SETUP, g_fake_tccr1b); // ICES1 clear: the falling edge
}



static void test_init_leaves_every_timer_interrupt_off(void)
{
    fake_regs_reset();
    g_fake_timsk1 = 0xFFU;
    clock_input_init();

    TEST_ASSERT_EQUAL(0x00, g_fake_timsk1);
}



static void test_init_forgets_an_edge_latched_before_it(void)
{
    fake_regs_reset();
    g_fake_tifr1.flags = (uint8_t)(CAPTURE_MASK | OTHER_FLAGS);
    clock_input_init();
    fake_flags_sync();

    // One write, of the capture flag alone, made after the edge was chosen
    TEST_ASSERT_EQUAL(1, g_fake_tifr1.writes);
    TEST_ASSERT_EQUAL(CAPTURE_MASK, g_fake_tifr1.last_write);
    TEST_ASSERT_EQUAL(CAPTURE_SETUP, g_fake_tccr1b_at_tifr1_write);
    TEST_ASSERT_EQUAL(OTHER_FLAGS, g_fake_tifr1.flags);
    TEST_ASSERT(!clock_input_edge());
}



static void test_no_edge_reads_false_and_writes_nothing(void)
{
    fake_regs_reset();
    TEST_ASSERT(!clock_input_edge());
    TEST_ASSERT(!clock_input_edge());
    fake_flags_sync();
    TEST_ASSERT_EQUAL(0, g_fake_tifr1.writes);
}



static void test_an_edge_is_reported_once(void)
{
    fake_regs_reset();
    g_fake_tifr1.flags = (uint8_t)CAPTURE_MASK;

    TEST_ASSERT(clock_input_edge());
    TEST_ASSERT(!clock_input_edge());
    TEST_ASSERT(!clock_input_edge());

    // And the next one is a new edge
    g_fake_tifr1.flags = (uint8_t)CAPTURE_MASK;
    TEST_ASSERT(clock_input_edge());
    TEST_ASSERT(!clock_input_edge());
    TEST_ASSERT_EQUAL(2, g_fake_tifr1.writes);
}



static void test_only_the_capture_flag_is_read_and_cleared(void)
{
    // The timer's other flags are neither an edge nor disturbed
    fake_regs_reset();
    g_fake_tifr1.flags = (uint8_t)OTHER_FLAGS;
    TEST_ASSERT(!clock_input_edge());
    fake_flags_sync();
    TEST_ASSERT_EQUAL(0, g_fake_tifr1.writes);

    g_fake_tifr1.flags = (uint8_t)(CAPTURE_MASK | OTHER_FLAGS);
    TEST_ASSERT(clock_input_edge());
    fake_flags_sync();
    TEST_ASSERT_EQUAL(CAPTURE_MASK, g_fake_tifr1.last_write);
    TEST_ASSERT_EQUAL(OTHER_FLAGS, g_fake_tifr1.flags);
}



static void test_no_pin_and_no_other_peripheral_is_touched(void)
{
    fake_regs_reset();
    g_fake_tifr1.flags = (uint8_t)CAPTURE_MASK;
    clock_input_init();
    (void)clock_input_edge();
    fake_flags_sync();

    TEST_ASSERT_EQUAL(0, g_fake_ddrb); // The pin is register_init()'s job
    TEST_ASSERT_EQUAL(0, g_fake_portb);
    TEST_ASSERT_EQUAL(0, g_fake_ddrd);
    TEST_ASSERT_EQUAL(0, g_fake_portd);
    TEST_ASSERT_EQUAL(0, g_fake_eifr.writes);
    TEST_ASSERT_EQUAL(0, g_fake_eicra);
    TEST_ASSERT_EQUAL(0, g_fake_sreg); // Interrupts are not its business
}



int main(void)
{
    RUN_TEST(test_init_sets_the_timer_to_capture_falling_edges);
    RUN_TEST(test_init_leaves_every_timer_interrupt_off);
    RUN_TEST(test_init_forgets_an_edge_latched_before_it);
    RUN_TEST(test_no_edge_reads_false_and_writes_nothing);
    RUN_TEST(test_an_edge_is_reported_once);
    RUN_TEST(test_only_the_capture_flag_is_read_and_cleared);
    RUN_TEST(test_no_pin_and_no_other_peripheral_is_touched);
    return TEST_RESULT();
}
