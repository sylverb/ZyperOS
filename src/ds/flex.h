/* SPDX-License-Identifier: GPL-3.0-only */
/* Boxes laid out by rules instead of typed y values (DESIGN.md "The
 * system", design system round 3). A screen builds a small tree of boxes
 * each frame - rows and columns with padding, gaps, alignment, grow and
 * shrink, wrapping, a kept aspect, sizes in whole steps - its leaves measured
 * by their caller (a text's real width and lines in its language), solves it
 * once, and draws each part in its box's rectangle. Integer units of the
 * canvas, so a screen moved onto it can keep its pixels exactly; no
 * allocation, the tree living in the caller's ds_flex; ids in the order the
 * boxes were added, a parent before its children. Part of the design system
 * (src/ds/, DESIGN.md "Where each part lives"); checked by check-flex. */
#ifndef ZYPEROS_DS_FLEX_H
#define ZYPEROS_DS_FLEX_H

#include <SDL.h>
#include <stdbool.h>

#define DS_FLEX_MAX 128

typedef enum { DS_COL, DS_ROW } ds_dir;
typedef enum { DS_A_AUTO, DS_A_START, DS_A_CENTER, DS_A_END, DS_A_STRETCH } ds_align;
typedef enum { DS_J_START, DS_J_CENTER, DS_J_END, DS_J_BETWEEN } ds_justify;
typedef struct { int l, t, r, b; } ds_edges;

/* A leaf's size for at most `max_w` across (INT_MAX: unbounded): its width
 * on one line, and its height when wrapped to max_w */
typedef void (*ds_measure_fn)(void *ctx, int max_w, int *w, int *h);

typedef struct {
	ds_dir dir;            /* how its children run */
	ds_edges pad;          /* its children from its edges */
	int gap;               /* between children along dir */
	int line_gap;          /* between wrapped lines */
	ds_justify justify;    /* children along dir, with room to spare */
	ds_align align;        /* children across dir (AUTO: stretch) */
	ds_align self;         /* this box across its parent's dir (AUTO: the parent's align) */
	int w, h;              /* a fixed size; 0 fits the content */
	int min_w, min_h;
	int max_w, max_h;      /* 0: none */
	int grow;              /* its share of the room left along the parent's dir */
	int shrink;            /* its share of an overflow, down to its min */
	int aspect_w, aspect_h;/* keeps w:h inside what it is given (a leaf's: a picture, a cover) */
	int step;              /* its size along the parent's dir in whole steps (a list's rows) */
	bool wrap;             /* a row whose children go on to more lines */
	ds_measure_fn measure;
	void *ctx;
} ds_box;

typedef struct {
	ds_box b;
	int parent, first, last, next;
	int pw, ph;            /* wanted width, height */
	int line;              /* its line in a wrapping row */
	SDL_Rect r;
} ds_flex_node;

typedef struct {
	ds_flex_node node[DS_FLEX_MAX];
	int n;
} ds_flex;

/* The root box, `w` by `h` at (0, 0); returns its id (0) */
int ds_flex_begin(ds_flex *L, int w, int h, const ds_box *root);
/* A box in `parent`; -1 when the tree is full */
int ds_flex_add(ds_flex *L, int parent, const ds_box *b);
void ds_flex_solve(ds_flex *L);
SDL_Rect ds_flex_rect(const ds_flex *L, int id);
/* The lines of a wrapping row after solve */
int ds_flex_lines(const ds_flex *L, int id);

#endif
