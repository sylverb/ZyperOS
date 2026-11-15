/* SPDX-License-Identifier: GPL-3.0-only */
/* The grid's rule, pure: see gridrule.h. tools/gridrule-check.c holds it. */
#include "gridrule.h"

#include <math.h>
#include <stdlib.h>

static int group_of(float a)
{
	return a < GG_PORTRAIT_UNDER ? 0 : a <= GG_SQUARE_TO ? 1 : 2;
}

static int by_value(const void *x, const void *y)
{
	float a = *(const float *)x, b = *(const float *)y;

	return a < b ? -1 : a > b;
}

float gg_shape(const float *art, int n, int cutouts)
{
	float in[256];
	int count[3] = { 0, 0, 0 }, m, g, k;

	if (n <= 0) return cutouts > 0 ? GG_SHAPE_CUTOUTS : GG_SHAPE_NONE;
	if (n > 256) n = 256;
	for (k = 0; k < n; k++) count[group_of(art[k])]++;
	for (g = 0; g < 3; g++) {
		if (count[g] * 2 < n) continue;
		for (m = 0, k = 0; k < n; k++)
			if (group_of(art[k]) == g) in[m++] = art[k];
		qsort(in, (size_t)m, sizeof *in, by_value);
		return fminf(2.0f, fmaxf(.5f, in[m / 2]));
	}
	return 1.0f;
}

gg_geom gg_layout(int left, int top, int w, int h, float shape, int gap, int least)
{
	float chmax = (h - gap) / 2.0f, best = 1e9f, cw, ch;
	int pick = 0, c;
	gg_geom g;

	if (shape <= 0) shape = GG_SHAPE_NONE;
	for (c = 2; c <= 9; c++) {
		float off;

		cw = (float)(w - (c - 1) * gap) / c;
		ch = fminf(cw / shape, chmax);
		off = fabsf(logf(cw / ch / shape));
		if (GG_ROWS * c < least) continue;
		if (cw / ch <= shape * GG_NEAR) { pick = c; break; }
		if (off < best) { best = off; pick = c; }
	}
	cw = (float)(w - (pick - 1) * gap) / pick;
	ch = fminf(cw / shape, chmax);
	g = (gg_geom){ left, top, pick, GG_ROWS, (int)floorf(cw), (int)floorf(ch), gap };
	g.y = top + (h - (GG_ROWS * g.ch + (GG_ROWS - 1) * gap)) / 2;
	return g;
}

unsigned gg_key(const char *file)
{
	unsigned h = 2166136261u;

	for (const unsigned char *p = (const unsigned char *)(file ? file : ""); *p; p++)
		h = (h ^ *p) * 16777619u;
	return h;
}

static Uint32 px_at(const SDL_Surface *s, int x, int y)
{
	return *(const Uint32 *)((const Uint8 *)s->pixels + y * s->pitch + x * 4);
}

bool gg_cutout(const SDL_Surface *s)
{
	enum { N = 32 };
	int clear = 0, edge = 0, all = 0, x, y;

	if (!s || s->w <= 0 || s->h <= 0 || s->format->format != SDL_PIXELFORMAT_ARGB8888) return false;
	for (y = 0; y < N; y++)
		for (x = 0; x < N; x++) {
			int sx = (int)((x + .5f) * s->w / N), sy = (int)((y + .5f) * s->h / N);
			bool see = (px_at(s, sx, sy) >> 24) < 128;

			all += see;
			if (x > 1 && y > 1 && x < N - 2 && y < N - 2) continue;
			edge++;
			clear += see;
		}
	return clear * 4 > edge || all * 6 > N * N;
}

