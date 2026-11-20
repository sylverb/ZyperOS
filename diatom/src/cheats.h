/* SPDX-License-Identifier: MPL-2.0 */
/* Cheats: the codes the launcher hands over, and when the core gets them.
 *
 * The launcher reads the .cht files and keeps which cheats are on (ZyperOS's
 * src/cheats.c). Diatom is handed only the codes that are on, one a line, in a
 * file: RUN cheats=<path> at a game's start, SETCHEATS path=<path> at each
 * switch, an empty path for none. A file, as cheevos= is, because one N64
 * GameShark code joined with + runs past what a protocol line holds.
 *
 * The core is handed them as RetroArch hands them (cheat_manager_apply_cheats):
 * retro_cheat_reset, then retro_cheat_set(i, true, code) for each, i counted
 * from 0. That happens once the game is loaded, at each SETCHEATS, and again
 * after a reset and each state load, since a core may keep its codes in what a
 * state replaces. Never in a frame: a cheat costs nothing per frame here, and
 * what a core does with a code is the core's.
 *
 * While any cheat has been on in a game, its achievements stop until the next
 * game (diatom_cheevos_pause): turning them all off again does not undo what
 * they changed in the game's memory.
 *
 * A core is resident (core.c), so the codes it was handed stay in it after
 * the game ends. The next game on it is reset first when it had any, and a
 * core that never had any is never touched, which keeps every game without
 * cheats exactly as it ran before cheats existed. */
#ifndef DIATOM_CHEATS_H
#define DIATOM_CHEATS_H

#include <stdbool.h>

#include "diatom.h"

/* How many codes, and how long one may be. A list longer is cut, with a line
 * in the log; libretro's longest file has 900 cheats, but a player turns on
 * a handful. */
#define DIATOM_CHEATS_MAX      256
#define DIATOM_CHEATS_CODE_MAX 4096

/* Read the codes in `path`, a line each (blank lines skipped), in place of
 * the ones held. NULL or "" is none. -1, and the ones held kept, when the
 * file cannot be read; else how many are held now. */
int  diatom_cheats_read(const char *path);

/* A game's start, once it is loaded: `path` is RUN's cheats= (NULL for none).
 * Hands them to `c`, or resets `c` when it holds an earlier game's, and says
 * whether achievements are paused for this game. -1 as diatom_cheats_read. */
int  diatom_cheats_begin(diatom_core *c, const char *path);

/* Hand the codes held to `c` again: after SETCHEATS, a reset, a state load.
 * With none held, a core that holds some is reset; one that holds none is
 * left alone. */
void diatom_cheats_apply(diatom_core *c);

/* How many codes are held now */
int  diatom_cheats_count(void);

#endif
