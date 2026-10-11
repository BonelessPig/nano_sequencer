/**
 * @file   test_eeprom.c
 * @brief  Host tests for the target non-volatile storage adapter
 *         (adapters/target/eeprom.c), compiled against fake registers. The
 *         fake EEPROM stores a byte only if the write was armed first, and
 *         then shows the write as in progress for a set number of polls, so
 *         the tests can see that nothing waits and nothing is started early.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <string.h>
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "eeprom.c"               // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define EEPROM_BYTES    (1024U)
#define ERASED          (0xFFU)
#define UNTOUCHED_BYTE  (0xA5U) // Marks a result the adapter must not write
#define INTERRUPTS_ON   (0x80U) // The global interrupt enable bit of SREG
#define OTHER_SREG_BITS (0x23U) // Flags that must come through unchanged
#define MANY_POLLS      (200U)

/**
 * @brief Fake registers at power-on state and no write in hand. The adapter's
 *        own state is static, so a write left over from an earlier test is
 *        polled through first.
 */
static void start_eeprom(void)
{
    fake_regs_reset();
    for (unsigned int i = 0U; i < MANY_POLLS; i++)
    {
        nv_poll();
    }
    fake_regs_reset();
    g_fake_sreg = INTERRUPTS_ON; // As the main loop runs
}

/**
 * @brief Polls until a write of this many bytes must have been dealt with.
 */
static void poll_times(unsigned int count)
{
    for (unsigned int i = 0U; i < count; i++)
    {
        nv_poll();
    }
    fake_eeprom_sync();
}



static void test_read_returns_the_bytes_at_the_address(void)
{
    uint8_t data[4] = { UNTOUCHED_BYTE, UNTOUCHED_BYTE, UNTOUCHED_BYTE, UNTOUCHED_BYTE };

    start_eeprom();
    g_fake_eeprom[0x123] = 0x11U;
    g_fake_eeprom[0x124] = 0x22U;
    g_fake_eeprom[0x125] = 0x33U;

    TEST_ASSERT_EQUAL(STATUS_OK, nv_read(0x123U, data, 3U));
    TEST_ASSERT_EQUAL(0x11, data[0]);
    TEST_ASSERT_EQUAL(0x22, data[1]);
    TEST_ASSERT_EQUAL(0x33, data[2]);
    TEST_ASSERT_EQUAL(UNTOUCHED_BYTE, data[3]); // No more than was asked for
    TEST_ASSERT_EQUAL(3, g_fake_eeprom_reads);
    TEST_ASSERT_EQUAL(0, g_fake_eeprom_writes);

    // Both halves of the address were set: the last byte read was at 0x125
    TEST_ASSERT_EQUAL(0x01, g_fake_eearh);
    TEST_ASSERT_EQUAL(0x25, g_fake_eearl);
}



static void test_read_reaches_the_first_and_last_bytes(void)
{
    uint8_t data = UNTOUCHED_BYTE;

    start_eeprom();
    g_fake_eeprom[0]                 = 0x5AU;
    g_fake_eeprom[EEPROM_BYTES - 1U] = 0xC3U;

    TEST_ASSERT_EQUAL(STATUS_OK, nv_read(0U, &data, 1U));
    TEST_ASSERT_EQUAL(0x5A, data);
    TEST_ASSERT_EQUAL(STATUS_OK, nv_read((uint16_t)(EEPROM_BYTES - 1U), &data, 1U));
    TEST_ASSERT_EQUAL(0xC3, data);
}



static void test_an_erased_eeprom_reads_as_all_ones(void)
{
    uint8_t data[12];

    start_eeprom();
    (void)memset(data, 0, sizeof(data));
    TEST_ASSERT_EQUAL(STATUS_OK, nv_read(0U, data, (uint8_t)sizeof(data)));
    for (size_t i = 0U; i < sizeof(data); i++)
    {
        TEST_ASSERT_EQUAL(ERASED, data[i]);
    }
}



static void test_read_rejects_bad_parameters_and_touches_nothing(void)
{
    uint8_t data[2] = { UNTOUCHED_BYTE, UNTOUCHED_BYTE };

    start_eeprom();
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, nv_read(0U, NULL, 1U));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, nv_read(0U, data, 0U));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, nv_read((uint16_t)EEPROM_BYTES, data, 1U));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, nv_read((uint16_t)(EEPROM_BYTES - 1U), data, 2U));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, nv_read(UINT16_MAX, data, 2U));

    TEST_ASSERT_EQUAL(UNTOUCHED_BYTE, data[0]);
    TEST_ASSERT_EQUAL(0, g_fake_eeprom_reads);

    // The last two bytes are still a valid run
    TEST_ASSERT_EQUAL(STATUS_OK, nv_read((uint16_t)(EEPROM_BYTES - 2U), data, 2U));
}



static void test_write_does_nothing_until_polled_then_one_byte_per_poll(void)
{
    static const uint8_t data[3] = { 0x12U, 0x34U, 0x56U };

    start_eeprom();
    TEST_ASSERT_EQUAL(STATUS_OK, nv_write(0x200U, data, 3U));
    fake_eeprom_sync();
    TEST_ASSERT_EQUAL(0, g_fake_eeprom_writes);
    TEST_ASSERT_EQUAL(0, g_fake_eeprom_reads); // The registers were not touched

    poll_times(1U);
    TEST_ASSERT_EQUAL(1, g_fake_eeprom_writes);
    TEST_ASSERT_EQUAL(0x12, g_fake_eeprom[0x200]);
    TEST_ASSERT_EQUAL(ERASED, g_fake_eeprom[0x201]);

    poll_times(1U);
    TEST_ASSERT_EQUAL(2, g_fake_eeprom_writes);
    TEST_ASSERT_EQUAL(0x34, g_fake_eeprom[0x201]);

    poll_times(1U);
    TEST_ASSERT_EQUAL(3, g_fake_eeprom_writes);
    TEST_ASSERT_EQUAL(0x56, g_fake_eeprom[0x202]);

    // And then it is finished: nothing more is written anywhere
    poll_times(MANY_POLLS);
    TEST_ASSERT_EQUAL(3, g_fake_eeprom_writes);
    TEST_ASSERT_EQUAL(ERASED, g_fake_eeprom[0x1FF]);
    TEST_ASSERT_EQUAL(ERASED, g_fake_eeprom[0x203]);
}



static void test_write_keeps_its_own_copy_of_the_bytes(void)
{
    uint8_t data[2] = { 0x12U, 0x34U };

    start_eeprom();
    TEST_ASSERT_EQUAL(STATUS_OK, nv_write(8U, data, 2U));
    data[0] = 0x00U; // The caller reuses its buffer straight away
    data[1] = 0x00U;
    poll_times(2U);

    TEST_ASSERT_EQUAL(0x12, g_fake_eeprom[8]);
    TEST_ASSERT_EQUAL(0x34, g_fake_eeprom[9]);
}



static void test_each_byte_is_armed_then_started_with_interrupts_off(void)
{
    static const uint8_t data[2] = { 0x12U, 0x34U };

    start_eeprom();
    g_fake_sreg = (uint8_t)(INTERRUPTS_ON | OTHER_SREG_BITS);
    TEST_ASSERT_EQUAL(STATUS_OK, nv_write(0U, data, 2U));
    poll_times(2U);

    TEST_ASSERT_EQUAL(2, g_fake_eeprom_writes);
    TEST_ASSERT_EQUAL(0, g_fake_eeprom_unarmed);
    TEST_ASSERT(!g_fake_eeprom_b_unmasked);

    // And the status register is given back exactly as it was found
    TEST_ASSERT_EQUAL(INTERRUPTS_ON | OTHER_SREG_BITS, g_fake_sreg);
}



static void test_interrupts_that_were_off_stay_off(void)
{
    static const uint8_t data[1] = { 0x12U };

    start_eeprom();
    g_fake_sreg = OTHER_SREG_BITS;
    TEST_ASSERT_EQUAL(STATUS_OK, nv_write(0U, data, 1U));
    poll_times(1U);

    TEST_ASSERT_EQUAL(1, g_fake_eeprom_writes);
    TEST_ASSERT_EQUAL(OTHER_SREG_BITS, g_fake_sreg);
}



static void test_poll_waits_for_the_byte_in_progress_without_blocking(void)
{
    static const uint8_t data[2] = { 0x12U, 0x34U };

    start_eeprom();
    g_fake_eeprom_busy_polls = 5U;
    TEST_ASSERT_EQUAL(STATUS_OK, nv_write(0U, data, 2U));

    poll_times(1U); // Starts the first byte
    TEST_ASSERT_EQUAL(1, g_fake_eeprom_writes);

    // Each poll looks once and comes straight back while that byte is written
    for (unsigned int i = 0U; i < 5U; i++)
    {
        poll_times(1U);
        TEST_ASSERT_EQUAL(1, g_fake_eeprom_writes);
        TEST_ASSERT_EQUAL(ERASED, g_fake_eeprom[1]);
    }

    poll_times(1U);
    TEST_ASSERT_EQUAL(2, g_fake_eeprom_writes);
    TEST_ASSERT_EQUAL(0x34, g_fake_eeprom[1]);
}



static void test_a_byte_already_there_is_not_written_again(void)
{
    static const uint8_t data[4] = { 0x12U, 0x34U, 0xFFU, 0x78U };

    start_eeprom();
    g_fake_eeprom[0x10] = 0x12U; // The first is there already; the third is
    g_fake_eeprom[0x11] = 0x99U; // 0xFF over an erased byte
    TEST_ASSERT_EQUAL(STATUS_OK, nv_write(0x10U, data, 4U));

    poll_times(1U);
    TEST_ASSERT_EQUAL(0, g_fake_eeprom_writes); // Passed over, in one poll
    poll_times(1U);
    TEST_ASSERT_EQUAL(1, g_fake_eeprom_writes);
    poll_times(1U);
    TEST_ASSERT_EQUAL(1, g_fake_eeprom_writes);
    poll_times(1U);
    TEST_ASSERT_EQUAL(2, g_fake_eeprom_writes);

    TEST_ASSERT_EQUAL(0x12, g_fake_eeprom[0x10]);
    TEST_ASSERT_EQUAL(0x34, g_fake_eeprom[0x11]);
    TEST_ASSERT_EQUAL(0xFF, g_fake_eeprom[0x12]);
    TEST_ASSERT_EQUAL(0x78, g_fake_eeprom[0x13]);

    // Writing the same again costs no writes at all
    TEST_ASSERT_EQUAL(STATUS_OK, nv_write(0x10U, data, 4U));
    poll_times(4U);
    TEST_ASSERT_EQUAL(2, g_fake_eeprom_writes);
}



static void test_a_second_write_is_refused_until_the_first_has_gone(void)
{
    static const uint8_t first[2]  = { 0x12U, 0x34U };
    static const uint8_t second[2] = { 0xABU, 0xCDU };

    start_eeprom();
    TEST_ASSERT_EQUAL(STATUS_OK, nv_write(0U, first, 2U));
    TEST_ASSERT_EQUAL(ERR_BUSY, nv_write(4U, second, 2U));
    poll_times(1U);
    TEST_ASSERT_EQUAL(ERR_BUSY, nv_write(4U, second, 2U));
    poll_times(1U);

    // The last byte has been started: the next write can be taken
    TEST_ASSERT_EQUAL(STATUS_OK, nv_write(4U, second, 2U));
    poll_times(2U);

    TEST_ASSERT_EQUAL(0x12, g_fake_eeprom[0]);
    TEST_ASSERT_EQUAL(0x34, g_fake_eeprom[1]);
    TEST_ASSERT_EQUAL(ERASED, g_fake_eeprom[2]); // Nothing of the refused ones
    TEST_ASSERT_EQUAL(0xAB, g_fake_eeprom[4]);
    TEST_ASSERT_EQUAL(0xCD, g_fake_eeprom[5]);
}



static void test_read_is_refused_while_a_write_is_being_carried_out(void)
{
    static const uint8_t data[2] = { 0x12U, 0x34U };
    uint8_t              read    = UNTOUCHED_BYTE;

    start_eeprom();
    g_fake_eeprom_busy_polls = 3U;
    TEST_ASSERT_EQUAL(STATUS_OK, nv_write(0U, data, 2U));

    // Bytes still to go
    TEST_ASSERT_EQUAL(ERR_BUSY, nv_read(0U, &read, 1U));
    poll_times(1U);
    TEST_ASSERT_EQUAL(ERR_BUSY, nv_read(0U, &read, 1U));

    // Three polls while the first byte is written, and one that starts the
    // last. Nothing is left to hand over, but the hardware is still writing
    poll_times(4U);
    TEST_ASSERT_EQUAL(2, g_fake_eeprom_writes);
    for (unsigned int i = 0U; i < 3U; i++)
    {
        TEST_ASSERT_EQUAL(ERR_BUSY, nv_read(0U, &read, 1U));
    }
    TEST_ASSERT_EQUAL(UNTOUCHED_BYTE, read);
    TEST_ASSERT_EQUAL(0, g_fake_eeprom_reads - 2U); // One look at each byte, by the write

    // Finished: the read goes through and sees what was written
    TEST_ASSERT_EQUAL(STATUS_OK, nv_read(1U, &read, 1U));
    TEST_ASSERT_EQUAL(0x34, read);
}



static void test_write_rejects_bad_parameters_and_takes_nothing(void)
{
    static const uint8_t data[NV_WRITE_BYTES_MAX + 1U] = { 0x12U };

    start_eeprom();
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, nv_write(0U, NULL, 1U));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, nv_write(0U, data, 0U));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM,
                      nv_write(0U, data, (uint8_t)(NV_WRITE_BYTES_MAX + 1U)));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, nv_write((uint16_t)EEPROM_BYTES, data, 1U));
    TEST_ASSERT_EQUAL(ERR_INVALID_PARAM, nv_write((uint16_t)(EEPROM_BYTES - 1U), data, 2U));

    poll_times(MANY_POLLS);
    TEST_ASSERT_EQUAL(0, g_fake_eeprom_writes);
    TEST_ASSERT_EQUAL(0, g_fake_eeprom_reads);
}



static void test_the_largest_write_goes_to_the_end_of_the_eeprom(void)
{
    uint8_t data[NV_WRITE_BYTES_MAX];

    start_eeprom();
    for (size_t i = 0U; i < sizeof(data); i++)
    {
        data[i] = (uint8_t)(i + 1U);
    }
    TEST_ASSERT_EQUAL(STATUS_OK,
                      nv_write((uint16_t)(EEPROM_BYTES - NV_WRITE_BYTES_MAX), data,
                               (uint8_t)NV_WRITE_BYTES_MAX));
    poll_times(MANY_POLLS);

    TEST_ASSERT_EQUAL(NV_WRITE_BYTES_MAX, g_fake_eeprom_writes);
    for (size_t i = 0U; i < sizeof(data); i++)
    {
        TEST_ASSERT_EQUAL(i + 1U, g_fake_eeprom[(EEPROM_BYTES - NV_WRITE_BYTES_MAX) + i]);
    }
    TEST_ASSERT_EQUAL(ERASED, g_fake_eeprom[EEPROM_BYTES - NV_WRITE_BYTES_MAX - 1U]);
    TEST_ASSERT_EQUAL(ERASED, g_fake_eeprom[0]); // The address did not wrap
}



static void test_poll_with_nothing_to_write_leaves_the_eeprom_alone(void)
{
    start_eeprom();
    g_fake_eecr = (uint8_t)(1U << EEPE); // Even the status is not looked at
    g_fake_eeprom_b_writing  = true;
    g_fake_eeprom_polls_left = 7U;
    nv_poll();
    nv_poll();

    TEST_ASSERT_EQUAL(7, g_fake_eeprom_polls_left);
    TEST_ASSERT_EQUAL(0, g_fake_eeprom_reads);
    TEST_ASSERT_EQUAL(0, g_fake_eeprom_writes);
}



int main(void)
{
    RUN_TEST(test_read_returns_the_bytes_at_the_address);
    RUN_TEST(test_read_reaches_the_first_and_last_bytes);
    RUN_TEST(test_an_erased_eeprom_reads_as_all_ones);
    RUN_TEST(test_read_rejects_bad_parameters_and_touches_nothing);
    RUN_TEST(test_write_does_nothing_until_polled_then_one_byte_per_poll);
    RUN_TEST(test_write_keeps_its_own_copy_of_the_bytes);
    RUN_TEST(test_each_byte_is_armed_then_started_with_interrupts_off);
    RUN_TEST(test_interrupts_that_were_off_stay_off);
    RUN_TEST(test_poll_waits_for_the_byte_in_progress_without_blocking);
    RUN_TEST(test_a_byte_already_there_is_not_written_again);
    RUN_TEST(test_a_second_write_is_refused_until_the_first_has_gone);
    RUN_TEST(test_read_is_refused_while_a_write_is_being_carried_out);
    RUN_TEST(test_write_rejects_bad_parameters_and_takes_nothing);
    RUN_TEST(test_the_largest_write_goes_to_the_end_of_the_eeprom);
    RUN_TEST(test_poll_with_nothing_to_write_leaves_the_eeprom_alone);
    return TEST_RESULT();
}
