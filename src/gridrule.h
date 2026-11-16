/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_GRIDRULE_H
#define ZYPEROS_GRIDRULE_H

/* The grid's rule (MENU > View > Grid, DESIGN.md 11.47), with nothing of a
 * renderer's or the launcher's in it, so tools/gridrule-check.c holds it to
 * a table: which shape a shelf takes from its art, where its cells go, and
 * which art is a cut-out. src/gamegrid.h draws by it. */
#include <SDL.h>
#include <stdbool.h>

#define GG_ROWS 2

/* ---- the rule, pure (tools/gamegrid-check.c) ---------------------------- */

/* The groups a cover's shape (width over height) falls in */
#define GG_PORTRAIT_UNDER  .83f
#define GG_SQUARE_TO      1.20f
/* How far a cell may be from the shelf's shape, and a cover from its cell's
 * before it is drawn whole instead of filling it: 12%, so less than 6% of a
 * cover is ever cut from a side */
#define GG_NEAR           1.12f
/* The shape with nothing to go by: the generated card's portrait */
#define GG_SHAPE_NONE      .70f
/* A shelf of cut-outs alone (a wheel set): landscape cards */
#define GG_SHAPE_CUTOUTS  1.40f

/* The shelf's shape from the shapes of its covers (`art`, `n` of them; a
 * folder, a generated card and a cut-out are not among them) and how many
 * cut-outs it has. The group with at least half of the covers gives it, its
 * middle cover's (the median); with no such group, square. Kept between 1:2
 * and 2:1. */
float gg_shape(const float *art, int n, int cutouts);

/* Where the cells go, in the room (`left`, `top`, `w`, `h`), for a shelf of
 * `shape`: two rows, the cells as wide as the room less the gaps allows and
 * no taller than two rows allow; the fewest columns that show at least
 * `least` covers with cells within GG_NEAR of the shape. The grid in the
 * middle of its room. */
typedef struct { int x, y, cols, rows, cw, ch, gap; } gg_geom;
gg_geom gg_layout(int left, int top, int w, int h, float shape, int gap, int least);

/* L1 (dir < 0) and R1 (dir > 0) on a grid of `n`: a screenful (`page`) on,
 * the same column, or to the end where less than a screenful is left; from
 * the end itself, round to the other end. So a grid that fits on one screen
 * gets first and last, never a jump of the page modulo its length. Pure. */
int gg_page(int cursor, int n, int page, int dir);

/* The rows whose covers the grid keeps decoded: its window (GG_ROWS rows
 * from `top`) and GG_AHEAD rows either side, counted round the ends, since
 * Left from the first cover, Right from the last and L1/R1 at the ends come
 * round; at an end, one row more round it, which the glide into the other
 * end's page shows a sliver of. So whatever one press brings into sight - a
 * row, a page, the other end - is in hand before it slides in. Pure. */
#define GG_AHEAD GG_ROWS
bool gg_row_kept(int row, int top, int nrows);

/* The rows outside the window to ask for ahead, at most `max`, the nearest
 * kept when there are more: farthest first, since the loader decodes the
 * newest ask first, and the row under the window asked last of its
 * distance. How many. Pure. */
int gg_rows_ahead(int top, int nrows, int *rows, int max);

/* The key gg_note and gg_art_of take: a file name's hash */
unsigned gg_key(const char *file);

/* Whether a picture is a cut-out: a quarter of its edge clear, or a sixth
 * of all of it. Pure, on an ARGB8888 surface. */
bool gg_cutout(const SDL_Surface *s);

#endif
