/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_COVERCOLOR_H
#define ZYPEROS_COVERCOLOR_H

/* An album cover's main color (DESIGN.md 11.14): the cassette's stripe and
 * the vinyl it is pressed in. Content, as the cover is - the one color of
 * MP4's looks that is no theme's role.
 *
 * The color the eye takes the cover to be: the hue that the most of it is,
 * weighed by how vivid each part is, and that hue's own average color. A
 * cover that is mostly white with a red title is red; one that is a grey
 * photograph with no hue worth the name is its grey. No SDL, so
 * tools/covercolor-check.c holds it with pictures of its own. */

#include <stdint.h>

/* `w` x `h` pixels of 0xAARRGGBB, `pitch` bytes a row; 0xRRGGBB. A pixel
 * mostly transparent counts for nothing; all of them so, mid grey. */
unsigned cover_main_rgb(const uint32_t *px, int w, int h, int pitch);

/* The same color as dark as `l` (0-1, the HSL lightness), its hue kept and
 * its saturation held to at most `s_max`: the record's vinyl, dark enough
 * that the grooves' light reads on it */
unsigned cover_rgb_darken(unsigned rgb, float l, float s_max);

#endif
