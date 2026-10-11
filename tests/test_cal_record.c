/**
 * @file   test_cal_record.c
 * @brief  Host unit tests for the calibration record (core/cal_record.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "cal_record.h"
#include "test_harness.h"

#define RECORD_BYTES    (12U)   // The size the layout comes to
#define CHECKSUM_AT     (11U)
#define UNTOUCHED_ENTRY (0x5A5AU) // Marks a table entry that must not be written
#define UNTOUCHED_BYTE  (0xA5U)

static const pitch_cal_config_t g_ideal  = { { 0U, 1000U, 2000U, 3000U, 4000U } };
static const pitch_cal_config_t g_uneven = { { 7U, 1012U, 1998U, 3040U, 4095U } };

// The ideal table as a record. The checksum was worked out separately, by a
// CRC-8 (polynomial 0x07, from 0) that gives the standard 0xF4 for "123456789"
static const uint8_t g_ideal_record[RECORD_BYTES] =
{
    0x01U, 0x00U, 0x00U, 0xE8U, 0x03U, 0xD0U, 0x07U, 0xB8U, 0x0BU, 0xA0U, 0x0FU, 0x30U
};

/**
 * @brief A table whose every entry is the marker, to see what decode wrote.
 */
static pitch_cal_config_t untouched_table(void)
{
    pitch_cal_config_t table;

    for (size_t point = 0U; point < PITCH_CAL_POINT_COUNT; point++)
    {
        table.cv[point] = UNTOUCHED_ENTRY;
    }
    return table;
}

/**
 * @brief Checks that decode refuses the bytes and writes nothing.
 */
static void expect_refused(const uint8_t *p_bytes)
{
    pitch_cal_config_t table = untouched_table();

    TEST_ASSERT(!cal_record_decode(p_bytes, &table));
    for (size_t point = 0U; point < PITCH_CAL_POINT_COUNT; point++)
    {
        TEST_ASSERT_EQUAL(UNTOUCHED_ENTRY, table.cv[point]);
    }
}



static void test_the_record_is_twelve_bytes(void)
{
    TEST_ASSERT_EQUAL(RECORD_BYTES, CAL_RECORD_SIZE);
}



static void test_encode_lays_out_version_entries_low_byte_first_and_checksum(void)
{
    uint8_t bytes[RECORD_BYTES];

    (void)memset(bytes, UNTOUCHED_BYTE, sizeof(bytes));
    cal_record_encode(&g_ideal, bytes);
    TEST_ASSERT_EQUAL(0, memcmp(g_ideal_record, bytes, sizeof(bytes)));

    // A second table, with its own separately worked checksum
    cal_record_encode(&g_uneven, bytes);
    TEST_ASSERT_EQUAL(0x01, bytes[0]);
    TEST_ASSERT_EQUAL(0x07, bytes[1]);
    TEST_ASSERT_EQUAL(0x00, bytes[2]);
    TEST_ASSERT_EQUAL(0xFF, bytes[9]);
    TEST_ASSERT_EQUAL(0x0F, bytes[10]);
    TEST_ASSERT_EQUAL(0x68, bytes[CHECKSUM_AT]);
}



static void test_decode_reads_back_what_encode_wrote(void)
{
    static const pitch_cal_config_t tables[] =
    {
        { { 0U, 1000U, 2000U, 3000U, 4000U } },
        { { 7U, 1012U, 1998U, 3040U, 4095U } },
        { { 4000U, 3000U, 2000U, 1000U, 0U } },
        { { 0U, 0U, 0U, 0U, 0U } },
        { { UINT16_MAX, 0x00FFU, 0xFF00U, 0x1234U, 0x8001U } } // Kept as given
    };

    for (size_t i = 0U; i < (sizeof(tables) / sizeof(tables[0])); i++)
    {
        uint8_t            bytes[RECORD_BYTES];
        pitch_cal_config_t table = untouched_table();

        cal_record_encode(&tables[i], bytes);
        TEST_ASSERT(cal_record_decode(bytes, &table));
        for (size_t point = 0U; point < PITCH_CAL_POINT_COUNT; point++)
        {
            TEST_ASSERT_EQUAL(tables[i].cv[point], table.cv[point]);
        }
    }
}



static void test_decode_accepts_a_record_made_elsewhere(void)
{
    pitch_cal_config_t table = untouched_table();

    TEST_ASSERT(cal_record_decode(g_ideal_record, &table));
    for (size_t point = 0U; point < PITCH_CAL_POINT_COUNT; point++)
    {
        TEST_ASSERT_EQUAL(g_ideal.cv[point], table.cv[point]);
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



static void test_decode_refuses_another_version_with_a_good_checksum(void)
{
    // Version 2 of the same entries; 0x11 is the right checksum for it
    static const uint8_t version_two[RECORD_BYTES] =
    {
        0x02U, 0x00U, 0x00U, 0xE8U, 0x03U, 0xD0U, 0x07U, 0xB8U, 0x0BU, 0xA0U, 0x0FU, 0x11U
    };
    uint8_t as_version_one[RECORD_BYTES];

    expect_refused(version_two);

    // The proof that only the version was wrong: the same bytes as version 1
    // need a different checksum, and with it they are accepted
    (void)memcpy(as_version_one, version_two, sizeof(as_version_one));
    as_version_one[0] = 0x01U;
    expect_refused(as_version_one);
    as_version_one[CHECKSUM_AT] = 0x30U;
    {
        pitch_cal_config_t table = untouched_table();

        TEST_ASSERT(cal_record_decode(as_version_one, &table));
    }
}



static void test_decode_refuses_any_single_bit_changed(void)
{
    for (size_t byte = 0U; byte < RECORD_BYTES; byte++)
    {
        for (unsigned int bit = 0U; bit < 8U; bit++)
        {
            uint8_t bytes[RECORD_BYTES];

            (void)memcpy(bytes, g_ideal_record, sizeof(bytes));
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
        (void)memcpy(bytes, g_ideal_record, written);
        expect_refused(bytes);
    }
}



static void test_null_pointers_do_nothing(void)
{
    uint8_t            bytes[RECORD_BYTES];
    pitch_cal_config_t table = untouched_table();

    (void)memset(bytes, UNTOUCHED_BYTE, sizeof(bytes));
    cal_record_encode(NULL, bytes);
    for (size_t i = 0U; i < RECORD_BYTES; i++)
    {
        TEST_ASSERT_EQUAL(UNTOUCHED_BYTE, bytes[i]);
    }
    cal_record_encode(&g_ideal, NULL);

    TEST_ASSERT(!cal_record_decode(NULL, &table));
    TEST_ASSERT_EQUAL(UNTOUCHED_ENTRY, table.cv[0]);
    TEST_ASSERT(!cal_record_decode(g_ideal_record, NULL));
    TEST_ASSERT(!cal_record_decode(NULL, NULL));
}



int main(void)
{
    RUN_TEST(test_the_record_is_twelve_bytes);
    RUN_TEST(test_encode_lays_out_version_entries_low_byte_first_and_checksum);
    RUN_TEST(test_decode_reads_back_what_encode_wrote);
    RUN_TEST(test_decode_accepts_a_record_made_elsewhere);
    RUN_TEST(test_decode_refuses_erased_and_blank_storage);
    RUN_TEST(test_decode_refuses_another_version_with_a_good_checksum);
    RUN_TEST(test_decode_refuses_any_single_bit_changed);
    RUN_TEST(test_decode_refuses_a_write_cut_off_part_way);
    RUN_TEST(test_null_pointers_do_nothing);
    return TEST_RESULT();
}
