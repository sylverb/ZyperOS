/* SPDX-License-Identifier: GPL-3.0-only */
/* The rules of src/ds/flex.h. Four passes over the tree, as a
 * flexbox does: the widths each box wants (bottom up), the widths each gets
 * (top down, with grow, shrink and the lines of a wrapping row), the heights
 * each wants at that width (bottom up, a text measured where it wraps), and
 * the heights and places each gets (top down, with steps and kept aspects).
 * Ids run parent before child, so a pass is one loop. */
#include "ds/flex.h"

#include <limits.h>
#include <string.h>

#define WANTS_ALL (1 << 20)   /* what a box with a kept aspect and nothing in it asks for */

static int clampi(int v, int lo, int hi)
{
	if (hi > 0 && v > hi) v = hi;
	if (v < lo) v = lo;
	return v;
}

int ds_flex_add(ds_flex *L, int parent, const ds_box *b)
{
	ds_flex_node *n;
	int id;

	if (L->n >= DS_FLEX_MAX) return -1;
	id = L->n++;
	n = &L->node[id];
	memset(n, 0, sizeof *n);
	n->b = *b;
	n->parent = parent;
	n->first = n->last = n->next = -1;
	if (parent >= 0) {
		ds_flex_node *p = &L->node[parent];

		if (p->last >= 0) L->node[p->last].next = id;
		else p->first = id;
		p->last = id;
	}
	return id;
}

int ds_flex_begin(ds_flex *L, int w, int h, const ds_box *root)
{
	ds_box b = *root;

	L->n = 0;
	b.w = w;
	b.h = h;
	return ds_flex_add(L, -1, &b);
}

static ds_align cross_of(const ds_flex *L, const ds_flex_node *c)
{
	ds_align a = c->b.self;

	if (a == DS_A_AUTO && c->parent >= 0) a = L->node[c->parent].b.align;
	return a == DS_A_AUTO ? DS_A_STRETCH : a;
}

static int weight(const ds_box *b, int size, int free, bool horiz)
{
	int lo = horiz ? b->min_w : b->min_h, hi = horiz ? b->max_w : b->max_h;

	if (free > 0) return (hi > 0 && size >= hi) ? 0 : b->grow;
	return size <= lo ? 0 : b->shrink * (size > 0 ? size : 0);
}

/* Sizes kids along one axis to fill `avail`: the room left goes to the ones
 * that grow, an overflow comes off the ones that shrink (by their size, as
 * a flexbox does), each within its min and max; what a clamp holds back
 * goes round again */
static void share(const ds_flex *L, const int *kids, int nk, int avail, bool horiz, int *size)
{
	int i, pass, sum = 0;

	for (i = 0; i < nk; i++) sum += size[i];
	for (pass = 0; pass < 4; pass++) {
		int free = avail - sum, total = 0, last = -1, given = 0;

		if (free == 0) break;
		for (i = 0; i < nk; i++) {
			int wt = weight(&L->node[kids[i]].b, size[i], free, horiz);

			if (wt) { total += wt; last = i; }
		}
		if (!total) break;
		for (i = 0; i < nk; i++) {
			const ds_box *b = &L->node[kids[i]].b;
			int wt = weight(b, size[i], free, horiz), d, ns;

			if (!wt) continue;
			d = i == last ? free - given : (int)((long long)free * wt / total);
			given += d;
			ns = clampi(size[i] + d, horiz ? b->min_w : b->min_h, horiz ? b->max_w : b->max_h);
			sum += ns - size[i];
			size[i] = ns;
		}
	}
}

static int kids_of(const ds_flex *L, int id, int *kids)
{
	int c, nk = 0;

	for (c = L->node[id].first; c >= 0; c = L->node[c].next) kids[nk++] = c;
	return nk;
}

static void want_w(ds_flex *L)
{
	int i;

	for (i = L->n - 1; i >= 0; i--) {
		ds_flex_node *n = &L->node[i];
		const ds_box *b = &n->b;
		int w = 0, k = 0, c;

		if (b->w) { n->pw = b->w; continue; }
		if (n->first < 0) {
			if (b->measure) {
				int mw = 0, mh = 0;

				b->measure(b->ctx, INT_MAX, &mw, &mh);
				w = mw;
			} else if (b->aspect_w > 0) {
				w = WANTS_ALL;
			}
		} else {
			for (c = n->first; c >= 0; c = L->node[c].next, k++) {
				int cw = L->node[c].pw;

				if (b->dir == DS_ROW) w += cw + (k ? b->gap : 0);
				else if (cw > w) w = cw;
			}
		}
		n->pw = clampi(w + b->pad.l + b->pad.r, b->min_w, b->max_w);
	}
}

static void place_w(ds_flex *L)
{
	int i, k;

	L->node[0].r.w = L->node[0].b.w;
	for (i = 0; i < L->n; i++) {
		ds_flex_node *n = &L->node[i];
		const ds_box *b = &n->b;
		int inner = n->r.w - b->pad.l - b->pad.r;
		int kids[DS_FLEX_MAX], size[DS_FLEX_MAX], nk = kids_of(L, i, kids);

		if (!nk) continue;
		if (b->dir == DS_COL) {
			for (k = 0; k < nk; k++) {
				ds_flex_node *cn = &L->node[kids[k]];
				int w = cn->b.w ? cn->b.w
				      : cross_of(L, cn) == DS_A_STRETCH ? inner
				      : cn->pw < inner ? cn->pw : inner;

				cn->r.w = clampi(w, cn->b.min_w, cn->b.max_w);
				cn->line = 0;
			}
			continue;
		}
		if (!b->wrap) {
			for (k = 0; k < nk; k++) size[k] = L->node[kids[k]].pw;
			share(L, kids, nk, inner - b->gap * (nk - 1), true, size);
			for (k = 0; k < nk; k++) { L->node[kids[k]].r.w = size[k]; L->node[kids[k]].line = 0; }
			continue;
		}
		{
			/* Greedy lines, as keyhints_rows breaks them: a box goes on to
			 * the next line when it and the gap before it do not fit */
			int line = 0, used = 0, first = 0;

			for (k = 0; k < nk; k++) {
				ds_flex_node *cn = &L->node[kids[k]];
				int w = cn->pw < inner ? cn->pw : inner;

				if (k > first && used + b->gap + w > inner) {
					share(L, kids + first, k - first, inner - b->gap * (k - first - 1), true, size + first);
					line++;
					first = k;
					used = w;
				} else {
					used += (k > first ? b->gap : 0) + w;
				}
				size[k] = w;
				cn->line = line;
			}
			share(L, kids + first, nk - first, inner - b->gap * (nk - first - 1), true, size + first);
			for (k = 0; k < nk; k++) L->node[kids[k]].r.w = size[k];
		}
	}
}

static void want_h(ds_flex *L)
{
	int i;

	for (i = L->n - 1; i >= 0; i--) {
		ds_flex_node *n = &L->node[i];
		const ds_box *b = &n->b;
		int h = 0, k = 0, c, iw = n->r.w - b->pad.l - b->pad.r;

		if (b->h) { n->ph = b->h; continue; }
		if (n->first < 0) {
			if (b->measure) {
				int mw = 0, mh = 0;

				b->measure(b->ctx, iw, &mw, &mh);
				h = mh;
			} else if (b->aspect_w > 0) {
				h = iw * b->aspect_h / b->aspect_w;
			}
		} else if (b->dir == DS_COL) {
			for (c = n->first; c >= 0; c = L->node[c].next, k++)
				h += L->node[c].ph + (k ? b->gap : 0);
		} else {
			int line = -1, lh = 0;

			for (c = n->first; c >= 0; c = L->node[c].next) {
				const ds_flex_node *cn = &L->node[c];

				if (cn->line != line) {
					if (line >= 0) h += lh + b->line_gap;
					line = cn->line;
					lh = 0;
				}
				if (cn->ph > lh) lh = cn->ph;
			}
			h += lh;
		}
		n->ph = clampi(h + b->pad.t + b->pad.b, b->min_h, b->max_h);
	}
}

static int cross_at(ds_align a, int free)
{
	return a == DS_A_CENTER ? free / 2 : a == DS_A_END ? free : 0;
}

static void justify(ds_justify j, int spare, int n, int *off, int *extra, int *rest)
{
	*off = *extra = *rest = 0;
	if (spare <= 0) return;
	if (j == DS_J_CENTER) *off = spare / 2;
	else if (j == DS_J_END) *off = spare;
	else if (j == DS_J_BETWEEN && n > 1) { *extra = spare / (n - 1); *rest = spare % (n - 1); }
}

static void place_h(ds_flex *L)
{
	int i, k;

	L->node[0].r.x = L->node[0].r.y = 0;
	L->node[0].r.h = L->node[0].b.h;
	for (i = 0; i < L->n; i++) {
		ds_flex_node *n = &L->node[i];
		const ds_box *b = &n->b;
		int iw = n->r.w - b->pad.l - b->pad.r, ih = n->r.h - b->pad.t - b->pad.b;
		int x0 = n->r.x + b->pad.l, y0 = n->r.y + b->pad.t;
		int kids[DS_FLEX_MAX], size[DS_FLEX_MAX], slot[DS_FLEX_MAX], nk = kids_of(L, i, kids);
		int off, extra, rest, used = 0, pos;

		if (!nk) continue;
		if (b->dir == DS_COL) {
			for (k = 0; k < nk; k++) size[k] = L->node[kids[k]].ph;
			share(L, kids, nk, ih - b->gap * (nk - 1), false, size);
			for (k = 0; k < nk; k++) {
				ds_flex_node *cn = &L->node[kids[k]];

				/* Whole steps: the room under the last one is spare */
				if (cn->b.step > 0) {
					int s = cn->b.step;

					size[k] = size[k] / s * s;
					if (size[k] < s) size[k] = s;
				}
				slot[k] = size[k];
				/* A kept aspect fits its slot: narrower if the slot is
				 * shorter than its width wants, at the slot's top. A
				 * leaf's alone (a picture, a cover): a box with boxes in
				 * it was given its width before this pass, and so were
				 * they */
				if (cn->b.aspect_w > 0 && !cn->b.w && cn->first < 0) {
					int w = size[k] * cn->b.aspect_w / cn->b.aspect_h;

					if (w > cn->r.w) w = cn->r.w;
					cn->r.w = w;
					size[k] = w * cn->b.aspect_h / cn->b.aspect_w;
				}
				used += slot[k] + (k ? b->gap : 0);
			}
			justify(b->justify, ih - used, nk, &off, &extra, &rest);
			pos = y0 + off;
			for (k = 0; k < nk; k++) {
				ds_flex_node *cn = &L->node[kids[k]];

				cn->r.h = size[k];
				cn->r.y = pos;
				cn->r.x = x0 + cross_at(cross_of(L, cn), iw - cn->r.w);
				pos += slot[k] + b->gap + extra + (k < rest ? 1 : 0);
			}
			continue;
		}
		{
			/* A row, one line or more: each line as tall as its tallest
			 * (a single line as tall as the row), its boxes spread by
			 * justify and placed across by align */
			int a = 0, line_y = y0;

			while (a < nk) {
				int z = a, lh = 0, cnt;

				while (z < nk && L->node[kids[z]].line == L->node[kids[a]].line) {
					if (L->node[kids[z]].ph > lh) lh = L->node[kids[z]].ph;
					z++;
				}
				if (!b->wrap) lh = ih;
				cnt = z - a;
				used = 0;
				for (k = a; k < z; k++) used += L->node[kids[k]].r.w + (k > a ? b->gap : 0);
				justify(b->justify, iw - used, cnt, &off, &extra, &rest);
				pos = x0 + off;
				for (k = a; k < z; k++) {
					ds_flex_node *cn = &L->node[kids[k]];
					ds_align al = cross_of(L, cn);
					int h = cn->b.h ? cn->b.h : al == DS_A_STRETCH ? lh : cn->ph < lh ? cn->ph : lh;

					cn->r.h = clampi(h, cn->b.min_h, cn->b.max_h);
					cn->r.y = line_y + cross_at(al, lh - cn->r.h);
					cn->r.x = pos;
					pos += cn->r.w + b->gap + extra + (k - a < rest ? 1 : 0);
				}
				line_y += lh + b->line_gap;
				a = z;
			}
		}
	}
}

void ds_flex_solve(ds_flex *L)
{
	if (L->n <= 0) return;
	want_w(L);
	place_w(L);
	want_h(L);
	place_h(L);
}

SDL_Rect ds_flex_rect(const ds_flex *L, int id)
{
	return id >= 0 && id < L->n ? L->node[id].r : (SDL_Rect){ 0, 0, 0, 0 };
}

int ds_flex_lines(const ds_flex *L, int id)
{
	int c, lines = 0;

	for (c = L->node[id].first; c >= 0; c = L->node[c].next)
		if (L->node[c].line + 1 > lines) lines = L->node[c].line + 1;
	return lines;
}
