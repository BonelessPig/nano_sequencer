#ifndef PITCH_CAL_H
#define PITCH_CAL_H
/**
 * @file   pitch_cal.h
 * @brief  Pitch calibration: turns a pitch in semitones into the value to
 *         send to the pitch CV output, through a table with one entry per
 *         octave. Pure logic with no hardware access; compiles unchanged for
 *         the MCU and for a PC.
 * @author BonelessPig
 *
 * The table holds the CV value that gives each octave of the lowest pitch:
 * entry 0 for semitone 0, entry 1 for semitone 12, and so on up to entry 4
 * for semitone 48. A pitch between two entries is placed on the straight
 * line between them, rounded to the nearest value. An ideal output stage has
 * evenly spaced entries; a real one is measured octave by octave, and the
 * table takes up its errors.
 *
 * A CV value is a number from 0 to PITCH_CAL_CV_MAX. What it means in volts
 * is up to whatever the app sends it to.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

#define PITCH_CAL_POINT_COUNT (5U)    // Table entries: semitones 0, 12, 24, 36, 48
#define PITCH_CAL_CV_MAX      (4095U) // The highest CV value (12 bits)

/**
 * @brief The calibration table, read when a step begins.
 */
typedef struct
{
    // CV value of each octave, lowest first. An entry above PITCH_CAL_CV_MAX
    // is treated as PITCH_CAL_CV_MAX. The entries need not rise
    uint16_t cv[PITCH_CAL_POINT_COUNT];
} pitch_cal_config_t;

/**
 * @brief  Gives the CV value of a pitch. Reads *p_config and has no other
 *         effect.
 * @param  p_config  The calibration table.
 * @param  semitone  Pitch in semitones above the lowest, 0 to 48. A higher
 *                   pitch is treated as 48, the last entry of the table.
 * @return The CV value, 0 to PITCH_CAL_CV_MAX; 0 if p_config is null.
 */
uint16_t pitch_cal_cv(const pitch_cal_config_t *p_config, uint8_t semitone);

#endif /* PITCH_CAL_H */
