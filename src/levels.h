/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_LEVELS_H
#define ZYPEROS_LEVELS_H

/* The launcher's volume and brightness keys, and the boost past the top of
 * each (src/boost.h, DESIGN.md 11.6): every screen's loop calls levels_keys
 * where it called the four nudges, saying where it is (BOOST_OPEN on MP4's
 * screens, BOOST_FILL on MP4's lock screen, BOOST_HIDE in the menus). The
 * level stays the device's (src/device/: its nudge applies it, keeps it,
 * shows the bar and says it on the side lights, as before); the boost's
 * cells are kept here, in the database beside the level, and become MP4's
 * gain (Muse), a port's (the shared file portkeys and the plugin read) and
 * the gamma table's lift. Diatom keeps its own while a game runs and hands
 * it back (src/resident.c). */

#include <stdbool.h>

#include "boost.h"
#include "osdbar.h"
#include "platform.h"

/* After plat_settings_init: the boosts kept from before, applied */
void levels_init(void);

/* The level keys this frame: presses and held keys as in_repeat sees them,
 * let-goes and the clock. `where` is BOOST_OPEN, BOOST_FILL or BOOST_HIDE
 * for the volume; brightness opens anywhere, and on MP4's lock screen
 * (FILL), where only the volume keys get through, is left alone. True when
 * a level key did something this frame. */
bool levels_keys(in_state *in, int where);

/* The bar's look for `kind` (OSDBAR_VOLUME, OSDBAR_BRIGHT) now, for
 * plat_draw_osd: false for any other kind */
bool levels_look(int kind, osdbar_look *out);

/* Whether the boost's bar still moves on its own (a charge, an opening, a
 * fold): the bar is drawn every frame until it stops */
bool levels_moving(void);

/* The bar went away: an empty boost closes, a filled one stays */
void levels_bar_gone(void);

/* The output changed (a plug in or out): the volume boost closes, so the
 * new output starts at the top without it (my pick) */
void levels_output_changed(void);

/* Diatom's level and boost for `kind` (OSDBAR_VOLUME, OSDBAR_BRIGHT) at
 * the end of a game or a pause: kept and applied here; the level itself is
 * the caller's to apply (plat_volume_set_pct) */
void levels_from_game(int kind, int level, int boost);

/* The boost cells kept for `kind`, for telling Diatom */
int levels_boost(int kind);

/* A shot of the bar at `level`, with `cells` boost cells open when it is
 * the top (--osd): the device's level is not looked at again */
void levels_shot(int kind, int level, int cells);

#endif
