/**
 * @file   test_errlog.c
 * @brief  Host unit tests for the crash log (core/errlog.c). The checksums
 *         in the records written out here were worked out apart from the
 *         firmware, in Python: CRC-8, polynomial 0x07, starting from 1.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "errlog.h"
#include "test_harness.h"

#define RECORD_BYTES   (5U)    // The size the layout comes to
#define RECORD_SLOTS   (8U)
#define LOG_BYTES      (40U)
#define ERASED         (0xFFU) // What storage never written holds
#define WATCHDOG       (0x08U) // The reset cause that is a crash
#define UNTOUCHED      (0xA5U) // Marks something that must not be written
#define PLACE_A        (5U)
#define PLACE_B        (3U)

// number 1, cause 8, place 5, count 1 and count 2
static const uint8_t g_first_record[RECORD_BYTES]  = { 0x01U, 0x08U, 0x05U, 0x01U, 0x17U };
static const uint8_t g_first_twice[RECORD_BYTES]   = { 0x01U, 0x08U, 0x05U, 0x02U, 0x1EU };
// number 2, cause 8, place 3, count 1
static const uint8_t g_second_record[RECORD_BYTES] = { 0x02U, 0x08U, 0x03U, 0x01U, 0x53U };

/**
 * @brief Fills a log with what erased storage holds.
 */
static void erase(uint8_t *p_log)
{
    (void)memset(p_log, ERASED, LOG_BYTES);
}

/**
 * @brief Adds a crash that must be taken, and returns the slot it went to.
 */
static uint8_t add(uint8_t *p_log, uint8_t place)
{
    uint8_t offset = UNTOUCHED;

    TEST_ASSERT(errlog_add(p_log, WATCHDOG, place, &offset));
    TEST_ASSERT_EQUAL(0, offset % RECORD_BYTES);
    return (uint8_t)(offset / RECORD_BYTES);
}

/**
 * @brief Checks the newest record of a log.
 */
static void expect_newest(const uint8_t *p_log, uint8_t number, uint8_t place, uint8_t count)
{
    errlog_entry_t entry = { UNTOUCHED, UNTOUCHED, UNTOUCHED, UNTOUCHED };

    TEST_ASSERT(errlog_newest(p_log, &entry));
    TEST_ASSERT_EQUAL(number, entry.number);
    TEST_ASSERT_EQUAL(WATCHDOG, entry.cause);
    TEST_ASSERT_EQUAL(place, entry.place);
    TEST_ASSERT_EQUAL(count, entry.count);
}

/**
 * @brief Checks that a log holds no record, and that asking writes nothing.
 */
static void expect_empty(const uint8_t *p_log)
{
    errlog_entry_t entry = { UNTOUCHED, UNTOUCHED, UNTOUCHED, UNTOUCHED };

    TEST_ASSERT(!errlog_newest(p_log, &entry));
    TEST_ASSERT_EQUAL(UNTOUCHED, entry.number);
    TEST_ASSERT_EQUAL(UNTOUCHED, entry.cause);
    TEST_ASSERT_EQUAL(UNTOUCHED, entry.place);
    TEST_ASSERT_EQUAL(UNTOUCHED, entry.count);
}



static void test_the_log_is_eight_records_of_five_bytes(void)
{
    TEST_ASSERT_EQUAL(RECORD_BYTES, ERRLOG_RECORD_SIZE);
    TEST_ASSERT_EQUAL(RECORD_SLOTS, ERRLOG_RECORD_COUNT);
    TEST_ASSERT_EQUAL(LOG_BYTES, ERRLOG_SIZE);
}



static void test_erased_and_blank_storage_hold_no_record(void)
{
    uint8_t ring[LOG_BYTES];

    erase(ring);
    expect_empty(ring);

    (void)memset(ring, 0x00, sizeof(ring));
    expect_empty(ring);
}



static void test_the_first_crash_is_record_one_in_the_first_slot(void)
{
    uint8_t ring[LOG_BYTES];

    erase(ring);
    TEST_ASSERT_EQUAL(0, add(ring, PLACE_A));

    TEST_ASSERT_EQUAL(0, memcmp(g_first_record, ring, RECORD_BYTES));
    for (size_t i = RECORD_BYTES; i < LOG_BYTES; i++)
    {
        TEST_ASSERT_EQUAL(ERASED, ring[i]); // Nothing else is written
    }
    expect_newest(ring, 1U, PLACE_A, 1U);
}



static void test_a_start_that_was_not_a_crash_is_not_recorded(void)
{
    // Every cause without the watchdog among them: power-on, the reset pin,
    // a brown-out, and none at all
    for (uint8_t cause = 0U; cause < 8U; cause++)
    {
        uint8_t ring[LOG_BYTES];
        uint8_t offset = UNTOUCHED;

        erase(ring);
        TEST_ASSERT(!errlog_add(ring, cause, PLACE_A, &offset));
        TEST_ASSERT_EQUAL(UNTOUCHED, offset);
        expect_empty(ring);
    }
}



static void test_the_watchdog_with_another_cause_is_a_crash_and_the_cause_is_kept(void)
{
    // number 1, cause 9 (power-on as well), place 1, count 1
    static const uint8_t expected[RECORD_BYTES] = { 0x01U, 0x09U, 0x01U, 0x01U, 0x28U };
    uint8_t              ring[LOG_BYTES];
    uint8_t              offset = UNTOUCHED;
    errlog_entry_t       entry;

    erase(ring);
    TEST_ASSERT(errlog_add(ring, 0x09U, 1U, &offset));
    TEST_ASSERT_EQUAL(0, offset);
    TEST_ASSERT_EQUAL(0, memcmp(expected, ring, RECORD_BYTES));
    TEST_ASSERT(errlog_newest(ring, &entry));
    TEST_ASSERT_EQUAL(0x09, entry.cause);
}



static void test_the_same_crash_again_raises_the_count_and_takes_no_slot(void)
{
    uint8_t ring[LOG_BYTES];

    erase(ring);
    (void)add(ring, PLACE_A);
    TEST_ASSERT_EQUAL(0, add(ring, PLACE_A));

    TEST_ASSERT_EQUAL(0, memcmp(g_first_twice, ring, RECORD_BYTES));
    TEST_ASSERT_EQUAL(ERASED, ring[RECORD_BYTES]);
    expect_newest(ring, 1U, PLACE_A, 2U);
}



static void test_a_crash_somewhere_else_takes_the_next_slot_and_number(void)
{
    uint8_t ring[LOG_BYTES];

    erase(ring);
    (void)add(ring, PLACE_A);
    TEST_ASSERT_EQUAL(1, add(ring, PLACE_B));

    TEST_ASSERT_EQUAL(0, memcmp(g_first_record, ring, RECORD_BYTES)); // Left as it was
    TEST_ASSERT_EQUAL(0, memcmp(g_second_record, &ring[RECORD_BYTES], RECORD_BYTES));
    expect_newest(ring, 2U, PLACE_B, 1U);

    // And back at the first place: a new record, since the newest differs
    TEST_ASSERT_EQUAL(2, add(ring, PLACE_A));
    expect_newest(ring, 3U, PLACE_A, 1U);
}



static void test_another_cause_at_the_same_place_is_another_record(void)
{
    uint8_t        ring[LOG_BYTES];
    uint8_t        offset = UNTOUCHED;
    errlog_entry_t entry;

    erase(ring);
    (void)add(ring, PLACE_A);
    TEST_ASSERT(errlog_add(ring, 0x0CU, PLACE_A, &offset)); // A brown-out as well
    TEST_ASSERT_EQUAL(RECORD_BYTES, offset);
    TEST_ASSERT(errlog_newest(ring, &entry));
    TEST_ASSERT_EQUAL(2, entry.number);
    TEST_ASSERT_EQUAL(0x0C, entry.cause);
    TEST_ASSERT_EQUAL(1, entry.count);
}



static void test_the_count_stops_at_255_and_then_nothing_is_written(void)
{
    // number 1, cause 8, place 5, count 255
    static const uint8_t full[RECORD_BYTES] = { 0x01U, 0x08U, 0x05U, 0xFFU, 0xE3U };
    uint8_t              ring[LOG_BYTES];
    uint8_t              before[LOG_BYTES];
    uint8_t              offset = UNTOUCHED;

    erase(ring);
    for (unsigned int crash = 0U; crash < 255U; crash++)
    {
        TEST_ASSERT_EQUAL(0, add(ring, PLACE_A));
    }
    TEST_ASSERT_EQUAL(0, memcmp(full, ring, RECORD_BYTES));

    (void)memcpy(before, ring, sizeof(before));
    TEST_ASSERT(!errlog_add(ring, WATCHDOG, PLACE_A, &offset));
    TEST_ASSERT_EQUAL(UNTOUCHED, offset);
    TEST_ASSERT_EQUAL(0, memcmp(before, ring, sizeof(ring)));
    expect_newest(ring, 1U, PLACE_A, 255U);

    // A different crash is still taken
    TEST_ASSERT_EQUAL(1, add(ring, PLACE_B));
    expect_newest(ring, 2U, PLACE_B, 1U);
}



static void test_the_ninth_record_goes_round_to_the_first_slot(void)
{
    uint8_t ring[LOG_BYTES];

    erase(ring);
    for (uint8_t record = 0U; record < RECORD_SLOTS; record++)
    {
        // Alternating places, so that each crash is a record of its own
        TEST_ASSERT_EQUAL(record, add(ring, (uint8_t)(record % 2U)));
        expect_newest(ring, (uint8_t)(record + 1U), (uint8_t)(record % 2U), 1U);
    }

    TEST_ASSERT_EQUAL(0, add(ring, PLACE_A));
    expect_newest(ring, 9U, PLACE_A, 1U);
    TEST_ASSERT_EQUAL(2, ring[RECORD_BYTES]); // Record 2 is now the oldest kept

    TEST_ASSERT_EQUAL(1, add(ring, PLACE_B));
    expect_newest(ring, 10U, PLACE_B, 1U);
    TEST_ASSERT_EQUAL(1, add(ring, PLACE_B)); // The same again stays in slot 1
    expect_newest(ring, 10U, PLACE_B, 2U);
}



static void test_the_number_goes_from_255_round_to_0_and_the_log_still_orders(void)
{
    uint8_t ring[LOG_BYTES];

    erase(ring);
    for (unsigned int record = 0U; record < 255U; record++)
    {
        (void)add(ring, (uint8_t)(record % 2U));
    }
    expect_newest(ring, 255U, 0U, 1U);

    (void)add(ring, 1U);
    expect_newest(ring, 0U, 1U, 1U);
    (void)add(ring, 0U);
    expect_newest(ring, 1U, 0U, 1U);
}



static void test_a_slot_that_is_not_a_record_is_an_empty_place(void)
{
    uint8_t ring[LOG_BYTES];

    // Three records, then the newest spoiled, as by a write cut off part
    // way: the one before it is the newest again, and the slot is reused
    erase(ring);
    (void)add(ring, PLACE_A);
    (void)add(ring, PLACE_B);
    (void)add(ring, PLACE_A);
    ring[(2U * RECORD_BYTES) + 3U] = 0x02U; // The count, without its checksum
    expect_newest(ring, 2U, PLACE_B, 1U);

    TEST_ASSERT_EQUAL(2, add(ring, PLACE_A));
    expect_newest(ring, 3U, PLACE_A, 1U);
}



static void test_any_single_bit_changed_spoils_a_record(void)
{
    for (size_t byte = 0U; byte < RECORD_BYTES; byte++)
    {
        for (unsigned int bit = 0U; bit < 8U; bit++)
        {
            uint8_t ring[LOG_BYTES];

            erase(ring);
            (void)memcpy(ring, g_first_record, RECORD_BYTES);
            ring[byte] = (uint8_t)(ring[byte] ^ (1U << bit));
            expect_empty(ring);
        }
    }
}



static void test_a_record_of_another_version_is_not_one(void)
{
    // The first record with the checksum a layout numbered 0 would give it
    static const uint8_t other[RECORD_BYTES] = { 0x01U, 0x08U, 0x05U, 0x01U, 0x01U };
    uint8_t              ring[LOG_BYTES];

    erase(ring);
    (void)memcpy(ring, other, RECORD_BYTES);
    expect_empty(ring);

    // It is written over as if the log were empty
    TEST_ASSERT_EQUAL(0, add(ring, PLACE_A));
    TEST_ASSERT_EQUAL(0, memcmp(g_first_record, ring, RECORD_BYTES));
}



static void test_a_record_found_alone_is_the_newest_wherever_it_is(void)
{
    for (uint8_t slot = 0U; slot < RECORD_SLOTS; slot++)
    {
        uint8_t ring[LOG_BYTES];

        erase(ring);
        (void)memcpy(&ring[slot * RECORD_BYTES], g_second_record, RECORD_BYTES);
        expect_newest(ring, 2U, PLACE_B, 1U);

        // The next goes after it, round to the first slot from the last
        TEST_ASSERT_EQUAL((slot + 1U) % RECORD_SLOTS, add(ring, PLACE_A));
        expect_newest(ring, 3U, PLACE_A, 1U);
    }
}



static void test_null_pointers_do_nothing(void)
{
    uint8_t        ring[LOG_BYTES];
    uint8_t        offset = UNTOUCHED;
    errlog_entry_t entry  = { UNTOUCHED, UNTOUCHED, UNTOUCHED, UNTOUCHED };

    erase(ring);
    TEST_ASSERT(!errlog_add(NULL, WATCHDOG, PLACE_A, &offset));
    TEST_ASSERT_EQUAL(UNTOUCHED, offset);
    TEST_ASSERT(!errlog_add(ring, WATCHDOG, PLACE_A, NULL));
    expect_empty(ring);

    (void)add(ring, PLACE_A);
    TEST_ASSERT(!errlog_newest(NULL, &entry));
    TEST_ASSERT_EQUAL(UNTOUCHED, entry.number);
    TEST_ASSERT(!errlog_newest(ring, NULL));
}



int main(void)
{
    RUN_TEST(test_the_log_is_eight_records_of_five_bytes);
    RUN_TEST(test_erased_and_blank_storage_hold_no_record);
    RUN_TEST(test_the_first_crash_is_record_one_in_the_first_slot);
    RUN_TEST(test_a_start_that_was_not_a_crash_is_not_recorded);
    RUN_TEST(test_the_watchdog_with_another_cause_is_a_crash_and_the_cause_is_kept);
    RUN_TEST(test_the_same_crash_again_raises_the_count_and_takes_no_slot);
    RUN_TEST(test_a_crash_somewhere_else_takes_the_next_slot_and_number);
    RUN_TEST(test_another_cause_at_the_same_place_is_another_record);
    RUN_TEST(test_the_count_stops_at_255_and_then_nothing_is_written);
    RUN_TEST(test_the_ninth_record_goes_round_to_the_first_slot);
    RUN_TEST(test_the_number_goes_from_255_round_to_0_and_the_log_still_orders);
    RUN_TEST(test_a_slot_that_is_not_a_record_is_an_empty_place);
    RUN_TEST(test_any_single_bit_changed_spoils_a_record);
    RUN_TEST(test_a_record_of_another_version_is_not_one);
    RUN_TEST(test_a_record_found_alone_is_the_newest_wherever_it_is);
    RUN_TEST(test_null_pointers_do_nothing);
    return TEST_RESULT();
}
