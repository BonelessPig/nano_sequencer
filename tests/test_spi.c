/**
 * @file   test_spi.c
 * @brief  Host tests for the target SPI driver (adapters/target/spi.c),
 *         compiled against fake registers. The fake SPI completes a transfer
 *         after a set number of polls, and only if it has been enabled as
 *         master, so both the normal path and the timeout can be driven.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "spi.c"                  // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define SS_MASK         (0x04U) // PB2
#define SCK_MASK        (0x20U) // PB5
#define MOSI_MISO_MASK  (0x18U) // PB3 and PB4
#define SPCR_MASTER_1MHZ (0x51U) // SPE, MSTR, SPR0: mode 0, MSB first, clock / 16
#define UNTOUCHED_BYTE  (0xA5U) // Marks a result the driver must not write

/**
 * @brief Fake registers at power-on state, then the driver brought up.
 */
static void start_spi(void)
{
    fake_regs_reset();
    spi_init();
}



static void test_init_enables_master_mode_0_at_one_megahertz(void)
{
    fake_regs_reset();
    g_fake_spcr = 0xAEU; // Stale bits a bootloader might leave must not survive
    g_fake_spsr = 0x01U; // Double speed
    spi_init();

    TEST_ASSERT_EQUAL(SPCR_MASTER_1MHZ, g_fake_spcr);
    TEST_ASSERT_EQUAL(0, g_fake_spsr);
}



static void test_init_makes_ss_and_sck_outputs_and_leaves_the_data_pins(void)
{
    fake_regs_reset();
    spi_init();
    TEST_ASSERT_EQUAL(SS_MASK | SCK_MASK, g_fake_ddrb);
    TEST_ASSERT_EQUAL(SS_MASK, g_fake_portb); // SS high, clock low

    // Other pins of the port keep their direction and level
    fake_regs_reset();
    g_fake_ddrb  = 0x02U;
    g_fake_portb = 0x02U;
    spi_init();
    TEST_ASSERT_EQUAL(0x02U | SS_MASK | SCK_MASK, g_fake_ddrb);
    TEST_ASSERT_EQUAL(0x02U | SS_MASK, g_fake_portb);
    TEST_ASSERT_EQUAL(0, g_fake_ddrb & MOSI_MISO_MASK);
}



static void test_init_sets_ss_high_then_output_then_selects_master(void)
{
    fake_regs_reset();
    spi_init();

    // High before it was an output: never driven low
    TEST_ASSERT(0U != (g_fake_portb_at_first_ddrb & SS_MASK));
    // And an output before master mode: a low level could not cancel it
    TEST_ASSERT(0U != (g_fake_ddrb_at_spcr & SS_MASK));
    TEST_ASSERT(0U != (g_fake_portb_at_spcr & SS_MASK));
}



static void test_transfer_sends_the_byte_and_returns_what_came_back(void)
{
    static const uint8_t sent[]     = { 0x00U, 0xFFU, 0x5AU, 0x81U };
    static const uint8_t received[] = { 0xC3U, 0x00U, 0xFFU, 0x7EU };

    start_spi();
    for (size_t i = 0U; i < sizeof(sent); i++)
    {
        g_fake_spi_rx[i] = received[i];
    }

    for (size_t i = 0U; i < sizeof(sent); i++)
    {
        uint8_t rx = UNTOUCHED_BYTE;

        TEST_ASSERT_EQUAL(STATUS_OK, spi_transfer(sent[i], &rx));
        TEST_ASSERT_EQUAL(received[i], rx);
        TEST_ASSERT_EQUAL(sent[i], g_fake_spi_tx[i]);
    }
    TEST_ASSERT_EQUAL(sizeof(sent), g_fake_spi_transfers);
}



static void test_transfer_leaves_the_complete_flag_clear(void)
{
    uint8_t rx = UNTOUCHED_BYTE;

    start_spi();
    (void)spi_transfer(0x12U, &rx);

    // The data register was read after the flag was seen, which clears it
    TEST_ASSERT_EQUAL(0, g_fake_spsr & (1U << SPIF));
}



static void test_transfer_waits_for_a_slow_one(void)
{
    uint8_t rx = UNTOUCHED_BYTE;

    start_spi();
    g_fake_spi_rx[0]      = 0x3CU;
    g_fake_spi_busy_polls = SPI_TIMEOUT_LOOPS - 1U; // Done on the last poll allowed

    TEST_ASSERT_EQUAL(STATUS_OK, spi_transfer(0x00U, &rx));
    TEST_ASSERT_EQUAL(0x3C, rx);
}



static void test_transfer_times_out_when_the_poll_budget_is_spent(void)
{
    uint8_t rx = UNTOUCHED_BYTE;

    start_spi();
    g_fake_spi_rx[0]      = 0x3CU;
    g_fake_spi_busy_polls = SPI_TIMEOUT_LOOPS; // One poll more than is allowed

    TEST_ASSERT_EQUAL(ERR_TIMEOUT, spi_transfer(0x00U, &rx));
    TEST_ASSERT_EQUAL(UNTOUCHED_BYTE, rx);
    TEST_ASSERT_EQUAL(0, g_fake_spi_transfers);
}



static void test_transfer_times_out_if_the_spi_was_never_enabled(void)
{
    uint8_t rx = UNTOUCHED_BYTE;

    fake_regs_reset(); // No spi_init()

    TEST_ASSERT_EQUAL(ERR_TIMEOUT, spi_transfer(0x00U, &rx));
    TEST_ASSERT_EQUAL(UNTOUCHED_BYTE, rx);
}



static void test_null_result_pointer_is_rejected_and_nothing_is_sent(void)
{
    start_spi();

    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, spi_transfer(0x77U, NULL));
    TEST_ASSERT(!g_fake_spi_b_pending);
    TEST_ASSERT_EQUAL(0, g_fake_spi_transfers);
}



int main(void)
{
    RUN_TEST(test_init_enables_master_mode_0_at_one_megahertz);
    RUN_TEST(test_init_makes_ss_and_sck_outputs_and_leaves_the_data_pins);
    RUN_TEST(test_init_sets_ss_high_then_output_then_selects_master);
    RUN_TEST(test_transfer_sends_the_byte_and_returns_what_came_back);
    RUN_TEST(test_transfer_leaves_the_complete_flag_clear);
    RUN_TEST(test_transfer_waits_for_a_slow_one);
    RUN_TEST(test_transfer_times_out_when_the_poll_budget_is_spent);
    RUN_TEST(test_transfer_times_out_if_the_spi_was_never_enabled);
    RUN_TEST(test_null_result_pointer_is_rejected_and_nothing_is_sent);
    return TEST_RESULT();
}
