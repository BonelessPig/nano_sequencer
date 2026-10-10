/**
 * @file   test_cv_output.c
 * @brief  Host tests for the target pitch CV output
 *         (adapters/target/cv_output.c), compiled against fake registers. The
 *         SPI driver it calls is a stub here that records each byte sent and
 *         whether the DAC's chip select was low at the time.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "cv_output.c"            // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define SELECT_MASK      (0x04U) // PB2: chip select, low while a frame is sent
#define OTHER_PORTB_BITS ((uint8_t)~SELECT_MASK)
#define FRAME_BYTES      (2U)
#define MAX_TRANSFERS    (4U)      // More than any one write makes
#define NEVER            (0xFFFFU) // No transfer fails
#define CONFIG_CHANNEL_A_GAIN_2_ON (0x1000U) // What the frame carries above the value

static uint8_t  g_tx[MAX_TRANSFERS];         // Bytes the adapter sent, in order
static bool     g_b_selected[MAX_TRANSFERS]; // Whether chip select was low for each
static uint16_t g_transfer_count   = 0U;     // Calls to the spi_transfer stub
static uint16_t g_failing_transfer = NEVER;  // Which call fails, counting from 0

/**
 * @brief Stands in for the SPI driver: notes the byte and the state of chip
 *        select, and times out on the call the test chose.
 */
port_status_t spi_transfer(uint8_t tx, uint8_t *p_rx)
{
    port_status_t status = STATUS_OK;

    if (g_transfer_count < MAX_TRANSFERS)
    {
        g_tx[g_transfer_count]         = tx;
        g_b_selected[g_transfer_count] = (0U == (g_fake_portb & SELECT_MASK));
    }
    if (g_transfer_count == g_failing_transfer)
    {
        status = ERR_TIMEOUT;
    }
    else
    {
        *p_rx = 0xFFU; // An undriven data line; the adapter must not care
    }
    g_transfer_count++;

    return status;
}



/**
 * @brief Fake registers at power-on state but with chip select idle high, as
 *        spi_init() leaves it, and nothing sent yet.
 */
static void start_dac(void)
{
    fake_regs_reset();
    g_fake_portb = SELECT_MASK;
    g_fake_ddrb  = SELECT_MASK;

    g_transfer_count   = 0U;
    g_failing_transfer = NEVER;
    for (size_t i = 0U; i < MAX_TRANSFERS; i++)
    {
        g_tx[i]         = 0U;
        g_b_selected[i] = false;
    }
}



/**
 * @brief The 16 bits of the frame the adapter sent, first byte high.
 */
static uint16_t frame_sent(void)
{
    return (uint16_t)(((uint16_t)g_tx[0] << 8U) | g_tx[1]);
}



static void test_a_voltage_is_sent_as_one_frame_for_channel_a_at_gain_2(void)
{
    static const uint16_t millivolts[] =
    {
        0U, 1U, 83U, 255U, 256U, 1000U, 2048U, 3750U, 0x0A5AU, 4095U
    };

    for (size_t i = 0U; i < (sizeof(millivolts) / sizeof(millivolts[0])); i++)
    {
        start_dac();

        TEST_ASSERT_EQUAL(STATUS_OK, cv_output_write(millivolts[i]));
        TEST_ASSERT_EQUAL(FRAME_BYTES, g_transfer_count);
        TEST_ASSERT_EQUAL(CONFIG_CHANNEL_A_GAIN_2_ON | millivolts[i], frame_sent());
    }
}



static void test_chip_select_is_low_for_the_frame_and_high_after(void)
{
    start_dac();
    (void)cv_output_write(1234U);

    TEST_ASSERT(g_b_selected[0]);
    TEST_ASSERT(g_b_selected[1]);
    TEST_ASSERT(0U != (g_fake_portb & SELECT_MASK)); // Raised: the DAC takes the value
}



static void test_nothing_but_chip_select_is_touched(void)
{
    // Once with every other PORTB bit clear and once with them all set
    for (uint8_t pass = 0U; pass < 2U; pass++)
    {
        const uint8_t others = (0U == pass) ? 0U : OTHER_PORTB_BITS;

        start_dac();
        g_fake_portb = (uint8_t)(others | SELECT_MASK);
        g_fake_portd = 0x5AU;
        g_fake_ddrd  = 0xA5U;
        (void)cv_output_write(2000U);

        TEST_ASSERT_EQUAL(others | SELECT_MASK, g_fake_portb);
        TEST_ASSERT_EQUAL(SELECT_MASK, g_fake_ddrb); // Directions are the driver's job
        TEST_ASSERT_EQUAL(0x5A, g_fake_portd);
        TEST_ASSERT_EQUAL(0xA5, g_fake_ddrd);
    }
}



static void test_every_write_is_a_frame_of_its_own(void)
{
    start_dac();
    TEST_ASSERT_EQUAL(STATUS_OK, cv_output_write(500U));
    TEST_ASSERT_EQUAL(STATUS_OK, cv_output_write(500U)); // The same again is still sent

    TEST_ASSERT_EQUAL(2U * FRAME_BYTES, g_transfer_count);
    TEST_ASSERT_EQUAL(g_tx[0], g_tx[2]);
    TEST_ASSERT_EQUAL(g_tx[1], g_tx[3]);
    TEST_ASSERT(g_b_selected[2]); // Selected again for the second frame
}



static void test_a_failed_first_byte_ends_the_frame_there(void)
{
    start_dac();
    g_failing_transfer = 0U;

    TEST_ASSERT_EQUAL(ERR_TIMEOUT, cv_output_write(1000U));
    TEST_ASSERT_EQUAL(1, g_transfer_count); // The second byte is not sent
    // Deselected all the same: the DAC drops a frame that is cut short
    TEST_ASSERT(0U != (g_fake_portb & SELECT_MASK));
}



static void test_a_failed_second_byte_is_returned_and_chip_select_released(void)
{
    start_dac();
    g_failing_transfer = 1U;

    TEST_ASSERT_EQUAL(ERR_TIMEOUT, cv_output_write(1000U));
    TEST_ASSERT_EQUAL(FRAME_BYTES, g_transfer_count);
    TEST_ASSERT(0U != (g_fake_portb & SELECT_MASK));
}



static void test_a_voltage_above_the_range_is_rejected_without_touching_the_bus(void)
{
    start_dac();

    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM,
                      cv_output_write((uint16_t)(CV_OUTPUT_MILLIVOLTS_MAX + 1U)));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, cv_output_write(UINT16_MAX));
    TEST_ASSERT_EQUAL(0, g_transfer_count);
    TEST_ASSERT_EQUAL(SELECT_MASK, g_fake_portb); // Never selected
}



int main(void)
{
    RUN_TEST(test_a_voltage_is_sent_as_one_frame_for_channel_a_at_gain_2);
    RUN_TEST(test_chip_select_is_low_for_the_frame_and_high_after);
    RUN_TEST(test_nothing_but_chip_select_is_touched);
    RUN_TEST(test_every_write_is_a_frame_of_its_own);
    RUN_TEST(test_a_failed_first_byte_ends_the_frame_there);
    RUN_TEST(test_a_failed_second_byte_is_returned_and_chip_select_released);
    RUN_TEST(test_a_voltage_above_the_range_is_rejected_without_touching_the_bus);
    return TEST_RESULT();
}
