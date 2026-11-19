/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_RUMBLE_H
#define ZYPEROS_RUMBLE_H

#include <stdbool.h>
#include <stddef.h>

/* How hard the motor runs: MENU > Sound > Rumble for every game, and the
 * in-game Controls tab's Rumble row for a game or its system (Applies To).
 * Four levels, a share of what a game asks: Off is none, Low 35%, Medium
 * 65%, Full what the game asks, as ZyperOS always ran it (my picks in the
 * Sound lab, 2026-11-19). Full on a new card, so nothing changes for a
 * player who never touches the row.
 *
 * The level multiplies the system's own share (systems.cfg's rumble| lines:
 * Doom's 35%), and the product goes to Diatom as RUN rumble= (and SETRUMBLE
 * while a game is paused), which hands the port a 0 to 1 scale
 * (diatom_port_rumble): the Pixel 2's force feedback runs at the strength
 * times it, the Brick's motor takes a voltage for it.
 *
 * Kept on the card:
 *
 *     rumble                  the device database: MENU's level, absent Full
 *     rumble.<TAG>            the library: a system's own level
 *     rumble.<TAG>.<file>     the library: a game's own level
 *
 * A game falls back to its system's level, and a system to MENU's; a key
 * never holds the level it falls back to, so choosing it removes the key.
 * Rename and Delete Game move and remove a game's key with its file
 * (src/gamern.c, src/gamedel.c).
 *
 * The levels and the product are pure; the store reads and writes the two
 * databases (src/db.h) and nothing else: no SDL. */

typedef enum { RUMBLE_OFF, RUMBLE_LOW, RUMBLE_MEDIUM, RUMBLE_FULL, RUMBLE_LEVELS } rumble_level;

#define RUMBLE_DEFAULT RUMBLE_FULL

/* The step's buzz as a level is chosen, in milliseconds: the lab's */
#define RUMBLE_BUZZ_MS 160

/* The level's share of what a game asks, in percent: 0, 35, 65, 100 */
int rumble_level_pct(rumble_level l);
/* The word the databases keep: "off", "low", "medium", "full" */
const char *rumble_level_id(rumble_level l);
/* `id` as a level; `fallback` for NULL or a word this release cannot read */
rumble_level rumble_level_parse(const char *id, rumble_level fallback);
/* The level's name in the language that is on: Off, Low, Medium, Full */
const char *rumble_level_name(rumble_level l);
/* What the level does, for its list's note, into `out` */
void rumble_level_note(rumble_level l, char *out, size_t n);
/* `l` stepped by `dir` (+1 Right, -1 Left), stopping at Off and Full */
rumble_level rumble_level_step(rumble_level l, int dir);

/* The share of what a core asks that reaches the motor, in percent, 0 to
 * 100: the level's times the system's (`share`, 1 to 99, or 0 for none,
 * which is all of it). A level above Off never rounds to nothing. */
int rumble_run_pct(int share, rumble_level l);

/* MENU's level, from the device database */
rumble_level rumble_menu(void);
void rumble_menu_set(rumble_level l);

/* The scope's own level: the system's (`file` NULL or "") or a game's.
 * False, and *out untouched, where it has none. */
bool rumble_own(const char *tag, const char *file, rumble_level *out);
/* What the scope falls back to without a level of its own: a game to its
 * system's (or MENU's), a system to MENU's. The in-game list marks it
 * Default. */
rumble_level rumble_fallback(const char *tag, const char *file);
/* The scope's level: its own, else what it falls back to */
rumble_level rumble_scope(const char *tag, const char *file);
/* Keeps `l` for the scope; the key goes when `l` is what it falls back to */
void rumble_put(const char *tag, const char *file, rumble_level l);

#endif
