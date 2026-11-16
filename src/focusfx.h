/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_FOCUSFX_H
#define ZYPEROS_FOCUSFX_H

/* What the cover row does when it stops on a game's card with no art
 * (src/cardart.h): one light crossing the card, once (**sheen**), and, for
 * a played game, its frame of play turning from blurred to sharp
 * (**sharpen**). DESIGN.md 11.1.
 *
 * Both start when the row has stopped on the card and end by themselves:
 * the sheen once across and gone, the sharp card held while the focus
 * stays. Nothing loops - a still shelf draws nothing (DESIGN.md 8) - and
 * every one of them stops at once when the focus moves, or the row does.
 *
 * Plain C, so tools/focusfx-check.c holds the timeline without a window. */

#include <stdbool.h>
#include <stdint.h>

typedef struct {
	int      sys, idx;    /* the card it is for; -1 none */
	bool     settled;     /* the row is stopped on it */
	uint32_t since;       /* when it stopped */
	uint32_t sharp_from;  /* when the sharp card was in hand, 0 not yet */
} focusfx;

typedef struct {
	float    sheen;   /* the light's way across the card, 0 to 1; < 0 none */
	float    sharp;   /* how far the sharp card is drawn over the blurred, 0 to 1 */
	bool     moving;  /* either is under way: draw again */
	uint32_t until;   /* and until when (the redraw clock); 0 when nothing moves */
} focusfx_out;

/* Forget the card: the shelf is left, or rebuilt */
void focusfx_reset(focusfx *f);

/* One look, every frame the cover row is drawn. `sys`, `idx`: the focused
 * card; `generated`: it is a game's card with no art (no sheen otherwise);
 * `played`: it shows a frame of play, so it has a sharp one; `settled`: the
 * row is stopped; `sharp_ready`: the sharp card is in hand. Returns true
 * when the focus is new or the row has just stopped on it: the moment to
 * ask for the sharp card. */
bool focusfx_step(focusfx *f, int sys, int idx, bool generated, bool played, bool settled,
                  bool sharp_ready, uint32_t now, focusfx_out *out);

#endif
