/* SPDX-License-Identifier: GPL-3.0-only */
/* A panel (DESIGN.md 11.3): the **surface** slab in its **edge** frame,
 * lifted off what is behind it, a heading over a rule, and the button hints
 * at its foot. The menus, a question in tiles, the list and Player 2's
 * panel are built on it; each lays out its own heading and rows. Part of the
 * design system (src/ds/, DESIGN.md "Where each part lives"). */
#ifndef ZYPEROS_DS_PANEL_H
#define ZYPEROS_DS_PANEL_H

#include <SDL.h>
#include <stdbool.h>

#include "menumotion.h"  /* mm_pose */

/* How a panel is drawn at a pose other than rest, scaled or faded as it
 * opens and closes: the caller's pass (src/main.c's panel_pass_begin and
 * panel_pass_end, with panel_scaled). `begin` false: drawn straight to the
 * screen. */
typedef struct {
	SDL_FRect (*scaled)(const SDL_Rect *panel, float scale);
	bool (*begin)(void *ctx);
	void (*end)(void *ctx, const SDL_Rect *panel, mm_pose pose);
	void *ctx;
} ds_pass;

/* The slab at rest: lifted (elevation.panel) in `lift_rgb` - a system's
 * color, or CFG_NO_COLOR for a panel that is no system's - where `lift`,
 * then **surface** in its **edge** frame, radius.panel */
void ds_panel_slab(SDL_Renderer *r, const SDL_Rect *panel, unsigned lift_rgb, bool lift);

/* The slab at `pose`: at rest as ds_panel_slab; posed, the lift at the
 * pose's size and strength on the screen, then the slab into the caller's
 * pass. True when the pass is on: end it with ds_panel_close. */
bool ds_panel_open(SDL_Renderer *r, const SDL_Rect *panel, unsigned lift_rgb, mm_pose pose,
                   const ds_pass *pass);
void ds_panel_close(const SDL_Rect *panel, mm_pose pose, bool posed, const ds_pass *pass);

/* A heading's rule, DS_RULE high, `w` long at (x, y), in `c`: **edge**, or a
 * system's mark (ui_sys_mark) in its own panel */
void ds_panel_rule(SDL_Renderer *r, int x, int y, int w, SDL_Color c);

/* The legend centred between x0 and x1 on `cy`: packed button hints
 * (keyhints.h), or a line of **meta** **text-dim** */
void ds_panel_legend(SDL_Renderer *r, const char *legend, int x0, int x1, int cy);

#endif
