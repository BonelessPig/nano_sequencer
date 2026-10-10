/**
 * @file   test_note_map.c
 * @brief  Host unit tests for note mapping (core/note_map.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "note_map.h"
#include "test_harness.h"

#define POISON_BYTE (0xA5)
#define NOTE_COUNT  (15U) // Note values that sound: 1 to 15
#define ROOT_MAX    (11U) // Highest root
#define SEMITONE_MAX (45U) // Highest pitch note_map.h says any setting gives

/**
 * @brief The controls for a scale and a root.
 */
static note_map_config_t make_config(note_map_scale_t scale, uint8_t root)
{
    note_map_config_t config;

    (void)memset(&config, 0, sizeof(config));
    config.scale = scale;
    config.root  = root;
    return config;
}



/**
 * @brief Checks the pitch of every sounding note value, 1 to 15, against a
 *        table of NOTE_COUNT expected pitches.
 */
static void expect_pitches(const note_map_config_t *p_config, const uint8_t *p_expected)
{
    for (uint8_t note = 1U; note <= NOTE_COUNT; note++)
    {
        uint8_t semitone = POISON_BYTE;

        TEST_ASSERT(note_map_semitone(p_config, note, &semitone));
        TEST_ASSERT_EQUAL(p_expected[note - 1U], semitone);
    }
}



static void test_note_zero_is_a_rest_in_every_scale(void)
{
    for (int scale = 0; scale < (int)NOTE_MAP_SCALE_COUNT; scale++)
    {
        const note_map_config_t config   = make_config((note_map_scale_t)scale, 7U);
        uint8_t                 semitone = POISON_BYTE;

        TEST_ASSERT(!note_map_semitone(&config, NOTE_MAP_REST, &semitone));
        TEST_ASSERT_EQUAL(0, semitone); // Written, as 0
    }
}



static void test_chromatic_is_one_semitone_a_note(void)
{
    static const uint8_t expected[NOTE_COUNT] =
    {
        0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U
    };
    const note_map_config_t config = make_config(NOTE_MAP_SCALE_CHROMATIC, 0U);

    expect_pitches(&config, expected);
}



static void test_major_scale_spans_two_octaves(void)
{
    static const uint8_t expected[NOTE_COUNT] =
    {
        0U, 2U, 4U, 5U, 7U, 9U, 11U, 12U, 14U, 16U, 17U, 19U, 21U, 23U, 24U
    };
    const note_map_config_t config = make_config(NOTE_MAP_SCALE_MAJOR, 0U);

    expect_pitches(&config, expected);
}



static void test_minor_scale_spans_two_octaves(void)
{
    static const uint8_t expected[NOTE_COUNT] =
    {
        0U, 2U, 3U, 5U, 7U, 8U, 10U, 12U, 14U, 15U, 17U, 19U, 20U, 22U, 24U
    };
    const note_map_config_t config = make_config(NOTE_MAP_SCALE_MINOR, 0U);

    expect_pitches(&config, expected);
}



static void test_major_pentatonic_spans_three_octaves(void)
{
    static const uint8_t expected[NOTE_COUNT] =
    {
        0U, 2U, 4U, 7U, 9U, 12U, 14U, 16U, 19U, 21U, 24U, 26U, 28U, 31U, 33U
    };
    const note_map_config_t config = make_config(NOTE_MAP_SCALE_MAJOR_PENTATONIC, 0U);

    expect_pitches(&config, expected);
}



static void test_minor_pentatonic_spans_three_octaves(void)
{
    static const uint8_t expected[NOTE_COUNT] =
    {
        0U, 3U, 5U, 7U, 10U, 12U, 15U, 17U, 19U, 22U, 24U, 27U, 29U, 31U, 34U
    };
    const note_map_config_t config = make_config(NOTE_MAP_SCALE_MINOR_PENTATONIC, 0U);

    expect_pitches(&config, expected);
}



static void test_root_moves_every_note_up_by_the_same_amount(void)
{
    for (int scale = 0; scale < (int)NOTE_MAP_SCALE_COUNT; scale++)
    {
        const note_map_config_t at_zero = make_config((note_map_scale_t)scale, 0U);

        for (uint8_t root = 0U; root <= ROOT_MAX; root++)
        {
            const note_map_config_t config = make_config((note_map_scale_t)scale, root);

            for (uint8_t note = 1U; note <= NOTE_COUNT; note++)
            {
                uint8_t unmoved = POISON_BYTE;
                uint8_t moved   = POISON_BYTE;

                (void)note_map_semitone(&at_zero, note, &unmoved);
                TEST_ASSERT(note_map_semitone(&config, note, &moved));
                TEST_ASSERT_EQUAL(unmoved + root, moved);
            }
        }
    }
}



static void test_root_beyond_an_octave_wraps(void)
{
    const note_map_config_t config   = make_config(NOTE_MAP_SCALE_MAJOR, 14U); // As 2
    uint8_t                 semitone = POISON_BYTE;

    TEST_ASSERT(note_map_semitone(&config, 1U, &semitone));
    TEST_ASSERT_EQUAL(2, semitone);
    TEST_ASSERT(note_map_semitone(&config, 3U, &semitone));
    TEST_ASSERT_EQUAL(6, semitone);
}



static void test_highest_pitch_is_the_documented_maximum(void)
{
    unsigned int highest = 0U;

    for (int scale = 0; scale < (int)NOTE_MAP_SCALE_COUNT; scale++)
    {
        const note_map_config_t config =
            make_config((note_map_scale_t)scale, (uint8_t)ROOT_MAX);
        uint8_t semitone = 0U;

        (void)note_map_semitone(&config, (uint8_t)NOTE_MAP_NOTE_MAX, &semitone);
        if (semitone > highest)
        {
            highest = semitone;
        }
    }
    TEST_ASSERT_EQUAL(SEMITONE_MAX, highest);
}



static void test_note_above_the_maximum_plays_as_the_maximum(void)
{
    const note_map_config_t config   = make_config(NOTE_MAP_SCALE_MINOR_PENTATONIC, 11U);
    uint8_t                 semitone = POISON_BYTE;

    TEST_ASSERT(note_map_semitone(&config, 16U, &semitone));
    TEST_ASSERT_EQUAL(SEMITONE_MAX, semitone);
    TEST_ASSERT(note_map_semitone(&config, 255U, &semitone));
    TEST_ASSERT_EQUAL(SEMITONE_MAX, semitone);
}



static void test_unknown_scale_is_chromatic(void)
{
    static const uint8_t expected[NOTE_COUNT] =
    {
        0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U
    };
    note_map_config_t config = make_config(NOTE_MAP_SCALE_COUNT, 0U);

    expect_pitches(&config, expected);
    config.scale = (note_map_scale_t)99;
    expect_pitches(&config, expected);
}



static void test_null_pointers_are_ignored(void)
{
    const note_map_config_t config   = make_config(NOTE_MAP_SCALE_MAJOR, 0U);
    uint8_t                 semitone = POISON_BYTE;

    TEST_ASSERT(!note_map_semitone(NULL, 5U, &semitone));
    TEST_ASSERT_EQUAL(POISON_BYTE, semitone); // Not written
    TEST_ASSERT(!note_map_semitone(&config, 5U, NULL));
}



int main(void)
{
    RUN_TEST(test_note_zero_is_a_rest_in_every_scale);
    RUN_TEST(test_chromatic_is_one_semitone_a_note);
    RUN_TEST(test_major_scale_spans_two_octaves);
    RUN_TEST(test_minor_scale_spans_two_octaves);
    RUN_TEST(test_major_pentatonic_spans_three_octaves);
    RUN_TEST(test_minor_pentatonic_spans_three_octaves);
    RUN_TEST(test_root_moves_every_note_up_by_the_same_amount);
    RUN_TEST(test_root_beyond_an_octave_wraps);
    RUN_TEST(test_highest_pitch_is_the_documented_maximum);
    RUN_TEST(test_note_above_the_maximum_plays_as_the_maximum);
    RUN_TEST(test_unknown_scale_is_chromatic);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}
