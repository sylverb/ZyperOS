/* SPDX-License-Identifier: GPL-3.0-only */
/* ZyperOS's own screens through an old screen's shader (1.0.6): MENU >
 * Display > Shader from the home, for the home, the shelves, the menus and
 * every screen while no game runs (DESIGN.md 11.50).
 *
 * A game's shader redraws the game's few pixels as an old screen would, from
 * a picture much smaller than the panel (Diatom's GLSL, diatom/port/pixel2.c).
 * ZyperOS's screens have no such pixels: they are drawn at the panel's own
 * size, words and all, and drawn again at a smaller size they would blur. So
 * here the old screen is the glass, not the picture: the frame as it is,
 * times what the same shader makes of a WHITE old screen of SCREENFX_COLS x
 * SCREENFX_LINES pixels filling the panel - its scanlines, its grille, its
 * grid, its dark curved edge - and, for CRT Curved, the frame bent by the
 * same curve. The words stay sharp and the look is the game's.
 *
 * That product is a picture made once, when the shader or its strength
 * changes (screenfx_mask), and a frame shows it with one more textured pass,
 * a multiply (the device's plat_present). Off makes no picture and no pass.
 *
 * Pure: no SDL, no files. make check-screenfx holds it to tools/shaderdemo.c's
 * port of Diatom's GLSL. */
#ifndef ZYPEROS_SCREENFX_H
#define ZYPEROS_SCREENFX_H

#include <stdbool.h>

typedef enum {
	SFX_OFF,
	SFX_CRT,
	SFX_CRT_CURVED,
	SFX_SCANLINES,
	SFX_LCD,
	SFX_KINDS
} screenfx_kind;

/* The old screen the glass is: a 240-line picture, a TV's, 4:3 */
#define SCREENFX_COLS  320
#define SCREENFX_LINES 240

/* A shader by Diatom's name for it (src/app.h SHADERS: "crt", "crt-curved",
 * "scanlines", "lcd"); anything else, Off and Sharp-Shimmerless with it, is
 * SFX_OFF: Sharp-Shimmerless sizes a game's pixels, and these screens have
 * none to size */
screenfx_kind screenfx_kind_of(const char *name);

/* Whether kind k bends the picture (CRT Curved), so the frame goes through
 * screenfx_warp's mesh as well as under the mask */
bool screenfx_curved(screenfx_kind k);

/* The mask: w x h pixels, 4 bytes each (red, green, blue, 255 in memory
 * order, SDL's RGBA32), what kind k at `strength` percent (10 to 100) makes
 * of a white SCREENFX_COLS x SCREENFX_LINES screen drawn over w x h. 255 is
 * the frame as it is, 0 black. Nothing for SFX_OFF. */
void screenfx_mask(screenfx_kind k, int strength, int w, int h, unsigned char *rgba);

/* The same on a grid of `cols` x `lines`, the CRT's lines moved down by
 * `phase` of a line: the check's, to hold it to Diatom's own arithmetic at a
 * game's size (phase 0). screenfx_mask is this with SCREENFX_COLS,
 * SCREENFX_LINES and the phase that puts the first line's middle on the
 * first row of pixels' middle, so at two rows a line (the Pixel 2) every
 * other row is a line's bright middle: Diatom's own phase there puts every
 * row between two lines, evenly lit, and the CRT would show no lines. */
void screenfx_mask_grid(screenfx_kind k, int strength, int w, int h,
                        float cols, float lines, float phase, unsigned char *rgba);

/* CRT Curved's curve, Diatom's (warp 0.02 across, 0.015 down): the point of
 * the flat picture that the screen's point (x, y) shows, both 0..1 from the
 * top left. Outside 0..1 is past the picture's edge, which the mask's dark
 * edge covers. */
void screenfx_warp(float x, float y, float *u, float *v);

#endif
