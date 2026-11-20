/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_CHEATS_SCREEN_H
#define ZYPEROS_CHEATS_SCREEN_H

/* Cheats in a game (DESIGN.md 11.66): what the running game has on, handed
 * to Diatom at its start and at each switch, the list the in-game Game tab's
 * Cheats row opens over the paused game, the question the first cheat asks
 * while signed in to RetroAchievements, the pop-up as a game starts with
 * cheats on, and the question after a game that stopped with cheats on.
 * The files and the list are src/cheats.h's; Diatom's side is its cheats.h. */
#include <stdbool.h>
#include <stddef.h>

#include "app.h"

/* The game about to start: `s`'s game `file`. Its cheats that are on are
 * written for Diatom and `cheats=` is put on `keys` (tab separated, `kn`
 * long) for its RUN, unless `without` (the crash question's Without
 * Cheats): then it starts with none, and what is kept for it is left as it
 * was. How many are on. */
int  cheats_game_begin(const system_cfg *s, const char *file, bool without,
                       char *keys, size_t kn);
/* RUNNING: the pop-up "2 cheats on" over the game's first frames, when any
 * are on */
void cheats_game_started(void);
/* The game is over: `crashed`, it stopped by itself while cheats were on,
 * and its next start asks first (cheats_crash_asks) */
void cheats_game_end(bool crashed);

/* How many of the running game's cheats are on now, and whether one has
 * been on since it started (its achievements wait: the Achievements row's
 * Paused) */
int  cheats_game_on(void);
bool cheats_game_paused(void);

/* The game stopped by itself last time with cheats on (cheats_game_end):
 * its start asks "Start it without them?" */
bool cheats_crash_asks(const system_cfg *s, const char *file);

/* The list over the paused game `bg` */
void cheats_page(app *a, SDL_Texture *bg);

/* For a check and the shots: the list's rows for the running game as
 * menu_rows_print says them, and the list drawn over `bg` with row `sel`
 * focused (`ask`: the first cheat's question over it) */
void cheats_rows_report(app *a);
void cheats_page_shot(app *a, SDL_Texture *bg, int sel, bool ask);

/* The shots' running game, with no launch: `s`'s game `file`, its cheats
 * as the card has them, `paused` as if one had been on */
void cheats_shot_game(const system_cfg *s, const char *file, bool paused);

#endif
