/**
 * @file   test_profile_record.c
 * @brief  Host unit tests for the profile record (core/profile_record.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "profile_record.h"
#include "test_harness.h"

#define AMOUNTS          (7U)    // The amounts a profile holds
#define RECORD_BYTES     (16U)   // The size the layout comes to
#define CHECKSUM_AT      (15U)
#define UNTOUCHED_AMOUNT (0x5A5AU) // Marks an amount that must not be written
#define UNTOUCHED_BYTE   (0xA5U)

static const uint16_t g_mixed[AMOUNTS] = { 0U, 1023U, 512U, 1U, 256U, 700U, 1000U };

// The mixed amounts as a record. The checksum was worked out separately, by a
// CRC-8 (polynomial 0x07, from 0) that gives the standard 0xF4 for "123456789"
static const uint8_t g_mixed_record[RECORD_BYTES] =
{
    0x01U, 0x00U, 0x00U, 0xFFU, 0x03U, 0x00U, 0x02U, 0x01U,
    0x00U, 0x00U, 0x01U, 0xBCU, 0x02U, 0xE8U, 0x03U, 0x14U
};

/**
 * @brief Fills a set of amounts with the marker, to see what decode wrote.
 */
static void mark_untouched(uint16_t *p_amounts)
{
    for (size_t i = 0U; i < AMOUNTS; i++)
    {
        p_amounts[i] = UNTOUCHED_AMOUNT;
    }
}

/**
 * @brief Checks that decode refuses the bytes and writes nothing.
 */
static void expect_refused(const uint8_t *p_bytes)
{
    uint16_t amounts[AMOUNTS];

    mark_untouched(amounts);
    TEST_ASSERT(!profile_record_decode(p_bytes, amounts));
    for (size_t i = 0U; i < AMOUNTS; i++)
    {
        TEST_ASSERT_EQUAL(UNTOUCHED_AMOUNT, amounts[i]);
    }
}



static void test_the_record_is_sixteen_bytes_of_seven_amounts(void)
{
    TEST_ASSERT_EQUAL(RECORD_BYTES, PROFILE_RECORD_SIZE);
    TEST_ASSERT_EQUAL(AMOUNTS, PROFILE_AMOUNT_COUNT);
}



static void test_encode_lays_out_version_amounts_low_byte_first_and_checksum(void)
{
    // A second set, kept as given, with its own separately worked checksum
    static const uint16_t wide[AMOUNTS] = { 1023U, 0U, 300U, 4U, 5U, 6U, 0xABCDU };
    uint8_t               bytes[RECORD_BYTES];

    (void)memset(bytes, UNTOUCHED_BYTE, sizeof(bytes));
    profile_record_encode(g_mixed, bytes);
    TEST_ASSERT_EQUAL(0, memcmp(g_mixed_record, bytes, sizeof(bytes)));

    profile_record_encode(wide, bytes);
    TEST_ASSERT_EQUAL(0x01, bytes[0]);
    TEST_ASSERT_EQUAL(0xFF, bytes[1]);
    TEST_ASSERT_EQUAL(0x03, bytes[2]);
    TEST_ASSERT_EQUAL(0xCD, bytes[13]);
    TEST_ASSERT_EQUAL(0xAB, bytes[14]);
    TEST_ASSERT_EQUAL(0x63, bytes[CHECKSUM_AT]);
}



static void test_decode_reads_back_what_encode_wrote(void)
{
    static const uint16_t sets[][AMOUNTS] =
    {
        { 0U, 1023U, 512U, 1U, 256U, 700U, 1000U },
        { 100U, 200U, 300U, 400U, 500U, 600U, 700U },
        { 0U, 0U, 0U, 0U, 0U, 0U, 0U },
        { UINT16_MAX, 0x00FFU, 0xFF00U, 0x1234U, 0x8001U, 1U, 2U } // Kept as given
    };

    for (size_t set = 0U; set < (sizeof(sets) / sizeof(sets[0])); set++)
    {
        uint8_t  bytes[RECORD_BYTES];
        uint16_t amounts[AMOUNTS];

        mark_untouched(amounts);
        profile_record_encode(sets[set], bytes);
        TEST_ASSERT(profile_record_decode(bytes, amounts));
        for (size_t i = 0U; i < AMOUNTS; i++)
        {
            TEST_ASSERT_EQUAL(sets[set][i], amounts[i]);
        }
    }
}



static void test_decode_accepts_a_record_made_elsewhere(void)
{
    uint16_t amounts[AMOUNTS];

    mark_untouched(amounts);
    TEST_ASSERT(profile_record_decode(g_mixed_record, amounts));
    for (size_t i = 0U; i < AMOUNTS; i++)
    {
        TEST_ASSERT_EQUAL(g_mixed[i], amounts[i]);
    }
}



static void test_decode_refuses_erased_and_blank_storage(void)
{
    uint8_t bytes[RECORD_BYTES];

    (void)memset(bytes, 0xFF, sizeof(bytes)); // An EEPROM never written
    expect_refused(bytes);

    // All zeros has a checksum that matches, and is refused on its version
    (void)memset(bytes, 0x00, sizeof(bytes));
    expect_refused(bytes);
}



static void test_the_blank_byte_over_the_first_clears_a_record(void)
{
    uint8_t bytes[RECORD_BYTES];

    (void)memcpy(bytes, g_mixed_record, sizeof(bytes));
    bytes[0] = PROFILE_RECORD_BLANK;
    TEST_ASSERT_EQUAL(0xFF, PROFILE_RECORD_BLANK);
    expect_refused(bytes);
}



static void test_decode_refuses_another_version_with_a_good_checksum(void)
{
    // Version 2 of the same amounts; 0xCC is the right checksum for it
    static const uint8_t version_two[RECORD_BYTES] =
    {
        0x02U, 0x00U, 0x00U, 0xFFU, 0x03U, 0x00U, 0x02U, 0x01U,
        0x00U, 0x00U, 0x01U, 0xBCU, 0x02U, 0xE8U, 0x03U, 0xCCU
    };
    uint8_t  as_version_one[RECORD_BYTES];
    uint16_t amounts[AMOUNTS];

    expect_refused(version_two);

    // The proof that only the version was wrong: the same bytes as version 1
    // need a different checksum, and with it they are accepted
    (void)memcpy(as_version_one, version_two, sizeof(as_version_one));
    as_version_one[0] = 0x01U;
    expect_refused(as_version_one);
    as_version_one[CHECKSUM_AT] = 0x14U;
    TEST_ASSERT(profile_record_decode(as_version_one, amounts));
}



static void test_decode_refuses_any_single_bit_changed(void)
{
    for (size_t byte = 0U; byte < RECORD_BYTES; byte++)
    {
        for (unsigned int bit = 0U; bit < 8U; bit++)
        {
            uint8_t bytes[RECORD_BYTES];

            (void)memcpy(bytes, g_mixed_record, sizeof(bytes));
            bytes[byte] = (uint8_t)(bytes[byte] ^ (1U << bit));
            expect_refused(bytes);
        }
    }
}



static void test_decode_refuses_a_write_cut_off_part_way(void)
{
    // The first bytes of a new record over an erased one
    for (size_t written = 1U; written < RECORD_BYTES; written++)
    {
        uint8_t bytes[RECORD_BYTES];

        (void)memset(bytes, 0xFF, sizeof(bytes));
        (void)memcpy(bytes, g_mixed_record, written);
        expect_refused(bytes);
    }
}



static void test_null_pointers_do_nothing(void)
{
    uint8_t  bytes[RECORD_BYTES];
    uint16_t amounts[AMOUNTS];

    mark_untouched(amounts);
    (void)memset(bytes, UNTOUCHED_BYTE, sizeof(bytes));
    profile_record_encode(NULL, bytes);
    for (size_t i = 0U; i < RECORD_BYTES; i++)
    {
        TEST_ASSERT_EQUAL(UNTOUCHED_BYTE, bytes[i]);
    }
    profile_record_encode(g_mixed, NULL);

    TEST_ASSERT(!profile_record_decode(NULL, amounts));
    TEST_ASSERT_EQUAL(UNTOUCHED_AMOUNT, amounts[0]);
    TEST_ASSERT(!profile_record_decode(g_mixed_record, NULL));
    TEST_ASSERT(!profile_record_decode(NULL, NULL));
}



int main(void)
{
    RUN_TEST(test_the_record_is_sixteen_bytes_of_seven_amounts);
    RUN_TEST(test_encode_lays_out_version_amounts_low_byte_first_and_checksum);
    RUN_TEST(test_decode_reads_back_what_encode_wrote);
    RUN_TEST(test_decode_accepts_a_record_made_elsewhere);
    RUN_TEST(test_decode_refuses_erased_and_blank_storage);
    RUN_TEST(test_the_blank_byte_over_the_first_clears_a_record);
    RUN_TEST(test_decode_refuses_another_version_with_a_good_checksum);
    RUN_TEST(test_decode_refuses_any_single_bit_changed);
    RUN_TEST(test_decode_refuses_a_write_cut_off_part_way);
    RUN_TEST(test_null_pointers_do_nothing);
    return TEST_RESULT();
}
