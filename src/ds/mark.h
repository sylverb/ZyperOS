/* SPDX-License-Identifier: GPL-3.0-only */
/* Marks that stand for a state: a badge (a disc with a mark in it), a switch
 * as a mark (lit or a quiet ring), and the progress bar. Part of the design
 * system (src/ds/, DESIGN.md "Where each part lives"). Colors are the theme's
 * roles, asked for as they are drawn. */
#ifndef ZYPEROS_DS_MARK_H
#define ZYPEROS_DS_MARK_H

#include <SDL.h>
#include <stdbool.h>

#include "ds/glyph.h"

/* A disc `d` across centred on (cx, cy), in a DS_BADGE_RING **edge** ring,
 * with `g` in it at 64% of `d`: `lime`, the **accent** with **on-accent** on
 * it (on, a video); else **surface** with **accent-ink** (a source). `al`
 * its opacity. The Scraper's covers and marks (DESIGN.md 11.19). */
void ds_badge(SDL_Renderer *r, int cx, int cy, int d, ui_glyph g, bool lime, Uint8 al);

/* A switch as a mark: on, the lime badge; off, a quiet ring of **edge** on
 * **raised** with `g` at 60% in **text-dim** */
void ds_switch(SDL_Renderer *r, int cx, int cy, int d, ui_glyph g, bool on);

/* A cover's play mark, that it has a video snap: the **accent** disc `d`
 * across with its top left at (x, y), a play triangle in **on-accent** in
 * it, its centroid on the disc's middle (UI_GLYPH_PLAY is drawn so); at
 * `al`. On the shelf, the list, Cover and Video, and a paused video. */
void ds_play_disc(SDL_Renderer *r, int x, int y, int d, Uint8 al);

/* More rows above or below a list (DESIGN.md 11.3): a triangle DS_MORE_W
 * wide and DS_MORE_H tall, centred on cx, its top at `top`, pointing up or
 * down the way the list goes on; **text-dim** as a rule, `c` */
void ds_more(SDL_Renderer *r, float cx, float top, bool up, SDL_Color c);

/* The progress bar, UI_BAR_H (DS_BAR) high, `w` long at (x, y): **track**,
 * and `fill` of it from the left in **accent-ink** where fill > 0, at `al`
 * (ds_bar_in's rule for a small one). */
void ds_bar(SDL_Renderer *r, int x, int y, int w, int fill, Uint8 al);

/* A bar of any size and colors, for one that says more than progress (the
 * low battery's **warning**, a paused track's faded fill, System Update's
 * tall one): `track` in `track_c`, and `fill` of its width from the left in
 * `fill_c` where fill > 0, both with round ends (half the height). A fill
 * that has started is never shorter than its own round end, the bar's
 * height: a dot, not a sliver, and not nothing (every bar's one rule since
 * 1.0.6; each had its own). */
void ds_bar_in(SDL_Renderer *r, const SDL_Rect *track, int fill, SDL_Color track_c, SDL_Color fill_c);

#endif
