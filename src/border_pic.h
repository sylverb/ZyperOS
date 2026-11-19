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

/* The picture at `png` cut to the shape `w` x `h` from its middle and made
 * that size, smoothly, opaque: a surface the caller frees, or NULL */
SDL_Surface *border_own_surface(const char *png, int w, int h);

#endif
