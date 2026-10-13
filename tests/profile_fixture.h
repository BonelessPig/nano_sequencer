#ifndef PROFILE_FIXTURE_H
#define PROFILE_FIXTURE_H
/**
 * @file   profile_fixture.h
 * @brief  What the app loop's tests of the profiles and of the status light
 *         share: where the profiles are kept, how long each hold is, and
 *         helpers that store a profile, press the button and count events.
 *         Include it after app_fixture.h.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <stdint.h>
#include "profile_record.h"

#define PROFILES_ADDRESS (64U)   // Where the app keeps the profiles
#define RECORD_BYTES     (16U)
#define BUTTON_BIT       (0x08U) // The profile button, in the selector's byte
#define CLICK_TICKS      (100U)
#define SAVE_TICKS       (1100U)  // A hold of over a second
#define CLEAR_TICKS      (5100U)  // Of over five
#define CLEAR_ALL_TICKS  (10100U) // And of over ten
#define ACTION_RECALL    (1U)     // As log_profile() is given them
#define ACTION_SAVE      (2U)
#define ACTION_CLEAR     (3U)
#define ACTION_CLEAR_ALL (4U)
#define STORED_VERSION   (1U)
#define ERASED           (0xFFU)

// A profile's seven amounts, for selector positions 1 to 7
static const uint16_t g_stored[PROFILE_AMOUNT_COUNT] =
{
    100U, 200U, 300U, 400U, 500U, 600U, 700U
};

/**
 * @brief Where a profile's record begins in the storage.
 */
static inline uint16_t address_of(uint8_t profile)
{
    return (uint16_t)(PROFILES_ADDRESS + (profile * RECORD_BYTES));
}

/**
 * @brief Puts the stored amounts in the storage as a profile, as if saved
 *        before the power last went off.
 */
static inline void store_profile(uint8_t profile)
{
    uint8_t record[RECORD_BYTES];

    profile_record_encode(g_stored, record);
    host_set_nv_bytes(address_of(profile), record, (uint8_t)sizeof(record));
}

/**
 * @brief Counts the events in the record of one kind with these arguments.
 */
static inline unsigned int count_events(host_event_kind_t kind, uint16_t a, uint16_t b)
{
    unsigned int count = 0U;

    for (uint16_t i = 0U; i < host_event_count(); i++)
    {
        const host_event_t *p_event = host_event_get(i);

        if ((NULL != p_event) && (kind == p_event->kind) && (a == p_event->a) && (b == p_event->b))
        {
            count++;
        }
    }
    return count;
}

/**
 * @brief Counts the events in the record of one kind, whatever they say.
 */
static inline unsigned int count_kind(host_event_kind_t kind)
{
    unsigned int count = 0U;

    for (uint16_t i = 0U; i < host_event_count(); i++)
    {
        const host_event_t *p_event = host_event_get(i);

        if ((NULL != p_event) && (kind == p_event->kind))
        {
            count++;
        }
    }
    return count;
}

/**
 * @brief A started app with the pattern stopped, so that a long hold does
 *        not fill the event record with steps.
 */
static inline void start_stopped(void)
{
    start_app();
    run_ticks(2U);
    host_set_run_button(true);
    run_ticks(SETTLE_TICKS + 5U);
    host_set_run_button(false);
    run_ticks(SETTLE_TICKS + 5U);
}

/**
 * @brief Holds the profile button down for a time with the selector at a
 *        position and the pot at a reading, lets it go, and runs on until
 *        what it asked for has been done.
 */
static inline void press_button(uint8_t position, uint16_t pot, unsigned int hold_ticks)
{
    set_selector_and_pot((uint8_t)(position | BUTTON_BIT), pot);
    run_ticks(hold_ticks);
    set_selector_and_pot(position, pot);
    run_ticks(SETTLE_TICKS + (2U * SCAN_PERIOD_TICKS));
}

#endif /* PROFILE_FIXTURE_H */
