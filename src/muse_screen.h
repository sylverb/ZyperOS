/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_MUSE_SCREEN_H
#define ZYPEROS_MUSE_SCREEN_H

/* Muse (DESIGN.md 11.21): the covers asked for and loaded, the play mode,
 * what Muse is open over, an album's tracks, Muse's shelf, Album Art, and
 * album covers sent from the web page. The screens are src/muse_screen.c;
 * Now Playing is src/nowplaying_screen.c. */
#include "app.h"
#include "panel.h"

/* What main.c uses of it; each is described where it is defined */
bool cover_file(int al, char *out, size_t n);
void cover_recheck(int al);
void cover_shape(SDL_Surface **ps);
void muse_card_want(app *a, int s, int i);
void muse_shelf_screen(app *a, bool now);
char *albums_wanted(size_t *len, char *why, size_t wn);
char *album_pick(int id, const char *json, size_t n, size_t *len,
                 char *why, size_t wn);
void xfer_after_write(const char *abs);

#endif
