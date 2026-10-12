/**
 * @file   app_profile.c
 * @brief  The application's handling of the profiles (see app_profile.h).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "app_profile.h"
#include "panel.h"
#include "profile.h"
#include "profile_record.h"
#include "log_port.h"
#include "nv_port.h"

// Where the profiles are kept in the non-volatile storage, one record after
// another, clear of the calibration record and the crash log
#define PROFILE_ADDRESS (64U)

// The first of the bank's slots a profile holds: every one but slot 0, the
// tempo's, which stays where the pot is
#define PROFILE_FIRST_SLOT (1U)

// The profiles are numbered as the bank's slots are, by the selector, 0 to
// 7. One bit for each, in the set of those still to be cleared
#define ALL_PROFILES (0xFFU)

static profile_state_t  g_profile_state;                 // The profile button's state
static profile_action_t g_profile_action = PROFILE_NONE; // What it asked on the last tick
static uint8_t          g_profile        = 0U;           // The profile it asked it of
static uint8_t          g_clear_pending  = 0U;           // Profiles to clear, a bit each
static bool             g_b_recall_due   = false;        // true while a recall waits
static uint8_t          g_recall_profile = 0U;           // The profile it is to read



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
 * @brief  Carries out the recall that is waiting, if the storage can be
 *         read: puts the stored profile's amounts in use, if the profile
 *         holds any, and logs it. One that holds nothing changes nothing
 *         and is not an error. While the storage is busy with a write
 *         (the crash log's at start-up, say) the recall goes on waiting;
 *         any other failure is logged and the recall is given up.
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
    }
    else
    {
        uint16_t amounts[PROFILE_AMOUNT_COUNT];

        g_b_recall_due = false;
        if (profile_record_decode(record, amounts))
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
        log_profile(profile, (uint8_t)PROFILE_SAVE);
    }
    else
    {
        log_error(LOG_ERROR_PROFILE_WRITE, status);
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
}



void app_profile_apply(bank_state_t *p_bank)
{
    switch (g_profile_action)
    {
        case PROFILE_RECALL:
            g_b_recall_due   = true;
            g_recall_profile = g_profile;
            break;

        case PROFILE_SAVE:
            save_profile(p_bank, g_profile);
            break;

        case PROFILE_CLEAR:
            g_clear_pending = (uint8_t)(g_clear_pending | (uint8_t)(1U << g_profile));
            log_profile(g_profile, (uint8_t)PROFILE_CLEAR);
            break;

        case PROFILE_CLEAR_ALL:
            g_clear_pending = ALL_PROFILES;
            log_profile(g_profile, (uint8_t)PROFILE_CLEAR_ALL);
            break;

        default:
            // PROFILE_NONE: the button asked for nothing
            break;
    }

    if (g_b_recall_due)
    {
        recall_due_profile(p_bank);
    }
    clear_pending_profile();
}



void app_profile_run(const uint8_t *p_raw_steps, bool b_steps_valid, uint8_t elapsed_ticks)
{
    profile_inputs_t profile_in;

    profile_in.b_button       = panel_profile_button(p_raw_steps);
    profile_in.b_button_valid = b_steps_valid;
    profile_in.elapsed_ticks  = elapsed_ticks;

    g_profile_action = profile_tick(&g_profile_state, &profile_in);
    g_profile        = panel_selector_value(p_raw_steps);
}
