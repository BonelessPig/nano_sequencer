/**
 * @file   app_profile.c
 * @brief  The application's handling of the profiles (see app_profile.h).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "app_profile.h"
#include "blink.h"
#include "panel.h"
#include "profile.h"
#include "profile_record.h"
#include "log_port.h"
#include "nv_port.h"
#include "status_output_port.h"

// Where the profiles are kept in the non-volatile storage, one record after
// another, clear of the calibration record and the crash log
#define PROFILE_ADDRESS (64U)

// The first of the bank's slots a profile holds: every one but slot 0, the
// tempo's, which stays where the pot is
#define PROFILE_FIRST_SLOT (1U)

// The profiles are numbered as the bank's slots are, by the selector, 0 to
// 7. One bit for each, in the set of those still to be cleared
#define ALL_PROFILES  (0xFFU)
#define PROFILE_COUNT (8U)

static profile_state_t  g_profile_state;                 // The profile button's state
static profile_action_t g_profile_action = PROFILE_NONE; // What it asked on the last tick
static uint8_t          g_profile        = 0U;           // The profile it asked it of
static uint8_t          g_clear_pending  = 0U;           // Profiles to clear, a bit each
static bool             g_b_recall_due   = false;        // true while a recall waits
static uint8_t          g_recall_profile = 0U;           // The profile it is to read
static bool             g_b_recall_quiet = false;        // true if it is the power-on one

// The status light. It is lit while the selected profile holds something,
// and flickers as many times as the number of what was done: once for a
// recall, twice for a save, three times for a clear and four for a clear
// of every profile, the numbers the log gives them
static blink_state_t g_blink_state;                 // The light's state
static bool          g_b_light      = false;        // The light, as the last tick left it
static uint8_t       g_flickers     = 0U;           // Flickers to begin, 0 for none
static bool          g_b_refused    = false;        // true to begin the long blink
static uint8_t       g_held         = 0U;           // Profiles that hold something, a bit each
static uint8_t       g_scan_profile = PROFILE_COUNT; // The next to look at; 8 when done



/**
 * @brief  Gives where a profile's record is kept.
 * @param  profile  Which profile, 0 to 7.
 * @return Its address in the non-volatile storage.
 */
static uint16_t profile_address(uint8_t profile)
{
    // At most 64 + 7 * 16
    return (uint16_t)(PROFILE_ADDRESS + ((uint16_t)profile * PROFILE_RECORD_SIZE));
}



/**
 * @brief  Gives a profile's bit in a set of them.
 * @param  profile  Which profile, 0 to 7.
 * @return The bit.
 */
static uint8_t profile_bit(uint8_t profile)
{
    return (uint8_t)(1U << profile);
}



/**
 * @brief  Has the light say how a recall ended, unless it was the power-on
 *         one, which the button did not ask for.
 * @param  b_recalled  true if the profile's amounts were put in use.
 */
static void show_recall(bool b_recalled)
{
    if (!g_b_recall_quiet)
    {
        if (b_recalled)
        {
            g_flickers = (uint8_t)PROFILE_RECALL;
        }
        else
        {
            g_b_refused = true;
        }
    }
}



/**
 * @brief  Carries out the recall that is waiting, if the storage can be
 *         read: puts the stored profile's amounts in use, if the profile
 *         holds any, and logs it. One that holds nothing changes nothing
 *         and is not an error. While the storage is busy with a write
 *         (the crash log's at start-up, say) the recall goes on waiting;
 *         any other failure is logged and the recall is given up. The
 *         light is told how it ended.
 * @param  p_bank  The bank that takes the amounts.
 */
static void recall_due_profile(bank_state_t *p_bank)
{
    uint8_t             record[PROFILE_RECORD_SIZE];
    const port_status_t status =
        nv_read(profile_address(g_recall_profile), record, (uint8_t)PROFILE_RECORD_SIZE);

    if (ERR_BUSY == status)
    {
        // Tried again on the next tick
    }
    else if (STATUS_OK != status)
    {
        g_b_recall_due = false;
        log_error(LOG_ERROR_PROFILE_READ, status);
        show_recall(false);
    }
    else
    {
        uint16_t   amounts[PROFILE_AMOUNT_COUNT];
        const bool b_found = profile_record_decode(record, amounts);

        g_b_recall_due = false;
        show_recall(b_found);
        if (b_found)
        {
            for (uint8_t i = 0U; i < PROFILE_AMOUNT_COUNT; i++)
            {
                bank_set(p_bank, (uint8_t)(PROFILE_FIRST_SLOT + i), amounts[i]);
            }
            log_profile(g_recall_profile, (uint8_t)PROFILE_RECALL);
        }
    }
}



/**
 * @brief  Stores the amounts in use as a profile: starts the write of its
 *         record and logs it, or the failure to start.
 * @param  p_bank   The bank whose amounts are stored.
 * @param  profile  Which profile, 0 to 7.
 */
static void save_profile(const bank_state_t *p_bank, uint8_t profile)
{
    uint8_t       record[PROFILE_RECORD_SIZE];
    uint16_t      amounts[PROFILE_AMOUNT_COUNT];
    port_status_t status;

    for (uint8_t i = 0U; i < PROFILE_AMOUNT_COUNT; i++)
    {
        amounts[i] = bank_amount(p_bank, (uint8_t)(PROFILE_FIRST_SLOT + i));
    }
    profile_record_encode(amounts, record);
    status = nv_write(profile_address(profile), record, (uint8_t)PROFILE_RECORD_SIZE);

    if (STATUS_OK == status)
    {
        g_held     = (uint8_t)(g_held | profile_bit(profile));
        g_flickers = (uint8_t)PROFILE_SAVE;
        log_profile(profile, (uint8_t)PROFILE_SAVE);
    }
    else
    {
        g_b_refused = true;
        log_error(LOG_ERROR_PROFILE_WRITE, status);
    }
}



/**
 * @brief  Looks at the next profile of those not yet looked at since
 *         power-on, if the storage can be read, and notes whether it holds
 *         anything, for the light. Only its first byte is read, which a
 *         clear makes blank: a whole record read and checked takes a tenth
 *         of a millisecond, and this next to nothing. While the storage is
 *         busy with a write the profile goes on waiting; any other failure
 *         is logged and the profile taken as empty.
 */
static void scan_next_profile(void)
{
    uint8_t             first  = PROFILE_RECORD_BLANK;
    const port_status_t status = nv_read(profile_address(g_scan_profile), &first, 1U);

    if (ERR_BUSY != status)
    {
        if (STATUS_OK != status)
        {
            log_error(LOG_ERROR_PROFILE_READ, status);
        }
        else if (PROFILE_RECORD_BLANK != first)
        {
            g_held = (uint8_t)(g_held | profile_bit(g_scan_profile));
        }
        else
        {
            // Empty
        }
        g_scan_profile++;
    }
}



/**
 * @brief  Clears one of the profiles waiting to be cleared, the lowest, if
 *         the storage will take the write: one byte, which makes its
 *         record no record. While the storage is busy with an earlier
 *         write the profile goes on waiting; any other failure is logged
 *         and the profile is given up.
 */
static void clear_pending_profile(void)
{
    if (0U != g_clear_pending)
    {
        const uint8_t blank   = PROFILE_RECORD_BLANK;
        uint8_t       profile = 0U;
        uint8_t       bit     = 1U;
        port_status_t status;

        // One is waiting, so this stops at it, within the eight
        while (0U == (g_clear_pending & bit))
        {
            profile++;
            bit = (uint8_t)(bit << 1U);
        }

        status = nv_write(profile_address(profile), &blank, 1U);
        if (ERR_BUSY != status)
        {
            g_clear_pending = (uint8_t)(g_clear_pending & (uint8_t)(~bit));
            if (STATUS_OK != status)
            {
                log_error(LOG_ERROR_PROFILE_WRITE, status);
            }
        }
    }
}



void app_profile_init(void)
{
    profile_init(&g_profile_state);
    g_profile_action = PROFILE_NONE;
    g_profile        = 0U;
    g_clear_pending  = 0U;
    g_b_recall_due   = false;
    g_recall_profile = 0U;
    g_b_recall_quiet = false;

    blink_init(&g_blink_state);
    g_b_light      = false;
    g_flickers     = 0U;
    g_b_refused    = false;
    g_held       = 0U;
    g_scan_profile = PROFILE_COUNT;
}



void app_profile_apply(bank_state_t *p_bank)
{
    status_output_write(g_b_light);

    switch (g_profile_action)
    {
        case PROFILE_RECALL:
            g_b_recall_due   = true;
            g_b_recall_quiet = false;
            g_recall_profile = g_profile;
            break;

        case PROFILE_LOAD:
            // Power-on: a look at every profile, and then the recall
            g_b_recall_due   = true;
            g_b_recall_quiet = true;
            g_recall_profile = g_profile;
            g_scan_profile   = 0U;
            break;

        case PROFILE_SAVE:
            save_profile(p_bank, g_profile);
            break;

        case PROFILE_CLEAR:
            g_clear_pending = (uint8_t)(g_clear_pending | profile_bit(g_profile));
            g_held          = (uint8_t)(g_held & (uint8_t)(~profile_bit(g_profile)));
            g_flickers      = (uint8_t)PROFILE_CLEAR;
            log_profile(g_profile, (uint8_t)PROFILE_CLEAR);
            break;

        case PROFILE_CLEAR_ALL:
            g_clear_pending = ALL_PROFILES;
            g_held          = 0U;
            g_flickers      = (uint8_t)PROFILE_CLEAR_ALL;
            log_profile(g_profile, (uint8_t)PROFILE_CLEAR_ALL);
            break;

        default:
            // PROFILE_NONE: the button asked for nothing
            break;
    }

    // One read of the storage a tick at most. The look at the profiles
    // goes first, a byte a tick for eight ticks, which puts the power-on
    // recall, a whole record, clear of the first step: its tick is the
    // longest there is, and its MIDI bytes are on their way out for two
    // more
    if (g_scan_profile < PROFILE_COUNT)
    {
        scan_next_profile();
    }
    else if (g_b_recall_due)
    {
        recall_due_profile(p_bank);
    }
    else
    {
        // Nothing to read
    }
    clear_pending_profile();
}



void app_profile_run(const uint8_t *p_raw_steps, bool b_steps_valid, uint8_t elapsed_ticks)
{
    profile_inputs_t profile_in;
    blink_inputs_t   blink_in;

    profile_in.b_button       = panel_profile_button(p_raw_steps);
    profile_in.b_button_valid = b_steps_valid;
    profile_in.elapsed_ticks  = elapsed_ticks;

    g_profile_action = profile_tick(&g_profile_state, &profile_in);
    g_profile        = panel_selector_value(p_raw_steps);

    blink_in.b_on          = (0U != (g_held & profile_bit(g_profile)));
    blink_in.flickers      = g_flickers;
    blink_in.b_refused     = g_b_refused;
    blink_in.hold_stage    = profile_hold_stage(&g_profile_state);
    blink_in.elapsed_ticks = elapsed_ticks;

    g_b_light   = blink_tick(&g_blink_state, &blink_in);
    g_flickers  = 0U;
    g_b_refused = false;
}
