/* SPDX-License-Identifier: GPL-3.0-only */
/* The Border behind a game: what fills the screen around the game's picture
 * where the display mode leaves room (the launcher's src/border.h, DESIGN.md
 * 11.55). Black, as always, until the launcher says otherwise.
 *
 *   SETBORDER path=FILE   a picture the launcher made, the surface's size or
 *                         any size (stretched to the surface), in the overlay's
 *                         DTOV file (main.c): Your Own, a handheld's frame
 *   SETBORDER blur=1      the game's own picture, small, blurred and dimmed,
 *                         made from the frames as they come (border_blur)
 *   SETBORDER             black again
 *
 * Held until the next SETBORDER, and dropped at each RUN, so a game the
 * launcher says nothing for is black around as before. The port draws it
 * under the game and under every overlay (the level bar, On-Screen Stats,
 * the low battery's dot and the notices stay on top), from one upload: a
 * picture costs nothing a frame but its pixels drawn where black was.
 *
 * Blur is made on the CPU from the frame Diatom keeps anyway (g_frame), every
 * BORDER_BLUR_EVERY presented frames, as a BORDER_BLUR_W x BORDER_BLUR_H
 * picture the port draws stretched and smooth: the frame's middle, cut to the
 * surface's shape, averaged into the small picture's cells, blurred twice
 * with a 3x3 box, its colors a little stronger and then darkened, so the
 * game stays what the eye goes to. Plain C: test/border_test.c. */
#ifndef DIATOM_BORDER_H
#define DIATOM_BORDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BORDER_BLUR_W 32
#define BORDER_BLUR_H 24
/* A new blur every this many frames presented: 10 times a second at 60 */
#define BORDER_BLUR_EVERY 6
/* The largest picture a file may hold: the Brick's surface and more */
#define BORDER_MAX_W 2048
#define BORDER_MAX_H 2048

/* The blurred picture of a frame: `frame` is `w` x `h` pixels, `pitch` bytes
 * a row, RGB565 (`rgb565`) or XRGB8888; the result BGRA, opaque, into `out`
 * (BORDER_BLUR_W x BORDER_BLUR_H), cut to the shape `surf_w` x `surf_h`.
 * False, and `out` untouched, for a frame that is none. */
bool border_blur(const void *frame, int w, int h, size_t pitch, bool rgb565, int surf_w, int surf_h,
                 uint8_t out[BORDER_BLUR_W * BORDER_BLUR_H * 4]);

/* A DTOV file's picture, read whole: a new buffer of `*w` x `*h` BGRA pixels
 * the caller frees, its alpha set to opaque (a border is never see-through:
 * the panel would show what is under it), or NULL for a file that is not one
 * or is larger than BORDER_MAX_W x BORDER_MAX_H. */
uint8_t *border_read(const char *path, int *w, int *h);

#endif
