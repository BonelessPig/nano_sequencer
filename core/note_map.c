/**
 * @file   note_map.c
 * @brief  Note mapping implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "note_map.h"
#include <stddef.h>

// Notes in one octave of each scale
#define CHROMATIC_LENGTH  (12U)
#define DIATONIC_LENGTH   (7U)
#define PENTATONIC_LENGTH (5U)

/**
 * @brief One scale: the semitones above the root of each note in an octave.
 */
typedef struct
{
    const uint8_t *p_intervals; // Ascending, starting with 0 for the root
    uint8_t        length;      // Notes in one octave
} scale_t;



/**
 * @brief  Gives how far above the root a note of a scale is.
 * @param  scale   The scale; an unknown value is treated as chromatic.
 * @param  degree  Notes up from the root, 0 to NOTE_MAP_NOTE_MAX - 1; the
 *                 scale repeats an octave higher each time it runs out.
 * @return Semitones above the root, 0 to 34.
 */
static uint8_t semitones_above_root(note_map_scale_t scale, uint8_t degree)
{
    static const uint8_t chromatic[CHROMATIC_LENGTH] =
    {
        0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U
    };
    static const uint8_t major[DIATONIC_LENGTH] = { 0U, 2U, 4U, 5U, 7U, 9U, 11U };
    static const uint8_t minor[DIATONIC_LENGTH] = { 0U, 2U, 3U, 5U, 7U, 8U, 10U };
    static const uint8_t major_pentatonic[PENTATONIC_LENGTH] = { 0U, 2U, 4U, 7U, 9U };
    static const uint8_t minor_pentatonic[PENTATONIC_LENGTH] = { 0U, 3U, 5U, 7U, 10U };

    // Indexed by note_map_scale_t
    static const scale_t scales[NOTE_MAP_SCALE_COUNT] =
    {
        { chromatic,        CHROMATIC_LENGTH  },
        { major,            DIATONIC_LENGTH   },
        { minor,            DIATONIC_LENGTH   },
        { major_pentatonic, PENTATONIC_LENGTH },
        { minor_pentatonic, PENTATONIC_LENGTH }
    };

    const scale_t *p_scale = &scales[NOTE_MAP_SCALE_CHROMATIC];
    uint8_t        octave;

    if (scale < NOTE_MAP_SCALE_COUNT)
    {
        p_scale = &scales[scale];
    }
    octave = (uint8_t)(degree / p_scale->length);

    return (uint8_t)((octave * NOTE_MAP_SEMITONES_PER_OCTAVE)
                     + p_scale->p_intervals[degree % p_scale->length]);
}



bool note_map_semitone(const note_map_config_t *p_config, uint8_t note,
                       uint8_t *p_semitone)
{
    bool b_sounds = false;

    if ((NULL != p_config) && (NULL != p_semitone))
    {
        *p_semitone = 0U;

        if (NOTE_MAP_REST != note)
        {
            const uint8_t root = (uint8_t)(p_config->root % NOTE_MAP_SEMITONES_PER_OCTAVE);
            uint8_t       limited_note = note;

            if (limited_note > NOTE_MAP_NOTE_MAX)
            {
                limited_note = (uint8_t)NOTE_MAP_NOTE_MAX;
            }

            // Note value 1 is the root itself
            *p_semitone = (uint8_t)(root + semitones_above_root(p_config->scale,
                                                               (uint8_t)(limited_note - 1U)));
            b_sounds    = true;
        }
    }

    return b_sounds;
}
