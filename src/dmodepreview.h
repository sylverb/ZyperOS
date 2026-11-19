/* SPDX-License-Identifier: GPL-3.0-only */
/* The Display list's picture (menu_pick_preview, DESIGN.md 11.3): the
 * handheld's screen in small, and a game's frame on it as display mode `k`
 * would put it (src/dmodefit.h) - bars where the mode leaves the screen
 * bare, the edges gone where it crops, the shape it gives the picture - so
 * a mode's words have a picture beside them before it is chosen.
 *
 * In a game, the paused frame the in-game menu is over, at the size and
 * shape Diatom said (plat_resident_source). On a shelf, the game under the
 * cursor's last frame (its Auto picture) with the shape its system's games
 * last said, kept for it (dmode_note_shape); a game never played has none,
 * and a frame drawn for the purpose stands in. */
#ifndef ZYPEROS_DMODEPREVIEW_H
#define ZYPEROS_DMODEPREVIEW_H

#include <SDL.h>
#include <stdbool.h>

#include "app.h"

/* The picture for mode `k` in the box x, y, w x h (units): `frame` is the
 * game's picture, `src_w` x `src_h` as its core makes it with the shape
 * `aspect` (0 for square pixels), on Diatom's surface `surf_w` x `surf_h`;
 * NULL draws the stand-in */
void dmode_preview_draw(app *a, SDL_Texture *frame, int src_w, int src_h, double aspect,
                        int surf_w, int surf_h, int k, int x, int y, int w, int h);

/* The Super Game Boy list's picture (src/sgbmode.h) for choice `k` (Off,
 * Its Games, Every Game), at display mode `dmode` on this handheld's
 * screen, sized as Diatom would size it: a Game Boy game's 160x144 alone
 * for Off, the Super Game Boy's 256x224 with the game in its middle for the
 * other two, its border a stand-in of a game's own (Its Games) or the plain
 * frame in the game's lightest shade (Every Game). Drawn frames, ours: no
 * game's border and no Nintendo art. */
void dmode_sgb_preview(app *a, int dmode, int k, int x, int y, int w, int h);

/* The surface Diatom draws on, as the last DISPLAY said, or the device's
 * panel before one did */
void dmode_surface(int *w, int *h);

/* Keeps the shape the game running says (plat_resident_source) for its
 * system `tag`, for the shelf's list; and reads it back, 0 when none was
 * kept (square pixels) and `known` false */
void dmode_note_shape(const char *tag);
double dmode_shape_of(const char *tag, bool *known);

#endif
