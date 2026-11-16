/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_CARDART_DRAW_H
#define ZYPEROS_CARDART_DRAW_H

/* The pixels of the card of a game with no art (src/cardart.h): its ground,
 * a frame of play in its place, and the band its name sits on. On ARGB8888
 * surfaces, from any thread: nothing here touches a renderer, a font or the
 * launcher's state, so the art workers draw it (src/texload.c). The name
 * itself is set by src/ui.c, in the launcher's faces. */

#include <SDL.h>
#include <stdbool.h>

#include "cardart.h"

/* The ground over all of `s`: the gradient, the era's texture, the hash's
 * shape and light, a vignette, and `art` (the system's still image, or NULL)
 * as a faint light silhouette bled off a corner */
void cardart_paint_ground(SDL_Surface *s, unsigned sys_rgb, ca_texture tex,
                          const ca_ground *g, SDL_Surface *art);

/* A frame of play over all of `s`: its middle at the card's shape, blurred,
 * dimmed and turned a little toward the system's color, darker toward the
 * foot. False, and `s` untouched, for a frame it cannot use. */
bool cardart_paint_frame(SDL_Surface *s, SDL_Surface *frame, unsigned sys_rgb);

/* The name's band: rows `y0` to `y1` darkened to a little under half,
 * feathered `feather` rows above and below */
void cardart_paint_band(SDL_Surface *s, int y0, int y1, int feather);

#endif
