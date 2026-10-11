/**
 * @file   test_bank.c
 * @brief  Host unit tests for the bank of amounts (core/bank.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "bank.h"
#include "test_harness.h"

#define POISON_BYTE (0xA5)
#define HOME_SLOT   (0U)
#define NO_HOME     (BANK_SLOT_COUNT)

static bank_state_t g_state;

/**
 * @brief Runs one tick with a valid reading.
 */
static void tick(uint8_t selected, uint16_t reading)
{
    bank_inputs_t in;

    in.selected        = selected;
    in.reading         = reading;
    in.b_reading_valid = true;
    bank_tick(&g_state, &in);
}

/**
 * @brief Checks every slot's amount but one is 0.
 */
static void expect_zero_but(uint8_t slot)
{
    for (uint8_t other = 0U; other < BANK_SLOT_COUNT; other++)
    {
        if (other != slot)
        {
            TEST_ASSERT_EQUAL(0, bank_amount(&g_state, other));
        }
    }
}



static void test_init_empties_the_bank(void)
{
    (void)memset(&g_state, POISON_BYTE, sizeof(g_state));
    bank_init(&g_state, 5U);
    expect_zero_but(BANK_SLOT_COUNT);
    TEST_ASSERT_EQUAL(0, g_state.selected);
    TEST_ASSERT_EQUAL(5, g_state.home);
    TEST_ASSERT(!g_state.b_homed);
    TEST_ASSERT(!g_state.pickup.b_attached);
    TEST_ASSERT(!g_state.pickup.b_known);
}



static void test_the_home_slot_takes_the_first_reading_wherever_the_selector_is(void)
{
    bank_init(&g_state, 2U);
    tick(5U, 700U);
    TEST_ASSERT_EQUAL(700, bank_amount(&g_state, 2U));
    expect_zero_but(2U);

    // Once only: the pot is not setting it
    tick(5U, 300U);
    TEST_ASSERT_EQUAL(700, bank_amount(&g_state, 2U));
    expect_zero_but(2U);
}



static void test_a_selected_home_slot_follows_the_pot_from_the_start(void)
{
    bank_init(&g_state, HOME_SLOT);
    tick(HOME_SLOT, 700U);
    TEST_ASSERT_EQUAL(700, bank_amount(&g_state, HOME_SLOT));
    tick(HOME_SLOT, 300U);
    TEST_ASSERT_EQUAL(300, bank_amount(&g_state, HOME_SLOT));
    tick(HOME_SLOT, 1023U);
    TEST_ASSERT_EQUAL(1023, bank_amount(&g_state, HOME_SLOT));
    expect_zero_but(HOME_SLOT);
}



static void test_a_reading_that_is_not_valid_is_not_the_first(void)
{
    bank_inputs_t in;

    bank_init(&g_state, HOME_SLOT);
    in.selected        = HOME_SLOT;
    in.reading         = 700U;
    in.b_reading_valid = false;
    bank_tick(&g_state, &in);
    TEST_ASSERT_EQUAL(0, bank_amount(&g_state, HOME_SLOT));
    TEST_ASSERT(!g_state.b_homed);

    tick(HOME_SLOT, 600U);
    TEST_ASSERT_EQUAL(600, bank_amount(&g_state, HOME_SLOT));

    // Nor does a later one move anything
    in.reading = 100U;
    bank_tick(&g_state, &in);
    TEST_ASSERT_EQUAL(600, bank_amount(&g_state, HOME_SLOT));
}



static void test_with_no_home_slot_every_amount_waits_for_the_pot(void)
{
    bank_init(&g_state, NO_HOME);
    tick(2U, 700U);
    expect_zero_but(BANK_SLOT_COUNT);
    TEST_ASSERT(g_state.b_homed);

    // Down to meet the 0 it holds, and then up
    tick(2U, 0U);
    tick(2U, 400U);
    TEST_ASSERT_EQUAL(400, bank_amount(&g_state, 2U));
    expect_zero_but(2U);
}



static void test_another_slot_has_to_be_met_before_the_pot_sets_it(void)
{
    bank_init(&g_state, HOME_SLOT);
    tick(HOME_SLOT, 700U);

    // The pot is at 700 and slot 1 holds 0: nothing moves, however the pot
    // is turned short of meeting it
    tick(1U, 700U);
    tick(1U, 650U);
    tick(1U, 1023U);
    tick(1U, 20U);
    TEST_ASSERT_EQUAL(0, bank_amount(&g_state, 1U));
    TEST_ASSERT_EQUAL(700, bank_amount(&g_state, HOME_SLOT));

    // Met at the bottom, it follows the pot up, and the slot left behind
    // keeps what it had
    tick(1U, 0U);
    tick(1U, 300U);
    TEST_ASSERT_EQUAL(300, bank_amount(&g_state, 1U));
    TEST_ASSERT_EQUAL(700, bank_amount(&g_state, HOME_SLOT));
}



static void test_passing_through_a_slot_changes_nothing(void)
{
    bank_init(&g_state, HOME_SLOT);
    tick(HOME_SLOT, 300U);

    // On the way from slot 0 to slot 4 and back, with the pot left alone
    for (uint8_t slot = 1U; slot <= 4U; slot++)
    {
        tick(slot, 300U);
    }
    for (uint8_t slot = 3U; slot >= 1U; slot--)
    {
        tick(slot, 300U);
    }
    tick(HOME_SLOT, 300U);
    TEST_ASSERT_EQUAL(300, bank_amount(&g_state, HOME_SLOT));
    expect_zero_but(HOME_SLOT);

    // The pot is still where slot 0 was left, so it has it again at once
    tick(HOME_SLOT, 310U);
    TEST_ASSERT_EQUAL(310, bank_amount(&g_state, HOME_SLOT));
}



static void test_coming_back_to_a_slot_the_pot_has_left_waits_too(void)
{
    bank_init(&g_state, HOME_SLOT);
    tick(HOME_SLOT, 700U);
    tick(1U, 0U);
    tick(1U, 300U);

    // Slot 0 holds 700 and the pot is at 300
    tick(HOME_SLOT, 300U);
    tick(HOME_SLOT, 500U);
    TEST_ASSERT_EQUAL(700, bank_amount(&g_state, HOME_SLOT));

    // Passing 700 picks it up
    tick(HOME_SLOT, 800U);
    TEST_ASSERT_EQUAL(800, bank_amount(&g_state, HOME_SLOT));
    TEST_ASSERT_EQUAL(300, bank_amount(&g_state, 1U));
}



static void test_the_same_inputs_again_change_nothing(void)
{
    bank_state_t before;

    bank_init(&g_state, HOME_SLOT);
    tick(HOME_SLOT, 700U);
    tick(3U, 700U);
    before = g_state;
    for (unsigned int i = 0U; i < 8U; i++)
    {
        tick(3U, 700U);
    }
    TEST_ASSERT_EQUAL(0, memcmp(&before, &g_state, sizeof(g_state)));
}



static void test_out_of_range_controls_wrap_or_stop_at_the_top(void)
{
    bank_init(&g_state, HOME_SLOT);
    tick(HOME_SLOT, 5000U);
    TEST_ASSERT_EQUAL(1023, bank_amount(&g_state, HOME_SLOT));

    // Selector position 9 is slot 1, and so is slot 9 when asked for
    tick(9U, 1023U);
    TEST_ASSERT_EQUAL(1, g_state.selected);
    tick(9U, 0U);
    tick(9U, 250U);
    TEST_ASSERT_EQUAL(250, bank_amount(&g_state, 1U));
    TEST_ASSERT_EQUAL(250, bank_amount(&g_state, 9U));
    TEST_ASSERT_EQUAL(1023, bank_amount(&g_state, BANK_SLOT_COUNT));
}



static void test_null_pointers_are_ignored(void)
{
    bank_state_t  untouched;
    bank_inputs_t in;

    bank_init(NULL, HOME_SLOT); // Must not crash

    bank_init(&g_state, HOME_SLOT);
    untouched          = g_state;
    in.selected        = 3U;
    in.reading         = 700U;
    in.b_reading_valid = true;
    bank_tick(NULL, &in);
    bank_tick(&g_state, NULL);
    TEST_ASSERT_EQUAL(0, memcmp(&untouched, &g_state, sizeof(g_state)));
    TEST_ASSERT_EQUAL(0, bank_amount(NULL, 0U));
}



int main(void)
{
    RUN_TEST(test_init_empties_the_bank);
    RUN_TEST(test_the_home_slot_takes_the_first_reading_wherever_the_selector_is);
    RUN_TEST(test_a_selected_home_slot_follows_the_pot_from_the_start);
    RUN_TEST(test_a_reading_that_is_not_valid_is_not_the_first);
    RUN_TEST(test_with_no_home_slot_every_amount_waits_for_the_pot);
    RUN_TEST(test_another_slot_has_to_be_met_before_the_pot_sets_it);
    RUN_TEST(test_passing_through_a_slot_changes_nothing);
    RUN_TEST(test_coming_back_to_a_slot_the_pot_has_left_waits_too);
    RUN_TEST(test_the_same_inputs_again_change_nothing);
    RUN_TEST(test_out_of_range_controls_wrap_or_stop_at_the_top);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
