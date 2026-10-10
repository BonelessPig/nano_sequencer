#ifndef NOTE_MAP_H
#define NOTE_MAP_H
/**
 * @file   note_map.h
 * @brief  Note mapping: turns a step's note value into a pitch in semitones,
 *         through a scale and a root. Pure logic with no hardware access;
 *         compiles unchanged for the MCU and for a PC.
 * @author BonelessPig
 *
 * A note value of 0 is a rest. Values 1 to 15 are the first 15 notes of the
 * scale, counting up from the root and carrying on into the next octaves:
 * in a major scale 1 is the root, 8 the root an octave up and 15 the root two
 * octaves up.
 *
 * A pitch is a count of semitones above the lowest pitch the sequencer can
 * play, which is note value 1 with a root of 0. The highest any setting gives
 * is 45: note value 15 in the minor pentatonic scale is two octaves and ten
 * semitones above the root, and the root adds up to 11.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>

#define NOTE_MAP_REST                 (0U)  // The note value that plays nothing
#define NOTE_MAP_NOTE_MAX             (15U) // The highest note value
#define NOTE_MAP_SEMITONES_PER_OCTAVE (12U)

/**
 * @brief The scales a note value can be read in.
 */
typedef enum
{
    NOTE_MAP_SCALE_CHROMATIC = 0,    // Every semitone
    NOTE_MAP_SCALE_MAJOR,            // 0 2 4 5 7 9 11
    NOTE_MAP_SCALE_MINOR,            // Natural minor: 0 2 3 5 7 8 10
    NOTE_MAP_SCALE_MAJOR_PENTATONIC, // 0 2 4 7 9
    NOTE_MAP_SCALE_MINOR_PENTATONIC, // 0 3 5 7 10
    NOTE_MAP_SCALE_COUNT             // Number of scales; not a scale
} note_map_scale_t;

/**
 * @brief The note mapping controls.
 */
typedef struct
{
    note_map_scale_t scale; // An unknown value is treated as chromatic
    uint8_t          root;  // Semitones the scale starts above the lowest
                            // pitch, 0 to 11; larger values wrap (12 is 0)
} note_map_config_t;

/**
 * @brief  Gives the pitch of a note value. Reads *p_config, writes
 *         *p_semitone, and has no other effect.
 * @param  p_config    The controls as they are now.
 * @param  note        Note value: NOTE_MAP_REST, or 1 to NOTE_MAP_NOTE_MAX.
 *                     A larger value is treated as NOTE_MAP_NOTE_MAX.
 * @param  p_semitone  Receives the pitch in semitones, 0 to 45; 0 for a
 *                     rest. Not written if a pointer is null.
 * @return true if the note sounds; false if it is a rest, or if either
 *         pointer is null.
 */
bool note_map_semitone(const note_map_config_t *p_config, uint8_t note,
                       uint8_t *p_semitone);

#endif /* NOTE_MAP_H */
