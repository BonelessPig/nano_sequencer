#ifndef MOD_CONFIG_H
#define MOD_CONFIG_H
/**
 * @file   mod_config.h
 * @brief  The numbers a board chooses for its modulation: the shape of the
 *         modulating signal, and how far it moves each thing it can move.
 *         Another board takes mod.c and mod.h as they are and writes this
 *         file for itself. Constants only; it includes nothing.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */

// The signal (see mod.h). A reading and an amount are both this many bits
// wide, 9 to 15: on this board 10, so 0 to 1023 with the centre at 512
#define MOD_CONFIG_READING_BITS (10U)

// A swing of less than this either side of the centre is taken as none, so
// that an input with nothing plugged into it moves nothing, noise and all
#define MOD_CONFIG_DEAD_ZONE (16U)

// What the signal moves on this board, read by seq.c alone, which is where
// the names from seq.h and note_map.h below have their meaning. For each: how
// far it is moved at the signal's fullest, and the least and the most it may
// then be. Each is 0 to 255.

// A note: by this many places in the scale, and never to a rest
#define MOD_CONFIG_NOTE_RANGE (7U)
#define MOD_CONFIG_NOTE_MIN   (1U)
#define MOD_CONFIG_NOTE_MAX   (NOTE_MAP_NOTE_MAX)

// A gate's length, in clock pulses: by half a step, kept to at least one
// pulse and at most a whole step, which ties
#define MOD_CONFIG_GATE_RANGE (SEQ_PULSES_PER_STEP / 2U)
#define MOD_CONFIG_GATE_MIN   (1U)
#define MOD_CONFIG_GATE_MAX   (SEQ_PULSES_PER_STEP)

#endif /* MOD_CONFIG_H */
