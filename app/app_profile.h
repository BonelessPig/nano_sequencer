#ifndef APP_PROFILE_H
#define APP_PROFILE_H
/**
 * @file   app_profile.h
 * @brief  The application's handling of the profiles: eight stored sets of
 *         the bank's amounts, worked by one button. Part of the app, split
 *         from app.c by topic: it wires the profile button and the profile
 *         record of the core to the non-volatile storage and the log.
 * @author BonelessPig
 *
 * A profile holds every slot of the bank but slot 0, the tempo's, which
 * stays where the pot is. The profiles are numbered 0 to 7, as the selector's
 * positions are, and are kept one record after another from address 64 of
 * the storage.
 *
 * The status light is here too. It is lit while the profile the selector
 * stands at holds something (its record's first byte is not blank; whether
 * the rest is good is found at a recall), winks as a hold of the button passes 1 s, 5 s
 * and 10 s, flickers once for a recall, twice for a save, three times for a
 * clear and four for a clear of every profile, and gives one long blink
 * when a recall found nothing or a save could not be started. The power-on
 * recall says nothing.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include "bank.h"

/**
 * @brief  Puts the profile handling in its starting condition: the button
 *         not held, nothing waiting to be read or cleared, and the power-on
 *         recall still to be asked.
 * @note   Not ISR-safe. Touches no port.
 */
void app_profile_init(void);

/**
 * @brief  The apply step's part: does what the button asked for on the
 *         previous tick, of the profile the selector stood at, and carries
 *         on with a recall or any clearing still to be done. A recall is
 *         read when the storage is free and its amounts put in the bank; a
 *         save starts the write of the record; a clear writes one byte a
 *         profile, one profile a tick. Each is logged, and so is a failure.
 *         It writes the status light first, on every call, and ahead of
 *         the power-on recall reads one byte of one profile a tick, for
 *         eight ticks, to learn which hold anything.
 * @param  p_bank  The bank whose amounts are saved and recalled. Must not
 *                 be null.
 * @note   Not ISR-safe. Does not block: a write is only started (see
 *         nv_port.h).
 */
void app_profile_apply(bank_state_t *p_bank);

/**
 * @brief  The core step's part: follows the profile button and keeps what
 *         it asked for, and the profile the selector stood at, for the next
 *         tick's app_profile_apply(). The first valid panel read after
 *         app_profile_init() asks for a recall by itself. Works out the
 *         status light for the next tick too.
 * @param  p_raw_steps    The panel's raw bytes, as last read. Must not be
 *                        null.
 * @param  b_steps_valid  false if the panel could not be read.
 * @param  elapsed_ticks  Ticks (milliseconds) since the previous tick, 1 to
 *                        255.
 * @note   Not ISR-safe. Touches no port.
 */
void app_profile_run(const uint8_t *p_raw_steps, bool b_steps_valid, uint8_t elapsed_ticks);

#endif /* APP_PROFILE_H */
