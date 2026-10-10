/**
 * @file   test_shift_reg_reader.c
 * @brief  Host tests for the target step input (adapters/target/shift_reg_reader.c),
 *         compiled against fake registers. The SPI driver it calls is a stub
 *         here that clocks the fake's model of the 74HC165 chain, eight bits a
 *         transfer. The model only gives up its bits if the adapter pulses the
 *         load line first and leaves it high while it reads.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <string.h>
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "shift_reg_reader.c"     // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define POISON_BYTE      (0xA5U) // Marks buffer bytes the adapter must not write
#define OTHER_PORTB_BITS ((uint8_t)~FAKE_SHIFT_LOAD_MASK)
#define BITS_PER_BYTE    (8U)
#define NEVER            (0xFFFFU) // No transfer fails

static const uint8_t g_pattern[FAKE_SHIFT_CHAIN_BYTES] =
{
    0x01U, 0x23U, 0x45U, 0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU
};

static uint16_t g_transfer_count   = 0U;    // Calls to the spi_transfer stub
static uint16_t g_failing_transfer = NEVER; // Which call fails, counting from 0
static uint8_t  g_last_tx          = 0U;    // Byte the adapter last sent
static uint16_t g_loads_at_first_transfer = 0U; // Load pulses seen before any clock

/**
 * @brief Stands in for the SPI driver: eight clocks of the chain model, or a
 *        timeout on the call the test chose.
 */
port_status_t spi_transfer(uint8_t tx, uint8_t *p_rx)
{
    port_status_t status = STATUS_OK;

    if (0U == g_transfer_count)
    {
        fake_shift_sync();
        g_loads_at_first_transfer = g_fake_shift_loads;
    }
    g_last_tx = tx;

    if (g_transfer_count == g_failing_transfer)
    {
        status = ERR_TIMEOUT;
    }
    else
    {
        *p_rx = fake_shift_clock_byte();
    }
    g_transfer_count++;

    return status;
}



/**
 * @brief Fake registers at power-on state but with the load line idle high,
 *        as register_init() leaves it, and the chain's inputs set.
 */
static void start_chain(const uint8_t *p_inputs)
{
    fake_regs_reset();
    g_fake_portb = FAKE_SHIFT_LOAD_MASK;
    fake_shift_sync();
    g_fake_shift_loads = 0U;
    (void)memcpy(g_fake_shift_inputs, p_inputs, sizeof(g_fake_shift_inputs));

    g_transfer_count   = 0U;
    g_failing_transfer = NEVER;
    g_last_tx          = POISON_BYTE;
    g_loads_at_first_transfer = 0U;
}



static void test_full_chain_is_read_nearest_chip_first(void)
{
    uint8_t raw[SHIFT_REG_CHAIN_BYTES];

    start_chain(g_pattern);
    (void)memset(raw, POISON_BYTE, sizeof(raw));

    TEST_ASSERT_EQUAL(STATUS_OK, step_input_read(raw, SHIFT_REG_CHAIN_BYTES));
    TEST_ASSERT_EQUAL(0, memcmp(g_pattern, raw, sizeof(raw)));
}



static void test_read_pulses_load_once_then_makes_one_transfer_a_byte(void)
{
    uint8_t raw[SHIFT_REG_CHAIN_BYTES];

    start_chain(g_pattern);
    (void)step_input_read(raw, SHIFT_REG_CHAIN_BYTES);
    fake_shift_sync();

    TEST_ASSERT_EQUAL(1, g_fake_shift_loads);
    TEST_ASSERT_EQUAL(1, g_loads_at_first_transfer); // Loaded before the first clock
    TEST_ASSERT_EQUAL(SHIFT_REG_CHAIN_BYTES, g_transfer_count);
    TEST_ASSERT_EQUAL(SHIFT_REG_CHAIN_BYTES * BITS_PER_BYTE, g_fake_shift_clocks);
    TEST_ASSERT_EQUAL(0, g_last_tx); // Nothing listens; zeros are sent
}



static void test_load_is_left_idle_and_other_pins_untouched(void)
{
    uint8_t raw[SHIFT_REG_CHAIN_BYTES];

    // Once with every other PORTB bit clear and once with them all set
    for (uint8_t pass = 0U; pass < 2U; pass++)
    {
        const uint8_t others = (0U == pass) ? 0U : OTHER_PORTB_BITS;

        start_chain(g_pattern);
        g_fake_portb = (uint8_t)(others | FAKE_SHIFT_LOAD_MASK);
        (void)step_input_read(raw, SHIFT_REG_CHAIN_BYTES);

        TEST_ASSERT_EQUAL(others, g_fake_portb & OTHER_PORTB_BITS);
        TEST_ASSERT(0U != (g_fake_portb & FAKE_SHIFT_LOAD_MASK)); // Load high: shift mode
        TEST_ASSERT_EQUAL(0, memcmp(g_pattern, raw, sizeof(raw)));
    }
}



static void test_port_d_is_not_touched(void)
{
    uint8_t raw[SHIFT_REG_CHAIN_BYTES];

    // The chain used to be on PD2, PD3 and PD4; those pins are free now
    start_chain(g_pattern);
    g_fake_portd = 0x5AU;
    g_fake_ddrd  = 0xA5U;
    (void)step_input_read(raw, SHIFT_REG_CHAIN_BYTES);

    TEST_ASSERT_EQUAL(0x5A, g_fake_portd);
    TEST_ASSERT_EQUAL(0xA5, g_fake_ddrd);
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
        TEST_ASSERT_EQUAL(byte_count, g_transfer_count);
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



static void test_failed_transfer_is_returned_and_the_buffer_left_alone(void)
{
    // Whichever transfer fails, first to last
    for (uint16_t failing = 0U; failing < SHIFT_REG_CHAIN_BYTES; failing++)
    {
        uint8_t raw[SHIFT_REG_CHAIN_BYTES];

        start_chain(g_pattern);
        g_failing_transfer = failing;
        (void)memset(raw, POISON_BYTE, sizeof(raw));

        TEST_ASSERT_EQUAL(ERR_TIMEOUT, step_input_read(raw, SHIFT_REG_CHAIN_BYTES));
        TEST_ASSERT_EQUAL(failing + 1U, g_transfer_count); // It stops there
        for (size_t i = 0U; i < sizeof(raw); i++)
        {
            TEST_ASSERT_EQUAL(POISON_BYTE, raw[i]);
        }
        TEST_ASSERT(0U != (g_fake_portb & FAKE_SHIFT_LOAD_MASK)); // Load still idle
    }
}



static void test_invalid_parameters_are_rejected_without_touching_the_bus(void)
{
    uint8_t raw[SHIFT_REG_CHAIN_BYTES + 1U];

    start_chain(g_pattern);
    (void)memset(raw, POISON_BYTE, sizeof(raw));

    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, step_input_read(NULL, SHIFT_REG_CHAIN_BYTES));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, step_input_read(raw, 0U));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM,
                      step_input_read(raw, (uint8_t)(SHIFT_REG_CHAIN_BYTES + 1U)));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, step_input_read(raw, UINT8_MAX));
    fake_shift_sync();

    TEST_ASSERT_EQUAL(FAKE_SHIFT_LOAD_MASK, g_fake_portb);
    TEST_ASSERT_EQUAL(0, g_fake_shift_loads);
    TEST_ASSERT_EQUAL(0, g_transfer_count);
    for (size_t i = 0U; i < sizeof(raw); i++)
    {
        TEST_ASSERT_EQUAL(POISON_BYTE, raw[i]);
    }
}



int main(void)
{
    RUN_TEST(test_full_chain_is_read_nearest_chip_first);
    RUN_TEST(test_read_pulses_load_once_then_makes_one_transfer_a_byte);
    RUN_TEST(test_load_is_left_idle_and_other_pins_untouched);
    RUN_TEST(test_port_d_is_not_touched);
    RUN_TEST(test_short_read_takes_the_nearest_chips_only);
    RUN_TEST(test_each_bit_lands_in_its_own_place);
    RUN_TEST(test_every_read_latches_the_inputs_again);
    RUN_TEST(test_failed_transfer_is_returned_and_the_buffer_left_alone);
    RUN_TEST(test_invalid_parameters_are_rejected_without_touching_the_bus);
    return TEST_RESULT();
}
