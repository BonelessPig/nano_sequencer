/**
 * @file   test_pitch_cal.c
 * @brief  Host unit tests for the pitch calibration (core/pitch_cal.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include "pitch_cal.h"
#include "test_harness.h"

#define SEMITONES_PER_OCTAVE (12U)
#define TOP_SEMITONE         (48U) // The pitch of the table's last entry

// An ideal 1 V per octave output at 1 mV per CV value
static const pitch_cal_config_t g_ideal = { { 0U, 1000U, 2000U, 3000U, 4000U } };

static void test_each_octave_gives_its_table_entry(void)
{
    static const pitch_cal_config_t uneven = { { 7U, 1012U, 1998U, 3040U, 4095U } };

    for (uint8_t point = 0U; point < PITCH_CAL_POINT_COUNT; point++)
    {
        const uint8_t semitone = (uint8_t)(point * SEMITONES_PER_OCTAVE);

        TEST_ASSERT_EQUAL(uneven.cv[point], pitch_cal_cv(&uneven, semitone));
    }
}



static void test_ideal_table_gives_a_twelfth_of_an_octave_per_semitone(void)
{
    // 1000 / 12 = 83.33 a semitone, rounded to the nearest
    static const uint16_t first_octave[SEMITONES_PER_OCTAVE] =
    {
        0U, 83U, 167U, 250U, 333U, 417U, 500U, 583U, 667U, 750U, 833U, 917U
    };

    for (uint8_t semitone = 0U; semitone <= TOP_SEMITONE; semitone++)
    {
        const uint16_t octave   = (uint16_t)(semitone / SEMITONES_PER_OCTAVE);
        const uint16_t expected =
            (uint16_t)((octave * 1000U) + first_octave[semitone % SEMITONES_PER_OCTAVE]);

        TEST_ASSERT_EQUAL(expected, pitch_cal_cv(&g_ideal, semitone));
    }
    TEST_ASSERT_EQUAL(3750, pitch_cal_cv(&g_ideal, 45U)); // The highest pitch played
}



static void test_ideal_table_is_never_more_than_half_a_value_out(void)
{
    for (uint8_t semitone = 0U; semitone <= TOP_SEMITONE; semitone++)
    {
        // Twelve times the error, to stay in whole numbers
        const long exact_x12 = (long)semitone * 1000L;
        const long given_x12 = (long)pitch_cal_cv(&g_ideal, semitone) * 12L;
        const long error_x12 = (given_x12 > exact_x12) ? (given_x12 - exact_x12)
                                                       : (exact_x12 - given_x12);

        TEST_ASSERT(error_x12 <= 6L);
    }
}



static void test_a_pitch_between_two_entries_lies_on_the_line_between_them(void)
{
    // Each octave has its own slope
    static const pitch_cal_config_t uneven = { { 100U, 700U, 1900U, 1960U, 4095U } };

    TEST_ASSERT_EQUAL(150, pitch_cal_cv(&uneven, 1U));   // 100 + 600 * 1/12
    TEST_ASSERT_EQUAL(400, pitch_cal_cv(&uneven, 6U));   // 100 + 600 * 6/12
    TEST_ASSERT_EQUAL(650, pitch_cal_cv(&uneven, 11U));  // 100 + 600 * 11/12
    TEST_ASSERT_EQUAL(800, pitch_cal_cv(&uneven, 13U));  // 700 + 1200 * 1/12
    TEST_ASSERT_EQUAL(1930, pitch_cal_cv(&uneven, 30U)); // 1900 + 60 * 6/12
    TEST_ASSERT_EQUAL(2138, pitch_cal_cv(&uneven, 37U)); // 1960 + 2135 * 1/12 = 2137.9
    TEST_ASSERT_EQUAL(3917, pitch_cal_cv(&uneven, 47U)); // 1960 + 2135 * 11/12 = 3917.1
}



static void test_a_falling_table_is_followed_downwards(void)
{
    // An output stage that inverts: the highest pitch has the lowest value
    static const pitch_cal_config_t falling = { { 4000U, 3000U, 2000U, 1000U, 0U } };

    TEST_ASSERT_EQUAL(4000, pitch_cal_cv(&falling, 0U));
    TEST_ASSERT_EQUAL(3917, pitch_cal_cv(&falling, 1U));
    TEST_ASSERT_EQUAL(3500, pitch_cal_cv(&falling, 6U));
    TEST_ASSERT_EQUAL(250, pitch_cal_cv(&falling, 45U));
    TEST_ASSERT_EQUAL(0, pitch_cal_cv(&falling, 48U));
}



static void test_a_pitch_above_the_table_gives_the_last_entry(void)
{
    static const pitch_cal_config_t table = { { 0U, 1000U, 2000U, 3000U, 3900U } };

    TEST_ASSERT_EQUAL(3900, pitch_cal_cv(&table, 48U));
    TEST_ASSERT_EQUAL(3900, pitch_cal_cv(&table, 49U));
    TEST_ASSERT_EQUAL(3900, pitch_cal_cv(&table, 60U));
    TEST_ASSERT_EQUAL(3900, pitch_cal_cv(&table, UINT8_MAX));
}



static void test_an_entry_above_the_range_is_treated_as_the_maximum(void)
{
    static const pitch_cal_config_t too_high =
    {
        { 5000U, UINT16_MAX, 2000U, 4096U, UINT16_MAX }
    };

    TEST_ASSERT_EQUAL(PITCH_CAL_CV_MAX, pitch_cal_cv(&too_high, 0U));
    TEST_ASSERT_EQUAL(PITCH_CAL_CV_MAX, pitch_cal_cv(&too_high, 6U));  // Both ends limited
    TEST_ASSERT_EQUAL(PITCH_CAL_CV_MAX, pitch_cal_cv(&too_high, 12U));
    TEST_ASSERT_EQUAL(3048, pitch_cal_cv(&too_high, 18U)); // Half way from 4095 to 2000
    TEST_ASSERT_EQUAL(3048, pitch_cal_cv(&too_high, 30U)); // Half way from 2000 to 4095
    TEST_ASSERT_EQUAL(PITCH_CAL_CV_MAX, pitch_cal_cv(&too_high, 48U));
}



static void test_the_largest_table_does_not_overflow(void)
{
    static const pitch_cal_config_t full =
    {
        { PITCH_CAL_CV_MAX, PITCH_CAL_CV_MAX, PITCH_CAL_CV_MAX, PITCH_CAL_CV_MAX,
          PITCH_CAL_CV_MAX }
    };

    for (uint8_t semitone = 0U; semitone <= TOP_SEMITONE; semitone++)
    {
        TEST_ASSERT_EQUAL(PITCH_CAL_CV_MAX, pitch_cal_cv(&full, semitone));
    }
}



static void test_a_null_table_gives_zero(void)
{
    TEST_ASSERT_EQUAL(0, pitch_cal_cv(NULL, 0U));
    TEST_ASSERT_EQUAL(0, pitch_cal_cv(NULL, 30U));
    TEST_ASSERT_EQUAL(0, pitch_cal_cv(NULL, UINT8_MAX));
}



int main(void)
{
    RUN_TEST(test_each_octave_gives_its_table_entry);
    RUN_TEST(test_ideal_table_gives_a_twelfth_of_an_octave_per_semitone);
    RUN_TEST(test_ideal_table_is_never_more_than_half_a_value_out);
    RUN_TEST(test_a_pitch_between_two_entries_lies_on_the_line_between_them);
    RUN_TEST(test_a_falling_table_is_followed_downwards);
    RUN_TEST(test_a_pitch_above_the_table_gives_the_last_entry);
    RUN_TEST(test_an_entry_above_the_range_is_treated_as_the_maximum);
    RUN_TEST(test_the_largest_table_does_not_overflow);
    RUN_TEST(test_a_null_table_gives_zero);
    return TEST_RESULT();
}
