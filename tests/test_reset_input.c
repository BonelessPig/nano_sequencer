/**
 * @file   test_reset_input.c
 * @brief  Host tests for the target reset input
 *         (adapters/target/reset_input.c), compiled against fake registers.
 *         The fake flag register keeps a flag set until a one is written to
 *         it, as the hardware does.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "reset_input.c"          // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define EDGE_MASK    (0x01U) // INTF0 in EIFR
#define OTHER_FLAG   (0x02U) // INTF1, which nothing uses
#define FALLING_EDGE (0x02U) // EICRA: ISC01 alone

static void test_init_sets_the_flag_to_latch_falling_edges(void)
{
    fake_regs_reset();
    g_fake_eicra = 0xFFU; // Stale bits a bootloader might leave must not survive
    reset_input_init();

    TEST_ASSERT_EQUAL(FALLING_EDGE, g_fake_eicra);
}



static void test_init_leaves_the_external_interrupts_off(void)
{
    fake_regs_reset();
    g_fake_eimsk = 0xFFU;
    reset_input_init();

    TEST_ASSERT_EQUAL(0x00, g_fake_eimsk);
}



static void test_init_forgets_an_edge_latched_before_it(void)
{
    fake_regs_reset();
    g_fake_eifr.flags = (uint8_t)(EDGE_MASK | OTHER_FLAG);
    reset_input_init();
    fake_flags_sync();

    // One write, of the edge flag alone, made after the edge was chosen
    TEST_ASSERT_EQUAL(1, g_fake_eifr.writes);
    TEST_ASSERT_EQUAL(EDGE_MASK, g_fake_eifr.last_write);
    TEST_ASSERT_EQUAL(FALLING_EDGE, g_fake_eicra_at_eifr_write);
    TEST_ASSERT_EQUAL(OTHER_FLAG, g_fake_eifr.flags);
    TEST_ASSERT(!reset_input_edge());
}



static void test_no_edge_reads_false_and_writes_nothing(void)
{
    fake_regs_reset();
    TEST_ASSERT(!reset_input_edge());
    TEST_ASSERT(!reset_input_edge());
    fake_flags_sync();
    TEST_ASSERT_EQUAL(0, g_fake_eifr.writes);
}



static void test_an_edge_is_reported_once(void)
{
    fake_regs_reset();
    g_fake_eifr.flags = (uint8_t)EDGE_MASK;

    TEST_ASSERT(reset_input_edge());
    TEST_ASSERT(!reset_input_edge());
    TEST_ASSERT(!reset_input_edge());

    // And the next one is a new edge
    g_fake_eifr.flags = (uint8_t)EDGE_MASK;
    TEST_ASSERT(reset_input_edge());
    TEST_ASSERT(!reset_input_edge());
    TEST_ASSERT_EQUAL(2, g_fake_eifr.writes);
}



static void test_only_its_own_flag_is_read_and_cleared(void)
{
    fake_regs_reset();
    g_fake_eifr.flags = (uint8_t)OTHER_FLAG;
    TEST_ASSERT(!reset_input_edge());
    fake_flags_sync();
    TEST_ASSERT_EQUAL(0, g_fake_eifr.writes);

    g_fake_eifr.flags = (uint8_t)(EDGE_MASK | OTHER_FLAG);
    TEST_ASSERT(reset_input_edge());
    fake_flags_sync();
    TEST_ASSERT_EQUAL(EDGE_MASK, g_fake_eifr.last_write);
    TEST_ASSERT_EQUAL(OTHER_FLAG, g_fake_eifr.flags);
}



static void test_no_pin_and_no_other_peripheral_is_touched(void)
{
    fake_regs_reset();
    g_fake_eifr.flags = (uint8_t)EDGE_MASK;
    reset_input_init();
    (void)reset_input_edge();
    fake_flags_sync();

    TEST_ASSERT_EQUAL(0, g_fake_ddrd); // The pin is register_init()'s job
    TEST_ASSERT_EQUAL(0, g_fake_portd);
    TEST_ASSERT_EQUAL(0, g_fake_ddrb);
    TEST_ASSERT_EQUAL(0, g_fake_portb);
    TEST_ASSERT_EQUAL(0, g_fake_tifr1.writes);
    TEST_ASSERT_EQUAL(0, g_fake_tccr1b);
    TEST_ASSERT_EQUAL(0, g_fake_sreg); // Interrupts are not its business
}



int main(void)
{
    RUN_TEST(test_init_sets_the_flag_to_latch_falling_edges);
    RUN_TEST(test_init_leaves_the_external_interrupts_off);
    RUN_TEST(test_init_forgets_an_edge_latched_before_it);
    RUN_TEST(test_no_edge_reads_false_and_writes_nothing);
    RUN_TEST(test_an_edge_is_reported_once);
    RUN_TEST(test_only_its_own_flag_is_read_and_cleared);
    RUN_TEST(test_no_pin_and_no_other_peripheral_is_touched);
    return TEST_RESULT();
}
