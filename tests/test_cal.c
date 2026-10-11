/**
 * @file   test_cal.c
 * @brief  Host unit tests for calibration mode (core/cal.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "cal.h"
#include "panel.h"
#include "test_harness.h"

#define STEP_COUNT      (16U)
#define RAW_BYTES       (8U)
#define POINT_STEP      (0U)   // Chooses the point
#define STORE_STEP      (15U)  // Its lowest switch stores
#define STORE_ON        (0x5U) // Step 15 as the entry pattern leaves it
#define STORE_OFF       (0x4U) // The same with the store switch off
#define TRIM_CENTRE     (512U) // Tempo reading that gives the nominal value
#define TRIM_MAX        (1023U)
#define CV_PER_OCTAVE   (1000U)
#define CONFIRM_TICKS   (250U) // The gate is low this long after a store
#define UNTOUCHED_ENTRY (0x5A5AU)
#define UNTOUCHED_BYTE  (0xA5U)

static cal_state_t        g_state;
static cal_inputs_t       g_in;
static cal_outputs_t      g_out;
static pitch_cal_config_t g_table;
static uint8_t            g_steps[STEP_COUNT]; // The value of each step
static uint8_t            g_raw[RAW_BYTES];    // The same, packed

/**
 * @brief Packs g_steps into g_raw as the panel does: step 0 in the high
 *        nibble of byte 0.
 */
static void pack_steps(void)
{
    for (size_t byte = 0U; byte < RAW_BYTES; byte++)
    {
        g_raw[byte] = (uint8_t)((g_steps[byte * 2U] << 4U) | g_steps[(byte * 2U) + 1U]);
    }
}

/**
 * @brief A fresh state, every table entry marked, and inputs that read: the
 *        entry pattern on the last four steps, point 0, the trim at its
 *        middle, one tick elapsed.
 */
static void start(void)
{
    (void)memset(g_steps, 0, sizeof(g_steps));
    g_steps[12] = 0xAU;
    g_steps[13] = 0x5U;
    g_steps[14] = 0xAU;
    g_steps[15] = STORE_ON;

    g_in.p_raw_steps   = g_raw;
    g_in.b_steps_valid = true;
    g_in.tempo_raw     = TRIM_CENTRE;
    g_in.b_tempo_valid = true;
    g_in.elapsed_ticks = 1U;
    g_in.cv_per_octave = CV_PER_OCTAVE;

    for (size_t point = 0U; point < PITCH_CAL_POINT_COUNT; point++)
    {
        g_table.cv[point] = UNTOUCHED_ENTRY;
    }
    (void)memset(&g_out, UNTOUCHED_BYTE, sizeof(g_out));
    cal_init(&g_state);
}

/**
 * @brief One tick with the steps as g_steps now holds them.
 */
static void tick(void)
{
    pack_steps();
    cal_tick(&g_state, &g_in, &g_table, &g_out);
}

/**
 * @brief One tick with the given point and tempo reading; returns the CV.
 */
static uint16_t cv_for(uint8_t point_value, uint16_t tempo_raw)
{
    g_steps[POINT_STEP] = point_value;
    g_in.tempo_raw      = tempo_raw;
    tick();
    return g_out.cv;
}

/**
 * @brief Switches the store switch off and on again, a tick each.
 */
static void flip_store(void)
{
    g_steps[STORE_STEP] = STORE_OFF;
    tick();
    g_steps[STORE_STEP] = STORE_ON;
    tick();
}

/**
 * @brief Checks that no table entry has been written.
 */
static void expect_table_untouched(void)
{
    for (size_t point = 0U; point < PITCH_CAL_POINT_COUNT; point++)
    {
        TEST_ASSERT_EQUAL(UNTOUCHED_ENTRY, g_table.cv[point]);
    }
}



static void test_an_ordinary_panel_is_not_calibration_mode(void)
{
    start();
    g_steps[12] = 0x3U; // Any pattern but the entry one
    tick();

    TEST_ASSERT(!g_out.b_active);
    TEST_ASSERT_EQUAL(0, g_out.cv);
    TEST_ASSERT(!g_out.b_gate);
    TEST_ASSERT(!g_out.b_stored);
    TEST_ASSERT_EQUAL(0, g_out.point);

    // The mode was chosen on the first tick: the pattern later changes nothing
    g_steps[12] = 0xAU;
    for (unsigned int i = 0U; i < 20U; i++)
    {
        tick();
        TEST_ASSERT(!g_out.b_active);
    }
    flip_store();
    TEST_ASSERT(!g_out.b_stored);
    expect_table_untouched();
}



static void test_the_entry_pattern_on_the_first_tick_enters_the_mode(void)
{
    start();
    tick();
    TEST_ASSERT(g_out.b_active);
    TEST_ASSERT(g_out.b_gate);
    TEST_ASSERT(!g_out.b_stored); // The store switch was on already

    // And it stays, whatever the panel reads afterwards
    g_steps[12] = 0x0U;
    g_steps[13] = 0x0U;
    g_steps[14] = 0x0U;
    for (unsigned int i = 0U; i < 20U; i++)
    {
        tick();
        TEST_ASSERT(g_out.b_active);
    }
}



static void test_every_one_of_the_four_entry_steps_has_to_match(void)
{
    static const uint8_t pattern[4] = { 0xAU, 0x5U, 0xAU, 0x5U };

    for (size_t i = 0U; i < 4U; i++)
    {
        for (uint8_t value = 0U; value <= PANEL_VALUE_MAX; value++)
        {
            start();
            g_steps[12U + i] = value;
            tick();
            TEST_ASSERT_EQUAL(value == pattern[i], g_out.b_active);
        }
    }

    // The other twelve steps are free
    start();
    for (size_t step = 0U; step < 12U; step++)
    {
        g_steps[step] = 0xFU;
    }
    tick();
    TEST_ASSERT(g_out.b_active);
}



static void test_a_panel_that_cannot_be_read_plays_as_normal(void)
{
    start();
    g_in.b_steps_valid = false;
    tick();
    TEST_ASSERT(!g_out.b_active);

    // Readable from the next tick on, with the pattern: too late
    g_in.b_steps_valid = true;
    tick();
    TEST_ASSERT(!g_out.b_active);

    // No buffer at all is the same as an unreadable one
    start();
    pack_steps();
    g_in.p_raw_steps = NULL;
    cal_tick(&g_state, &g_in, &g_table, &g_out);
    TEST_ASSERT(!g_out.b_active);
}



static void test_step_0_chooses_the_point_at_its_nominal_value(void)
{
    start();
    for (uint8_t point = 0U; point < PITCH_CAL_POINT_COUNT; point++)
    {
        TEST_ASSERT_EQUAL(point * CV_PER_OCTAVE, cv_for(point, TRIM_CENTRE));
    }

    // A value above the last point is the last point
    for (uint8_t value = PITCH_CAL_POINT_COUNT; value <= PANEL_VALUE_MAX; value++)
    {
        TEST_ASSERT_EQUAL(4000, cv_for(value, TRIM_CENTRE));
    }
}



static void test_the_tempo_control_trims_one_value_for_every_eight_counts(void)
{
    start();
    TEST_ASSERT_EQUAL(2000, cv_for(2U, 512U));
    TEST_ASSERT_EQUAL(2000, cv_for(2U, 519U));
    TEST_ASSERT_EQUAL(2001, cv_for(2U, 520U));
    TEST_ASSERT_EQUAL(1999, cv_for(2U, 511U));
    TEST_ASSERT_EQUAL(1999, cv_for(2U, 504U));
    TEST_ASSERT_EQUAL(1998, cv_for(2U, 503U));
    TEST_ASSERT_EQUAL(1936, cv_for(2U, 0U));        // 64 below
    TEST_ASSERT_EQUAL(2063, cv_for(2U, TRIM_MAX));  // 63 above
    TEST_ASSERT_EQUAL(4063, cv_for(4U, TRIM_MAX));  // The highest it gives

    // A reading beyond the control's range is its top
    TEST_ASSERT_EQUAL(2063, cv_for(2U, 1024U));
    TEST_ASSERT_EQUAL(2063, cv_for(2U, UINT16_MAX));
}



static void test_the_lowest_point_cannot_be_trimmed_below_zero(void)
{
    start();
    TEST_ASSERT_EQUAL(0, cv_for(0U, 0U));
    TEST_ASSERT_EQUAL(0, cv_for(0U, 511U));
    TEST_ASSERT_EQUAL(0, cv_for(0U, 512U));
    TEST_ASSERT_EQUAL(1, cv_for(0U, 520U));
    TEST_ASSERT_EQUAL(63, cv_for(0U, TRIM_MAX));
}



static void test_the_trim_is_at_its_middle_until_read_and_holds_through_a_failure(void)
{
    start();
    g_in.b_tempo_valid = false;
    TEST_ASSERT_EQUAL(3000, cv_for(3U, 0U)); // The reading is not to be used

    g_in.b_tempo_valid = true;
    TEST_ASSERT_EQUAL(3010, cv_for(3U, 592U));

    g_in.b_tempo_valid = false;
    TEST_ASSERT_EQUAL(3010, cv_for(3U, 0U));
    TEST_ASSERT_EQUAL(1010, cv_for(1U, 0U)); // The point still follows the panel
}



static void test_the_nominal_octave_is_an_input_and_the_value_is_limited(void)
{
    start();
    g_in.cv_per_octave = 800U;
    TEST_ASSERT_EQUAL(2400, cv_for(3U, TRIM_CENTRE));
    g_in.cv_per_octave = 0U;
    TEST_ASSERT_EQUAL(10, cv_for(3U, 592U));

    // Four octaves of 1200 would be 4800
    g_in.cv_per_octave = 1200U;
    TEST_ASSERT_EQUAL(3600, cv_for(3U, TRIM_CENTRE));
    TEST_ASSERT_EQUAL(PITCH_CAL_CV_MAX, cv_for(4U, TRIM_CENTRE));

    // An octave above the whole range is treated as the whole range, so
    // nothing overflows
    g_in.cv_per_octave = UINT16_MAX;
    TEST_ASSERT_EQUAL(PITCH_CAL_CV_MAX, cv_for(1U, TRIM_CENTRE));
    TEST_ASSERT_EQUAL(PITCH_CAL_CV_MAX - 64U, cv_for(1U, 0U));
    TEST_ASSERT_EQUAL(PITCH_CAL_CV_MAX, cv_for(4U, TRIM_MAX));
}



static void test_switching_the_store_switch_on_stores_the_value_given(void)
{
    start();
    (void)cv_for(2U, 616U); // 13 above nominal
    TEST_ASSERT(!g_out.b_stored);

    g_steps[STORE_STEP] = STORE_OFF;
    tick();
    TEST_ASSERT(!g_out.b_stored);
    expect_table_untouched();

    g_steps[STORE_STEP] = STORE_ON;
    tick();
    TEST_ASSERT(g_out.b_stored);
    TEST_ASSERT_EQUAL(2, g_out.point);
    TEST_ASSERT_EQUAL(2013, g_out.cv);
    TEST_ASSERT_EQUAL(2013, g_table.cv[2]);

    // Only that entry
    TEST_ASSERT_EQUAL(UNTOUCHED_ENTRY, g_table.cv[0]);
    TEST_ASSERT_EQUAL(UNTOUCHED_ENTRY, g_table.cv[1]);
    TEST_ASSERT_EQUAL(UNTOUCHED_ENTRY, g_table.cv[3]);
    TEST_ASSERT_EQUAL(UNTOUCHED_ENTRY, g_table.cv[4]);

    // Once: the switch staying on stores nothing more, and the output still
    // follows the controls
    TEST_ASSERT_EQUAL(2020, cv_for(2U, 672U));
    TEST_ASSERT(!g_out.b_stored);
    TEST_ASSERT_EQUAL(0, g_out.point);
    TEST_ASSERT_EQUAL(2013, g_table.cv[2]);
}



static void test_each_point_is_stored_on_its_own(void)
{
    static const uint16_t trims[PITCH_CAL_POINT_COUNT] = { 560U, 472U, 512U, 0U, 1023U };
    static const uint16_t wanted[PITCH_CAL_POINT_COUNT] = { 6U, 995U, 2000U, 2936U, 4063U };

    start();
    for (uint8_t point = 0U; point < PITCH_CAL_POINT_COUNT; point++)
    {
        (void)cv_for(point, trims[point]);
        flip_store();
        TEST_ASSERT(g_out.b_stored);
        TEST_ASSERT_EQUAL(point, g_out.point);
    }
    for (size_t point = 0U; point < PITCH_CAL_POINT_COUNT; point++)
    {
        TEST_ASSERT_EQUAL(wanted[point], g_table.cv[point]);
    }

    // Only the lowest switch of step 15 is the store switch
    g_steps[STORE_STEP] = 0x1U;
    tick();
    g_steps[STORE_STEP] = 0xFU;
    tick();
    TEST_ASSERT(!g_out.b_stored);
    g_steps[STORE_STEP] = 0xEU;
    tick();
    g_steps[STORE_STEP] = 0x1U;
    tick();
    TEST_ASSERT(g_out.b_stored);
}



static void test_nothing_is_stored_while_the_panel_cannot_be_read(void)
{
    start();
    (void)cv_for(1U, TRIM_CENTRE);
    g_steps[STORE_STEP] = STORE_OFF;
    tick();

    // The switch goes on, but the read fails: the point and the switch stay
    // as they were last seen
    g_steps[STORE_STEP]  = STORE_ON;
    g_steps[POINT_STEP]  = 3U;
    g_in.b_steps_valid   = false;
    tick();
    TEST_ASSERT(g_out.b_active);
    TEST_ASSERT(!g_out.b_stored);
    TEST_ASSERT_EQUAL(1000, g_out.cv);
    expect_table_untouched();

    // Seen once the panel reads again
    g_in.b_steps_valid = true;
    tick();
    TEST_ASSERT(g_out.b_stored);
    TEST_ASSERT_EQUAL(3, g_out.point);
    TEST_ASSERT_EQUAL(3000, g_table.cv[3]);
}



static void test_the_gate_goes_low_for_a_quarter_second_after_a_store(void)
{
    start();
    tick();
    TEST_ASSERT(g_out.b_gate);

    flip_store();
    TEST_ASSERT(!g_out.b_gate); // From the tick of the store

    for (unsigned int i = 1U; i < CONFIRM_TICKS; i++)
    {
        tick();
        TEST_ASSERT(!g_out.b_gate);
    }
    tick();
    TEST_ASSERT(g_out.b_gate);
    tick();
    TEST_ASSERT(g_out.b_gate);
}



static void test_the_gate_is_timed_in_elapsed_ticks(void)
{
    start();
    tick();
    flip_store();

    g_in.elapsed_ticks = 100U;
    tick();
    TEST_ASSERT(!g_out.b_gate); // 150 left
    tick();
    TEST_ASSERT(!g_out.b_gate); // 50 left
    tick();
    TEST_ASSERT(g_out.b_gate);  // More than enough

    // A second store while the gate is still low starts the time again
    g_in.elapsed_ticks = 1U;
    flip_store();
    g_in.elapsed_ticks = 200U;
    tick();
    TEST_ASSERT(!g_out.b_gate);
    g_in.elapsed_ticks = 1U;
    flip_store();
    g_in.elapsed_ticks = 200U;
    tick();
    TEST_ASSERT(!g_out.b_gate); // 50 left of the second
    g_in.elapsed_ticks = 50U;
    tick();
    TEST_ASSERT(g_out.b_gate);
}



static void test_init_chooses_the_mode_again(void)
{
    start();
    tick();
    (void)cv_for(2U, 616U);
    flip_store();
    TEST_ASSERT(g_out.b_active);

    // An ordinary panel after a new init: out of the mode
    cal_init(&g_state);
    g_steps[15] = 0x0U;
    tick();
    TEST_ASSERT(!g_out.b_active);

    // And back in, with the trim at its middle and the gate high again
    cal_init(&g_state);
    g_steps[15]        = STORE_ON;
    g_in.b_tempo_valid = false;
    tick();
    TEST_ASSERT(g_out.b_active);
    TEST_ASSERT(g_out.b_gate);
    TEST_ASSERT_EQUAL(2000, g_out.cv);
}



static void test_null_pointers_do_nothing(void)
{
    cal_state_t before;

    cal_init(NULL);

    start();
    pack_steps();
    before = g_state;

    cal_tick(NULL, &g_in, &g_table, &g_out);
    cal_tick(&g_state, NULL, &g_table, &g_out);
    cal_tick(&g_state, &g_in, NULL, &g_out);
    cal_tick(&g_state, &g_in, &g_table, NULL);

    TEST_ASSERT_EQUAL(0, memcmp(&before, &g_state, sizeof(before)));
    TEST_ASSERT_EQUAL(UNTOUCHED_BYTE, *(const uint8_t *)&g_out);
    expect_table_untouched();

    // The state was not spent: the mode is still to be chosen
    tick();
    TEST_ASSERT(g_out.b_active);
}



int main(void)
{
    RUN_TEST(test_an_ordinary_panel_is_not_calibration_mode);
    RUN_TEST(test_the_entry_pattern_on_the_first_tick_enters_the_mode);
    RUN_TEST(test_every_one_of_the_four_entry_steps_has_to_match);
    RUN_TEST(test_a_panel_that_cannot_be_read_plays_as_normal);
    RUN_TEST(test_step_0_chooses_the_point_at_its_nominal_value);
    RUN_TEST(test_the_tempo_control_trims_one_value_for_every_eight_counts);
    RUN_TEST(test_the_lowest_point_cannot_be_trimmed_below_zero);
    RUN_TEST(test_the_trim_is_at_its_middle_until_read_and_holds_through_a_failure);
    RUN_TEST(test_the_nominal_octave_is_an_input_and_the_value_is_limited);
    RUN_TEST(test_switching_the_store_switch_on_stores_the_value_given);
    RUN_TEST(test_each_point_is_stored_on_its_own);
    RUN_TEST(test_nothing_is_stored_while_the_panel_cannot_be_read);
    RUN_TEST(test_the_gate_goes_low_for_a_quarter_second_after_a_store);
    RUN_TEST(test_the_gate_is_timed_in_elapsed_ticks);
    RUN_TEST(test_init_chooses_the_mode_again);
    RUN_TEST(test_null_pointers_do_nothing);
    return TEST_RESULT();
}
