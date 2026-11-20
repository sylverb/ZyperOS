/* SPDX-License-Identifier: GPL-3.0-only */
/* Where your PICO-8 draws its 128 x 128 screen on the panel for each Display
 * mode (DESIGN.md 11.55): the rect sd/pixel2/PICO-8.sh hands pico8_64 as
 * -draw_rect, and whether it draws unfiltered (-pixel_perfect 1).
 *
 * The rect is the one Diatom gives a 128 x 128 picture in that mode
 * (diatom/src/scale.c, src/dmodefit.c), so Full Screen, Sharp Pixels,
 * Sharp Pixels Zoomed, Fill Edges Cut and Actual Size mean over your PICO-8
 * what they mean over fake08 on the same shelf. On the Pixel 2's 640 x 480:
 *
 *   stretch            0,0      640 x 480   the panel, the shape changed
 *   aspect, square     80,0     480 x 480   the full height
 *   integer (and Tall) 128,48   384 x 384   3x
 *   overscale          0,-80    640 x 640   5x, the next whole size up, cut
 *   fill               0,-80    640 x 640   the full width, cut
 *   native             256,176  128 x 128   1x
 *
 * Unfiltered where every pixel of PICO-8's is a whole number of the panel's
 * (a factor of 128 both ways), filtered where it is not, as PICO-8 itself
 * draws with -pixel_perfect 0.
 *
 * A NEGATIVE OFFSET. Zoomed and Edges Cut start above the panel's top
 * (y -80). pico8_64 0.2.7 hands -draw_rect's x and y to SDL_RenderCopy as
 * they are on its GPU path (its get_dest_rect and codo_blit_to_video_plat,
 * read in the program; only its software blit clamps them at 0), and SDL
 * clips the quad, so the cut should be Diatom's. It is not tested on a
 * device. If a device test shows PICO-8 refusing it, P8_NEG_OK 0 here and
 * NEG_OK=0 in PICO-8.sh (make check-pico8 holds the two the same) turn each
 * of those two into the nearest mode that stays on the panel: Zoomed keeps
 * its whole sizes (Sharp Pixels), Edges Cut keeps the shape and the most of
 * the panel (Original Shape).
 *
 * Pure: no SDL, no device, for the launcher, portkeys (a screenshot crops
 * PICO-8's screen out of the panel by it) and tools/pico8-check.c. */
#ifndef ZYPEROS_P8DRAW_H
#define ZYPEROS_P8DRAW_H

#include <stdbool.h>

#include "dmodefit.h"

/* PICO-8's screen, in its own pixels */
#define P8_SCREEN 128

/* 1: pico8_64 takes a -draw_rect that starts off the panel (see above) */
#define P8_NEG_OK 1

/* The mode `display` (a display.<TAG> word, ZYPEROS_DISPLAY; NULL or a
 * word it does not know is stretch, as Diatom's default) on a landscape
 * surface `w` x `h`: the rect into `r`, and the return is whether it is
 * drawn unfiltered. `neg_ok` is P8_NEG_OK, a parameter for the check. */
bool p8_draw_rect(const char *display, int w, int h, bool neg_ok, dmode_rect *r);

/* The mode actually drawn for `display`: itself, or with `neg_ok` false
 * the nearest one on the panel (Zoomed's and Edges Cut's) */
const char *p8_draw_mode(const char *display, bool neg_ok);

#endif
