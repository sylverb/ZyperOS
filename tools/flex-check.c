/* SPDX-License-Identifier: GPL-3.0-only */
/* The layout engine's rules (src/ds/flex.h), one case each, then a
 * thousand random trees, then the time one screen's layout takes.
 *
 *   make check-flex
 *
 * Each rule as DESIGN.md "The system" states it: padding and gaps, grow
 * shared by weight with the remainder to the last, shrink by size, min and
 * max, justify and align, a wrapping row breaking where keyhints_rows breaks
 * a row of keys, whole steps, a kept aspect in a shorter slot, a measured
 * text taller where it is narrower. Then random trees, solved twice: every
 * box stays inside its parent wherever the parent has the room, no size is
 * negative, and the same tree gives the same rectangles. */
#include "../src/ds/flex.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int fails;

static void ck(int cond, const char *what)
{
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

static int eq(SDL_Rect r, int x, int y, int w, int h)
{
	return r.x == x && r.y == y && r.w == w && r.h == h;
}

/* A text of `len` letters 10 wide, wrapped on words of 5 letters, 20 a line */
static void text(void *ctx, int max_w, int *w, int *h)
{
	int len = *(int *)ctx, per = max_w == INT_MAX ? len : (max_w / 10 / 5) * 5, lines;

	if (per < 5) per = 5;
	lines = (len + per - 1) / per;
	*w = (len < per ? len : per) * 10;
	*h = lines * 20;
}

static void rules(void)
{
	static ds_flex L;
	int root, a, b, c, d, len = 30;

	/* A column: padding, then each box under the last, gap apart */
	root = ds_flex_begin(&L, 400, 300, &(ds_box){ .dir = DS_COL, .pad = { 10, 20, 30, 40 }, .gap = 8 });
	a = ds_flex_add(&L, root, &(ds_box){ .h = 50 });
	b = ds_flex_add(&L, root, &(ds_box){ .h = 60, .w = 100, .self = DS_A_CENTER });
	ds_flex_solve(&L);
	ck(eq(ds_flex_rect(&L, a), 10, 20, 360, 50), "a column's box stretches between its padding");
	ck(eq(ds_flex_rect(&L, b), 140, 78, 100, 60), "the next 8 under it, centered across");

	/* A row: room shared by weight, the remainder to the last that grows */
	root = ds_flex_begin(&L, 100, 50, &(ds_box){ .dir = DS_ROW, .gap = 0 });
	a = ds_flex_add(&L, root, &(ds_box){ .grow = 1 });
	b = ds_flex_add(&L, root, &(ds_box){ .grow = 1 });
	c = ds_flex_add(&L, root, &(ds_box){ .grow = 1 });
	ds_flex_solve(&L);
	ck(ds_flex_rect(&L, a).w == 33 && ds_flex_rect(&L, b).w == 33 && ds_flex_rect(&L, c).w == 34,
	   "100 among three: 33, 33 and the remainder's 34 to the last");
	ck(ds_flex_rect(&L, c).x == 66 && ds_flex_rect(&L, a).h == 50, "side by side, as tall as the row");

	/* Shrink by size, within a min */
	root = ds_flex_begin(&L, 200, 50, &(ds_box){ .dir = DS_ROW });
	a = ds_flex_add(&L, root, &(ds_box){ .w = 0, .min_w = 0, .shrink = 1, .measure = text, .ctx = &len });
	b = ds_flex_add(&L, root, &(ds_box){ .w = 100 });
	ds_flex_solve(&L);
	ck(ds_flex_rect(&L, a).w == 100 && ds_flex_rect(&L, b).w == 100,
	   "a text 300 wide gives up what the row lacks; a fixed box keeps its size");
	ck(ds_flex_rect(&L, a).h == 20 * 3 || ds_flex_rect(&L, a).h == 50,
	   "and wraps where it is narrower");

	/* Justify and align */
	root = ds_flex_begin(&L, 300, 100, &(ds_box){ .dir = DS_ROW, .justify = DS_J_BETWEEN, .align = DS_A_END });
	a = ds_flex_add(&L, root, &(ds_box){ .w = 50, .h = 20 });
	b = ds_flex_add(&L, root, &(ds_box){ .w = 50, .h = 40 });
	c = ds_flex_add(&L, root, &(ds_box){ .w = 50, .h = 30 });
	ds_flex_solve(&L);
	ck(ds_flex_rect(&L, a).x == 0 && ds_flex_rect(&L, b).x == 125 && ds_flex_rect(&L, c).x == 250,
	   "between: the first at the start, the last at the end, the room shared");
	ck(ds_flex_rect(&L, a).y == 80 && ds_flex_rect(&L, b).y == 60, "end: each on the row's foot");

	/* A wrapping row breaks as keyhints_rows does: a box goes on to the
	 * next line when it and the gap before it do not fit */
	root = ds_flex_begin(&L, 300, 200, &(ds_box){ .dir = DS_COL, .justify = DS_J_END });
	d = ds_flex_add(&L, root, &(ds_box){ .dir = DS_ROW, .wrap = true, .gap = 16, .line_gap = 12 });
	a = ds_flex_add(&L, d, &(ds_box){ .w = 140, .h = 32 });
	b = ds_flex_add(&L, d, &(ds_box){ .w = 140, .h = 32 });
	c = ds_flex_add(&L, d, &(ds_box){ .w = 140, .h = 32 });
	ds_flex_solve(&L);
	ck(ds_flex_lines(&L, d) == 2, "140 + 16 + 140 fits 300; a third goes on to a second line");
	ck(ds_flex_rect(&L, c).y == ds_flex_rect(&L, a).y + 44 && ds_flex_rect(&L, c).x == 0,
	   "the second line 12 under the first, from the start");
	ck(ds_flex_rect(&L, d).h == 76 && ds_flex_rect(&L, d).y == 124, "two lines tall, on the column's foot");

	/* Whole steps: a list window in whole rows, the rest spare under it */
	root = ds_flex_begin(&L, 400, 300, &(ds_box){ .dir = DS_COL });
	a = ds_flex_add(&L, root, &(ds_box){ .grow = 1, .step = 72 });
	b = ds_flex_add(&L, root, &(ds_box){ .h = 28 });
	ds_flex_solve(&L);
	ck(ds_flex_rect(&L, a).h == 216 && ds_flex_rect(&L, b).y == 216, "272 of room is three whole rows of 72");

	/* A kept aspect in a slot shorter than its width wants: narrower,
	 * placed across by align */
	root = ds_flex_begin(&L, 896, 460, &(ds_box){ .dir = DS_COL });
	a = ds_flex_add(&L, root, &(ds_box){ .aspect_w = 16, .aspect_h = 9, .self = DS_A_CENTER, .shrink = 1 });
	ds_flex_solve(&L);
	ck(ds_flex_rect(&L, a).h <= 460 && ds_flex_rect(&L, a).w * 9 / 16 == ds_flex_rect(&L, a).h,
	   "16:9 kept in a shorter slot");
	ck(ds_flex_rect(&L, a).x == (896 - ds_flex_rect(&L, a).w) / 2, "and centered");

	/* Min and max */
	root = ds_flex_begin(&L, 500, 50, &(ds_box){ .dir = DS_ROW });
	a = ds_flex_add(&L, root, &(ds_box){ .grow = 1, .max_w = 120 });
	b = ds_flex_add(&L, root, &(ds_box){ .grow = 1 });
	ds_flex_solve(&L);
	ck(ds_flex_rect(&L, a).w == 120 && ds_flex_rect(&L, b).w == 380, "a max holds back, the rest goes round again");
}

/* A small generator, so the random trees are the same on every run */
static unsigned seed = 12345u;

static int rnd(int n)
{
	seed = seed * 1103515245u + 12345u;
	return (int)((seed >> 16) % (unsigned)n);
}

static void random_trees(void)
{
	static ds_flex L, M;
	int t, i, bad = 0, moved = 0, negative = 0;

	for (t = 0; t < 1000; t++) {
		int lens[DS_FLEX_MAX], n;
		unsigned keep = seed;

		for (i = 0; i < DS_FLEX_MAX; i++) lens[i] = 5 + rnd(40);
		for (int pass = 0; pass < 2; pass++) {
			ds_flex *T = pass ? &M : &L;

			seed = keep;
			ds_flex_begin(T, 400 + rnd(600), 300 + rnd(400),
			              &(ds_box){ .dir = rnd(2) ? DS_COL : DS_ROW, .pad = { rnd(20), rnd(20), rnd(20), rnd(20) },
			                         .gap = rnd(3) * 8 });
			n = 1 + rnd(30);
			for (i = 1; i < n && T->n < DS_FLEX_MAX; i++) {
				int parent = rnd(T->n);
				ds_box b = { .dir = rnd(2) ? DS_COL : DS_ROW, .gap = rnd(3) * 8, .grow = rnd(3), .shrink = rnd(2),
				             .justify = (ds_justify)rnd(4), .align = (ds_align)rnd(5), .wrap = rnd(4) == 0 };

				if (T->node[parent].b.measure) parent = 0;   /* a text holds no boxes */
				switch (rnd(4)) {
				case 0: b.w = 8 * (1 + rnd(20)); b.h = 8 * (1 + rnd(10)); break;
				case 1: b.measure = text; b.ctx = &lens[i]; break;
				case 2: b.aspect_w = 16; b.aspect_h = 9; b.shrink = 1; break;
				default: break;
				}
				ds_flex_add(T, parent, &b);
			}
			ds_flex_solve(T);
		}
		for (i = 0; i < L.n; i++) {
			SDL_Rect r = L.node[i].r, s = M.node[i].r;

			if (memcmp(&r, &s, sizeof r)) moved++;
			if (r.w < 0 || r.h < 0) negative++;
		}
		/* Inside its parent where the parent's children fit it: a box is
		 * outside only when its parent has less room than they need */
		for (i = 1; i < L.n; i++) {
			const ds_flex_node *c = &L.node[i], *p = &L.node[c->parent];
			int need = 0, k, room;

			for (k = p->first; k >= 0; k = L.node[k].next)
				need += (p->b.dir == DS_COL ? L.node[k].ph : L.node[k].pw) + (k != p->first ? p->b.gap : 0);
			room = p->b.dir == DS_COL ? p->r.h - p->b.pad.t - p->b.pad.b : p->r.w - p->b.pad.l - p->b.pad.r;
			if (need > room || p->b.wrap) continue;
			/* and across: a box wider (in a column) or taller (in a row)
			 * than its parent's inside is an overflow its parent cannot
			 * help */
			if ((p->b.dir == DS_COL ? c->pw : c->ph) >
			    (p->b.dir == DS_COL ? p->r.w - p->b.pad.l - p->b.pad.r : p->r.h - p->b.pad.t - p->b.pad.b))
				continue;
			if (c->r.x < p->r.x || c->r.y < p->r.y ||
			    c->r.x + c->r.w > p->r.x + p->r.w || c->r.y + c->r.h > p->r.y + p->r.h) {
				if (bad < 8 && getenv("FLEX_DEBUG"))
					printf("  out: parent %s %d,%d %dx%d pad %d,%d,%d,%d need %d room %d | child w%d h%d grow%d shrink%d step%d asp%d meas%d wrap%d al%d self%d -> %d,%d %dx%d (pw %d ph %d)\n",
					       p->b.dir == DS_COL ? "col" : "row", p->r.x, p->r.y, p->r.w, p->r.h,
					       p->b.pad.l, p->b.pad.t, p->b.pad.r, p->b.pad.b, need, room,
					       c->b.w, c->b.h, c->b.grow, c->b.shrink, c->b.step, c->b.aspect_w, c->b.measure != NULL,
					       c->b.wrap, p->b.align, c->b.self, c->r.x, c->r.y, c->r.w, c->r.h, c->pw, c->ph);
				bad++;
			}
		}
	}
	ck(!moved, "the same tree gives the same rectangles");
	ck(!negative, "no size is negative");
	ck(!bad, "a box stays inside its parent wherever the parent has the room");
	if (bad || moved || negative) printf("  (%d outside, %d moved, %d negative)\n", bad, moved, negative);
}

/* One screen's worth: a column of a header, a 16:9 frame, a bar's row of
 * three and six keys wrapping at the foot, 14 boxes */
static double timing(void)
{
	static ds_flex L;
	int i, k, n = 200000;
	clock_t t0 = clock();

	for (i = 0; i < n; i++) {
		int root = ds_flex_begin(&L, 1024, 768, &(ds_box){ .dir = DS_COL, .pad = { 64, 0, 64, 32 }, .gap = 24 });
		int body = ds_flex_add(&L, root, &(ds_box){ .dir = DS_COL, .gap = 24, .grow = 1, .shrink = 1 });
		int row, keys;

		ds_flex_add(&L, body, &(ds_box){ .h = 72 });
		ds_flex_add(&L, body, &(ds_box){ .aspect_w = 16, .aspect_h = 9, .self = DS_A_CENTER, .shrink = 1 });
		row = ds_flex_add(&L, body, &(ds_box){ .dir = DS_ROW, .h = 32, .gap = 16, .align = DS_A_CENTER });
		ds_flex_add(&L, row, &(ds_box){ .w = 70, .h = 32 });
		ds_flex_add(&L, row, &(ds_box){ .h = 6, .grow = 1 });
		ds_flex_add(&L, row, &(ds_box){ .w = 90, .h = 32 });
		keys = ds_flex_add(&L, root, &(ds_box){ .dir = DS_ROW, .wrap = true, .gap = 16, .line_gap = 16,
		                                        .justify = DS_J_BETWEEN });
		for (k = 0; k < 6; k++) ds_flex_add(&L, keys, &(ds_box){ .w = 120 + 10 * k, .h = 32 });
		ds_flex_solve(&L);
	}
	return (double)(clock() - t0) / CLOCKS_PER_SEC * 1e6 / n;
}

int main(void)
{
	double us;

	rules();
	random_trees();
	us = timing();
	if (fails) { printf("\n%d layout check(s) failed\n", fails); return 1; }
	printf("ok: the layout's rules hold, and 1000 random trees stay in their parents, the same each time; "
	       "one screen's layout %.2f us here\n", us);
	return 0;
}
