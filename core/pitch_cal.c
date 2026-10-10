/**
 * @file   pitch_cal.c
 * @brief  Pitch calibration implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "pitch_cal.h"
#include <stddef.h>

#define SEMITONES_PER_POINT (12U) // The table has an entry every octave
#define ROUNDING            (SEMITONES_PER_POINT / 2U)
#define LAST_POINT          (PITCH_CAL_POINT_COUNT - 1U)



/**
 * @brief  Reads one entry of the table, limited to the CV range.
 * @param  p_config  The calibration table; not null.
 * @param  point     Entry to read, 0 to LAST_POINT.
 * @return The entry, 0 to PITCH_CAL_CV_MAX.
 */
static uint16_t point_cv(const pitch_cal_config_t *p_config, uint8_t point)
{
    uint16_t cv = p_config->cv[point];

    if (cv > PITCH_CAL_CV_MAX)
    {
        cv = PITCH_CAL_CV_MAX;
    }

    return cv;
}



uint16_t pitch_cal_cv(const pitch_cal_config_t *p_config, uint8_t semitone)
{
    uint16_t cv = 0U;

    if (NULL != p_config)
    {
        const uint8_t point = (uint8_t)(semitone / SEMITONES_PER_POINT);

        if (point >= LAST_POINT)
        {
            cv = point_cv(p_config, (uint8_t)LAST_POINT);
        }
        else
        {
            // Twelfths of the way from this entry to the next. Weighting the
            // two entries works whichever is the larger, and at most
            // 12 * PITCH_CAL_CV_MAX + 6 = 49146 fits in 16 bits
            const uint8_t  above  = (uint8_t)(semitone % SEMITONES_PER_POINT);
            const uint8_t  below  = (uint8_t)(SEMITONES_PER_POINT - above);
            const uint16_t lower  = point_cv(p_config, point);
            const uint16_t higher = point_cv(p_config, (uint8_t)(point + 1U));
            const uint16_t weighted =
                (uint16_t)((uint16_t)(lower * below) + (uint16_t)(higher * above));

            cv = (uint16_t)((uint16_t)(weighted + ROUNDING) / SEMITONES_PER_POINT);
        }
    }

    return cv;
}
