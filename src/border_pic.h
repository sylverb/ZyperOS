/* SPDX-License-Identifier: GPL-3.0-only */
/* The Border's pictures and what Diatom is told (src/border.h): the player's
 * own picture found on the card, made the panel's size once, written as the
 * DTOV file Diatom reads (its overlay's), and the SETBORDER line for a
 * system's kept choice. */
#ifndef ZYPEROS_BORDER_PIC_H
#define ZYPEROS_BORDER_PIC_H

#include <SDL.h>
#include <stdbool.h>
#include <stddef.h>

#include "app.h"
#include "border.h"

/* The file Diatom is handed for a picture */
#define BORDER_FILE "/tmp/zyperos-border.dtov"

/* The player's own picture for shelf system `s`: the first PNG by name in
 * Borders/<its folder>/ on the card, else in NextUI's Overlays/<its tag>/ or
 * Overlays/<NextUI's tag>/ (border_nextui_tag). False, `out` empty, for none.
 * A few directory reads: when a menu is built or a game starts, never in a
 * frame. */
bool border_own_path(const system_cfg *s, char *out, size_t n);

/* Whether this device's Diatom draws Blur: where it draws every frame on its
 * GPU (the Pixel 2), not where the CPU paints the pages (the Brick for now) */
bool border_blur_here(void);

/* What system `sys`'s row offers now: Blur where the device draws it, the
 * Handheld frames where one is drawn for it (src/handframe.h) and its
 * display mode is Sharp Pixels, Your Own where the card has a picture */
border_offer border_offer_for(const app *a, int sys);

/* System `sys`'s kept kind, as its row shows it (border_shown) */
border_kind border_load(const app *a, int sys);

/* Tell the game running what to draw around it: SETBORDER for system
 * `sys`'s kept kind, the player's picture made the surface's size and
 * written to BORDER_FILE first. Off, or a picture that cannot be read,
 * is black (SETBORDER with nothing). */
void border_send(app *a, int sys);

/* The player's own picture for `s` at the surface's size, for a list's
 * picture: made once and kept while its file is the same (the list's
 * picture draws it many times). NULL for none; `path` its file, "" none. */
SDL_Surface *border_own_cached(const system_cfg *s, char *path, size_t n);

/* The Handheld Color kept for system `sys` (border_color.TAG), an index
 * into its model's colors, 0 (the first) when none is kept or the system
 * has no frame */
int border_color_load(const app *a, int sys);

/* A model's color `i` in the language that is on ("Yellow"), by its word */
const char *border_color_label(const char *word);

/* Handheld Color's row for system `sys`: its value while Border is one of
 * the Handheld frames and the frame comes in more than one color (the Game
 * Boy Color's six); NULL, no row, else */
const char *border_color_value(const app *a, int sys);

/* Handheld Color stepped round the model's colors, kept for the system */
void border_color_step(const app *a, int sys, int d);

/* A handheld's frame for system `sys`, `kind` BORDER_HANDHELD or
 * BORDER_WHOLE, in color `color`, at the surface's size: a new surface the
 * caller frees and the game's rect in it (`gx`...), or NULL where the system
 * has no frame or the game does not fit */
SDL_Surface *border_frame_surface(const app *a, int sys, border_kind kind, int color,
                                  int *gx, int *gy, int *gw, int *gh);

/* The same frame for a list's picture, in color `color` (-1: the kept one),
 * kept while it is the same frame (its system, kind and color, named in
 * `key`), the game's rect in `at` */
SDL_Surface *border_frame_cached(const app *a, int sys, border_kind kind, int color, char *key, size_t n,
                                 SDL_Rect *at);

/* The picture at `png` cut to the shape `w` x `h` from its middle and made
 * that size, smoothly, opaque: a surface the caller frees, or NULL */
SDL_Surface *border_own_surface(const char *png, int w, int h);

#endif
