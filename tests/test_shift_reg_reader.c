/**
 * @file   test_shift_reg_reader.c
 * @brief  Host tests for the target step input (adapters/target/shift_reg_reader.c),
 *         compiled against fake registers. The fake models the 74HC165 chain:
 *         it only gives up its bits if the adapter pulses the load line and
 *         then clocks in the right order.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <string.h>
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "shift_reg_reader.c"     // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define POISON_BYTE     (0xA5U) // Marks buffer bytes the adapter must not write
#define OTHER_PORTD_BITS ((uint8_t)~(FAKE_SHIFT_LOAD_MASK | FAKE_SHIFT_CLK_MASK))

static const uint8_t g_pattern[FAKE_SHIFT_CHAIN_BYTES] =
{
    0x01U, 0x23U, 0x45U, 0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU
};

/**
 * @brief Fake registers at power-on state, with the chain's inputs set.
 */
static void start_chain(const uint8_t *p_inputs)
{
    fake_regs_reset();
    (void)memcpy(g_fake_shift_inputs, p_inputs, sizeof(g_fake_shift_inputs));
}



static void test_full_chain_is_read_nearest_chip_first(void)
{
    uint8_t raw[SHIFT_REG_CHAIN_BYTES];

    start_chain(g_pattern);
    (void)memset(raw, POISON_BYTE, sizeof(raw));

    TEST_ASSERT_EQUAL(STATUS_OK, step_input_read(raw, SHIFT_REG_CHAIN_BYTES));
    TEST_ASSERT_EQUAL(0, memcmp(g_pattern, raw, sizeof(raw)));
}



static void test_read_pulses_load_once_and_clocks_every_bit(void)
{
    uint8_t raw[SHIFT_REG_CHAIN_BYTES];

    start_chain(g_pattern);
    (void)step_input_read(raw, SHIFT_REG_CHAIN_BYTES);
    fake_shift_sync();

    TEST_ASSERT_EQUAL(1, g_fake_shift_loads);
    TEST_ASSERT_EQUAL(SHIFT_REG_CHAIN_BYTES * BITS_PER_BYTE, g_fake_shift_clocks);
}



static void test_lines_are_left_idle_and_other_pins_untouched(void)
{
    uint8_t raw[SHIFT_REG_CHAIN_BYTES];

    // Once with every other PORTD bit clear and once with them all set
    for (uint8_t pass = 0U; pass < 2U; pass++)
    {
        const uint8_t others = (0U == pass) ? 0U : OTHER_PORTD_BITS;

        start_chain(g_pattern);
        g_fake_portd = others;
        (void)step_input_read(raw, SHIFT_REG_CHAIN_BYTES);

        TEST_ASSERT_EQUAL(others, g_fake_portd & OTHER_PORTD_BITS);
        TEST_ASSERT(0U != (g_fake_portd & FAKE_SHIFT_LOAD_MASK)); // Load high: shift mode
        TEST_ASSERT(0U == (g_fake_portd & FAKE_SHIFT_CLK_MASK));  // Clock low
        TEST_ASSERT_EQUAL(0, memcmp(g_pattern, raw, sizeof(raw)));
    }
}



static void test_short_read_takes_the_nearest_chips_only(void)
{
    for (uint8_t byte_count = 1U; byte_count < SHIFT_REG_CHAIN_BYTES; byte_count++)
    {
        uint8_t raw[SHIFT_REG_CHAIN_BYTES];

        start_chain(g_pattern);
        (void)memset(raw, POISON_BYTE, sizeof(raw));

        TEST_ASSERT_EQUAL(STATUS_OK, step_input_read(raw, byte_count));
        fake_shift_sync();

        TEST_ASSERT_EQUAL(0, memcmp(g_pattern, raw, byte_count));
        TEST_ASSERT_EQUAL(POISON_BYTE, raw[byte_count]); // Nothing written past the request
        TEST_ASSERT_EQUAL(byte_count * BITS_PER_BYTE, g_fake_shift_clocks);
    }
}



static void test_each_bit_lands_in_its_own_place(void)
{
    // One input bit high at a time; exactly that bit must come back
    for (uint16_t position = 0U; position < FAKE_SHIFT_CHAIN_BITS; position++)
    {
        uint8_t inputs[FAKE_SHIFT_CHAIN_BYTES] = { 0U };
        uint8_t raw[SHIFT_REG_CHAIN_BYTES];

        inputs[position / 8U] = (uint8_t)(0x80U >> (position % 8U));
        start_chain(inputs);

        TEST_ASSERT_EQUAL(STATUS_OK, step_input_read(raw, SHIFT_REG_CHAIN_BYTES));
        TEST_ASSERT_EQUAL(0, memcmp(inputs, raw, sizeof(raw)));
    }
}



static void test_only_the_data_pin_is_sampled(void)
{
    static const uint8_t zeros[FAKE_SHIFT_CHAIN_BYTES] = { 0U };
    uint8_t              raw[SHIFT_REG_CHAIN_BYTES];

    start_chain(zeros);
    g_fake_pind_other_bits = 0xFFU; // Every other port D pin reads high
    (void)memset(raw, POISON_BYTE, sizeof(raw));

    TEST_ASSERT_EQUAL(STATUS_OK, step_input_read(raw, SHIFT_REG_CHAIN_BYTES));
    TEST_ASSERT_EQUAL(0, memcmp(zeros, raw, sizeof(raw)));
}



static void test_every_read_latches_the_inputs_again(void)
{
    static const uint8_t changed[FAKE_SHIFT_CHAIN_BYTES] =
    {
        0xFFU, 0x00U, 0x5AU, 0xA5U, 0x0FU, 0xF0U, 0x3CU, 0xC3U
    };
    uint8_t raw[SHIFT_REG_CHAIN_BYTES];

    start_chain(g_pattern);
    (void)step_input_read(raw, SHIFT_REG_CHAIN_BYTES);
    TEST_ASSERT_EQUAL(0, memcmp(g_pattern, raw, sizeof(raw)));

    // The switches move between ticks; the next read must see the new settings
    (void)memcpy(g_fake_shift_inputs, changed, sizeof(g_fake_shift_inputs));
    (void)step_input_read(raw, SHIFT_REG_CHAIN_BYTES);
    fake_shift_sync();

    TEST_ASSERT_EQUAL(0, memcmp(changed, raw, sizeof(raw)));
    TEST_ASSERT_EQUAL(2, g_fake_shift_loads);
}



static void test_invalid_parameters_are_rejected_without_touching_the_pins(void)
{
    uint8_t raw[SHIFT_REG_CHAIN_BYTES + 1U];

    start_chain(g_pattern);
    g_fake_portd = FAKE_SHIFT_LOAD_MASK; // Idle state after an earlier read
    fake_shift_sync();
    g_fake_shift_loads = 0U;
    (void)memset(raw, POISON_BYTE, sizeof(raw));

    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, step_input_read(NULL, SHIFT_REG_CHAIN_BYTES));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, step_input_read(raw, 0U));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, step_input_read(raw, (uint8_t)(SHIFT_REG_CHAIN_BYTES + 1U)));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, step_input_read(raw, UINT8_MAX));
    fake_shift_sync();

    TEST_ASSERT_EQUAL(FAKE_SHIFT_LOAD_MASK, g_fake_portd);
    TEST_ASSERT_EQUAL(0, g_fake_shift_loads);
    TEST_ASSERT_EQUAL(0, g_fake_shift_clocks);
    for (size_t i = 0U; i < sizeof(raw); i++)
    {
        TEST_ASSERT_EQUAL(POISON_BYTE, raw[i]);
    }
}



int main(void)
{
    RUN_TEST(test_full_chain_is_read_nearest_chip_first);
    RUN_TEST(test_read_pulses_load_once_and_clocks_every_bit);
    RUN_TEST(test_lines_are_left_idle_and_other_pins_untouched);
    RUN_TEST(test_short_read_takes_the_nearest_chips_only);
    RUN_TEST(test_each_bit_lands_in_its_own_place);
    RUN_TEST(test_only_the_data_pin_is_sampled);
    RUN_TEST(test_every_read_latches_the_inputs_again);
    RUN_TEST(test_invalid_parameters_are_rejected_without_touching_the_pins);
    return TEST_RESULT();
}
