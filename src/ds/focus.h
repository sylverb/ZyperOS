/* SPDX-License-Identifier: GPL-3.0-only */
/* The focus on the home and the shelves: Highlight's marks (DESIGN.md 3.5,
 * 11.47; my picks in the focus lab, 2026-11-19).
 *
 * Highlight picks how the item in focus is marked (src/look.h): Bar, as it
 * ships - the item grows a little on a soft lift, a short bar under it;
 * Border - a ring in the accent round it, a gap all round; Lift - it grows
 * more and rises on a deeper lift, one light crossing it as it lands; None -
 * it grows, nothing else. In every one the other items step back, a fifth
 * of the way toward the ground. Menus keep their fill whatever it says.
 *
 * Every mark sits on whole panel pixels (src/focusgeo.h): a ring of 2 and a
 * gap of 2 on the Pixel 2, 3 and 3 on the Brick, its corners the tile's own
 * plus the gap plus the ring, so the curves are parallel; the focused tile
 * grows to where its edges and its ring's land exact. The grow, the lift and
 * the name's color move with the tile's weight in the focus's glide (ms.quick,
 * out), and the mark glides from where it is, so the two are one motion.
 *
 * Part of the design system (src/ds/): no app.h. */
#ifndef ZYPEROS_DS_FOCUS_H
#define ZYPEROS_DS_FOCUS_H

#include <stdbool.h>
#include <SDL.h>

/* What Highlight draws on a grid, for one kind of item */
typedef struct {
	int   focus;    /* LOOK_FOCUS_* */
	float grow;     /* the focused item's growth, a share of its size */
	bool  ring;     /* Border's ring */
	bool  bar;      /* Bar's bar */
	int   lift;     /* DS_FOCUS_LIFT_* under the focused item */
	bool  sheen;    /* Lift's one light, on a cover */
} ds_focus_marks;

enum { DS_FOCUS_LIFT_NONE, DS_FOCUS_LIFT_SOFT, DS_FOCUS_LIFT_HIGH };

/* The kinds of item: a cover in a cell (the shelf's Grid), a system's
 * still (the home's Grid, Spotlight's dock) */
enum { DS_FOCUS_COVER, DS_FOCUS_STILL };

/* Each tile's weight in the focus's glide: 1 the focus, 0 a step away,
 * from what it was when the cursor last moved to what it is, on ms.quick's
 * curve; its grow, its lift and its name's color go with it, so they and
 * the mark are one motion. Kept: the tiles still part way when the cursor
 * moved (the one leaving the focus too), four at most. */
typedef struct { int i[4], n; float w[4]; } ds_focus_weights;

/* Tile `i`'s weight `since` ms after the cursor moved to `cursor` */
float ds_focus_weight(const ds_focus_weights *fw, int i, int cursor, Uint32 since);
/* The cursor leaves `cursor`, `since` ms after it came: the weights as they
 * are now become the next glide's start */
void ds_focus_weights_move(ds_focus_weights *fw, int cursor, Uint32 since);

/* Highlight's marks for an item of `kind`, as the look has it, or for
 * Highlight's choice `focus` (LOOK_FOCUS_*: its list's picture) */
ds_focus_marks ds_focus_marks_of(int kind);
ds_focus_marks ds_focus_marks_for(int kind, int focus);

/* The panel's pixels a unit is, on the handheld in force: 0.625 on the
 * Pixel 2, 1 on the Brick and this computer */
float ds_focus_k(void);

/* The focused item's rect: `cell` grown and placed on whole pixels with
 * its marks at weight 1, the cell itself at 0, between them on the way */
SDL_FRect ds_focus_tile(SDL_FRect cell, const ds_focus_marks *m, float w);

/* Border's ring round `tile`, whose corners are `radius` units (0 with
 * Square): a gap of 3 units and a ring of 3 in **accent-ink**, in panel
 * pixels, corners parallel to the tile's */
void ds_focus_ring(SDL_Renderer *r, SDL_FRect tile, int radius, Uint8 alpha);

/* Border on a row (the shelf's List): the ring inside `row`'s edge, of
 * the row's corners, the same panel pixels as a tile's */
void ds_focus_ring_in(SDL_Renderer *r, SDL_FRect row, int radius, Uint8 alpha);

/* Bar's bar: a pill 40 x 6 in **accent-ink**, 8 under `above`'s foot, in
 * the middle of it, on whole pixels */
void ds_focus_bar(SDL_Renderer *r, SDL_FRect above, Uint8 alpha);

/* An item's 1-pixel hairline just inside its edge, so very light art on
 * Beard and very dark art on Salt & Pepper keep an edge: ink at 16%, or
 * white at 10% */
void ds_focus_hairline(SDL_Renderer *r, SDL_FRect tile, int radius);

/* The step back of an item out of focus, its weight `w`: **ground** over
 * it, a fifth at 0 and none at 1 */
void ds_focus_dim(SDL_Renderer *r, SDL_FRect tile, int radius, float w);

/* Lift's one light across a cover (the **sheen** token), `t` of the way */
void ds_focus_sheen(SDL_Renderer *r, SDL_FRect tile, float t);

/* A ring between two rounded rects, `outer` and the hole `inner` inside it
 * (radii `ro`, `ri`), as rows of rects: a frame of any thickness, nothing
 * drawn in the hole */
void ds_round_ring(SDL_Renderer *r, SDL_Rect outer, SDL_Rect inner, int ro, int ri, SDL_Color c);

#endif
