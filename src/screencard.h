/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_SCREENCARD_H
#define ZYPEROS_SCREENCARD_H

/* Recents' Last Screen card (DESIGN.md 11.48, Card Picture): the screen the
 * game was left on, its resume point's picture, with its cover small in the
 * card's lower left corner.
 *
 * The picture is the one Diatom writes beside the resume state as the game
 * ends (<rom>.auto.bmp, the core's own frame, 256 x 224 for a SNES game), so
 * nothing new is saved and nothing runs while a game plays. It is made on
 * the art workers (src/texload.h, texload_want_screen), never on the frame:
 * the picture scaled up whole, by the nearest pixel, to about SC_HEIGHT
 * high, and the cover, averaged down, set on it over a soft shadow with
 * round corners.
 *
 * SDL surfaces only, no renderer: any thread (a generated card in its place
 * painted in the workers' own faces, ui_card_paint's `away`). */
#include <SDL.h>
#include <stdbool.h>

#include "ui.h"

/* The height the picture is scaled toward, whole times: 448, a folder card's */
#define SC_HEIGHT 448
/* The cover's height, and its inset from the card's left and bottom edges,
 * in hundredths of the card's height */
#define SC_COVER_SHARE 30
#define SC_INSET_SHARE 4

/* The card for the picture at `frame` and the cover at `cover`, an ARGB8888
 * surface the caller frees; NULL when the picture cannot be read. With no
 * cover file (NULL, "", or one that cannot be read), the game's generated
 * card from `card` in its place, as its shelf shows it (ui_card_paint,
 * `away` on a worker); with no order either, the picture alone. */
SDL_Surface *screen_card_make(const char *frame, const char *cover, const ui_card_order *card,
                              bool away);

/* The same from surfaces in hand, any format; `cover` may be NULL */
SDL_Surface *screen_card_from(SDL_Surface *frame, SDL_Surface *cover);

#endif
