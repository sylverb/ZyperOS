/* SPDX-License-Identifier: GPL-3.0-only */
/* Where each display mode puts a game's picture, as Diatom puts it
 * (diatom/src/scale.c, diatom_scale_rect), so the Display list can show
 * what a mode does before it is chosen (its preview, src/menu_pick.h). The
 * same arithmetic, written again here because the launcher links nothing of
 * Diatom's; make check-dmodefit holds the two together over every mode and
 * a spread of sizes.
 *
 * `name` is a mode's protocol word (app.h's DMODES[].name); the picture is
 * `src_w` x `src_h` with the shape `aspect` (width over height, 0 or less
 * for square pixels), on a surface `surf_w` x `surf_h`. The rect may reach
 * past the surface (fill, overscale): a caller clips it. Pure. */
#ifndef ZYPEROS_DMODEFIT_H
#define ZYPEROS_DMODEFIT_H

typedef struct { int x, y, w, h; } dmode_rect;

dmode_rect dmode_fit(const char *name, int src_w, int src_h, double aspect,
                     int surf_w, int surf_h);

#endif
