#ifndef APP_FIXTURE_H
#define APP_FIXTURE_H
/**
 * @file   app_fixture.h
 * @brief  What the test_app programs share: the panel they start from and
 *         the helpers that start the app and run its loop.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "app.h"
#include "cal_record.h"
#include "host_ports.h"
#include "midi_output_port.h"
#include "seq.h"
#include "test_harness.h"

#define TEMPO_RAW_150_BPM (480U)  // A step every 100 ticks
#define TEMPO_RAW_MAX     (1023U) // 285 BPM: 19 steps every 1000 ticks
#define TICKS_PER_STEP    (100U)  // At TEMPO_RAW_150_BPM
#define SCAN_PERIOD_TICKS (8U)    // How often the app re-reads its inputs
#define CAL_TRIM_CENTRE   (512U)  // Tempo reading that trims nothing in calibration mode
#define CAL_CONFIRM_TICKS (250U)  // The gate is low this long after a store
#define SETTLE_TICKS      (20U)   // Debounce time of the run button
#define SYNC_RAW_ONE_TO_ONE (500U) // Tempo reading that is a step to an external edge
#define CRASH_LOG_ADDRESS    (16U)   // Where the app keeps the crash log
#define CRASH_RECORD_BYTES   (5U)
#define RESET_CAUSE_WATCHDOG (0x08U)
#define PLACE_START_UP       (1U)    // The breadcrumb before the first tick
#define PLACE_BETWEEN_TICKS  (5U)    // And once a tick has finished
#define MIDI_CLOCK           (0xF8U)
#define MIDI_START           (0xFAU)
#define MIDI_STOP            (0xFCU)

// Step n holds note value 15 - n, so the step and the note are never the same
// number. In the chromatic scale the app plays, that is pitch 14 - n, and
// step 15 (note value 0) is a rest
static const uint8_t g_descending_notes[HOST_RAW_STEP_BYTES] =
{
    0xFEU, 0xDCU, 0xBAU, 0x98U, 0x76U, 0x54U, 0x32U, 0x10U
};

/**
 * @brief Checks that the recorded event at index is the expected output-port call.
 */
static inline void expect_event(uint16_t index, host_event_kind_t kind, uint16_t a, uint16_t b)
{
    const host_event_t *p_event = host_event_get(index);

    TEST_ASSERT(NULL != p_event);
    if (NULL != p_event)
    {
        TEST_ASSERT_EQUAL(kind, p_event->kind);
        TEST_ASSERT_EQUAL(a, p_event->a);
        TEST_ASSERT_EQUAL(b, p_event->b);
    }
}



/**
 * @brief Fresh fakes and a freshly initialized app, with the init not counted
 *        in the event record. The panel holds the descending notes and the
 *        tempo control is at 150 BPM.
 */
static inline void start_app(void)
{
    host_reset();
    (void)app_init();
    host_set_steps(g_descending_notes, STATUS_OK);
    host_set_tempo(TEMPO_RAW_150_BPM, STATUS_OK);
}



/**
 * @brief The same without clearing the fakes first: the app starts again as
 *        after a power cycle, with the non-volatile storage as it was left.
 */
static inline void start_app_keeping_storage(void)
{
    (void)app_init();
    host_set_steps(g_descending_notes, STATUS_OK);
    host_set_tempo(TEMPO_RAW_150_BPM, STATUS_OK);
}



/**
 * @brief Sets the panel as calibration mode uses it: the entry pattern on
 *        the last four steps, with the store switch (the lowest of step 15)
 *        on or off, and the point to set on step 0.
 */
static inline void set_calibration_panel(uint8_t point, bool b_store_on)
{
    uint8_t raw[HOST_RAW_STEP_BYTES] =
    {
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xA5U, 0xA4U, 0x00U
    };

    raw[0] = (uint8_t)(point << 4U);
    if (b_store_on)
    {
        raw[7] = 0xA5U;
    }
    host_set_steps(raw, STATUS_OK);
}



/**
 * @brief Fresh fakes and a freshly initialized app whose panel asks for
 *        calibration mode, at the given point and tempo control reading.
 */
static inline void start_calibration(uint8_t point, uint16_t tempo_raw)
{
    host_reset();
    (void)app_init();
    set_calibration_panel(point, true);
    host_set_tempo(tempo_raw, STATUS_OK);
}



/**
 * @brief Runs the main loop for a number of passes, with one tick elapsed
 *        before each.
 */
static inline void run_ticks(unsigned int count)
{
    host_set_elapsed_ticks(1U);
    for (unsigned int i = 0U; i < count; i++)
    {
        app_run_once();
    }
}



/**
 * @brief Sets the panel to the descending notes with the selector at a
 *        position, and the pot to a reading.
 */
static inline void set_selector_and_pot(uint8_t position, uint16_t reading)
{
    uint8_t raw[HOST_RAW_STEP_BYTES];

    (void)memcpy(raw, g_descending_notes, sizeof(raw));
    raw[HOST_RAW_STEP_BYTES - 1U] = position;
    host_set_steps(raw, STATUS_OK);
    host_set_tempo(reading, STATUS_OK);
}



/**
 * @brief Gives one of the bank's amounts a value: the selector to its
 *        position, the pot down to meet the 0 it holds, and up. 20 ticks.
 */
static inline void set_amount(uint8_t position, uint16_t amount)
{
    set_selector_and_pot(position, 0U);
    run_ticks(10U);
    set_selector_and_pot(position, amount);
    run_ticks(10U);
}

#endif /* APP_FIXTURE_H */
