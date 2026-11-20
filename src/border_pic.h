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
 * GPU, as the profile's gpu_shaders says (the Pixel 2, a Brick test kit's GPU
 * path), not where the CPU paints the pages (the Brick for now) */
bool border_blur_here(void);

/* Whether system `sys`'s display mode leaves room around its games, by the
 * size they last said (dmode_source_of; a system never played, by the mode
 * alone): the system menu's question. A game's Display tab asks it of
 * Diatom's own size instead. */
bool border_room_kept(const app *a, int sys);

/* What system `sys`'s row offers (border_offer_make), `room` whether its
 * display mode leaves room around the game: Blur and Your Own only then, the
 * Handheld frames where one is drawn for it (src/handframe.h) in any mode */
border_offer border_offer_room(const app *a, int sys, bool room);

/* The same, the room by border_room_kept */
border_offer border_offer_for(const app *a, int sys);

/* System `sys`'s kept kind, as its row shows it with `room` (border_shown,
 * and a frame Off at any mode but Sharp Pixels: border_in_mode) */
border_kind border_load_room(const app *a, int sys, bool room);

/* The same, the room by border_room_kept */
border_kind border_load(const app *a, int sys);

/* Border stepped by `d` among what system `sys`'s row offers with `room`,
 * and kept; a frame chosen turns the system's Display to Sharp Pixels, the
 * size it is drawn for. True when that turned Display: the caller keeps it
 * (display_save, main.c's, which this file does not link to, so the checks
 * that link it alone still do) and tells a game running its new mode before
 * its border. */
bool border_choose(app *a, int sys, int d, bool room);

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

/* The frame Handheld Color's list pictures each color in: Handheld, Whole
 * wherever it is offered, since Handheld at its full size can show the
 * lens alone and no body (the Game Boy Color's at 5x on the Brick), else
 * the kind kept */
border_kind border_color_kind(const app *a, int sys);

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
