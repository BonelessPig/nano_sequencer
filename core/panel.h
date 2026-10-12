#ifndef PANEL_H
#define PANEL_H
/**
 * @file   panel.h
 * @brief  The front panel as the core sees it: 16 steps of 4 switches each,
 *         packed into raw bytes, and after them one byte of 8 mode
 *         switches and one that holds the selector. Every engine reads the
 *         same step bits and gives them its own meaning. Pure logic with no hardware access; compiles
 *         unchanged for the MCU and for a PC.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>

#define PANEL_STEP_COUNT     (16U) // Number of steps on the panel
#define PANEL_VALUE_BITS     (4U)  // Switches per step; must evenly divide 8
#define PANEL_BITS_PER_BYTE  (8U)

// Raw bytes needed to hold every step's bits (8 with the values above)
#define PANEL_STEP_BYTE_COUNT \
    ((PANEL_STEP_COUNT * PANEL_VALUE_BITS) / PANEL_BITS_PER_BYTE)

// The whole panel as raw bytes: the steps' bytes, then the mode switches'
// one, then the selector's
#define PANEL_RAW_BYTE_COUNT (PANEL_STEP_BYTE_COUNT + 2U)

#define PANEL_VALUE_MAX      ((1U << PANEL_VALUE_BITS) - 1U) // Largest step value (15)

/**
 * @brief  Extracts one step's value from the packed raw bytes. The bits are
 *         packed MSB first: step 0 is the high nibble of byte 0, step 1 the
 *         low nibble of byte 0, step 2 the high nibble of byte 1, and so on.
 * @param  p_raw_steps  The panel's raw bytes, PANEL_RAW_BYTE_COUNT of them.
 * @param  step         Step index, 0 to PANEL_STEP_COUNT - 1. A larger value
 *                      wraps round (16 is step 0).
 * @return The step's value, 0 to PANEL_VALUE_MAX. 0 if p_raw_steps is null.
 */
uint8_t panel_step_value(const uint8_t *p_raw_steps, uint8_t step);

/**
 * @brief  Gives the eight mode switches as they are read: the byte that
 *         follows the steps' bytes, each bit one switch, 1 being on. The
 *         low three choose the engine (see seq.h) and the other five are
 *         the chosen engine's own.
 * @param  p_raw_steps  The panel's raw bytes, PANEL_RAW_BYTE_COUNT of them.
 * @return The mode switches, 0 to 255. 0 if p_raw_steps is null.
 */
uint8_t panel_mode_value(const uint8_t *p_raw_steps);

/**
 * @brief  Gives the position of the selector, a coded rotary switch: the
 *         low three bits of the byte that follows the mode switches', as a
 *         number. The next bit up is the profile button, and the byte's
 *         other four bits have nothing on them.
 * @param  p_raw_steps  The panel's raw bytes, PANEL_RAW_BYTE_COUNT of them.
 * @return The position, 0 to 7. 0 if p_raw_steps is null.
 */
uint8_t panel_selector_value(const uint8_t *p_raw_steps);

/**
 * @brief  Gives the profile button as it is read, bounce and all: bit 3 of
 *         the selector's byte, 1 being down.
 * @param  p_raw_steps  The panel's raw bytes, PANEL_RAW_BYTE_COUNT of them.
 * @return true if the button reads as down. false if p_raw_steps is null.
 */
bool panel_profile_button(const uint8_t *p_raw_steps);

#endif /* PANEL_H */
