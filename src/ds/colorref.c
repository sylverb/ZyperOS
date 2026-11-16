/* SPDX-License-Identifier: GPL-3.0-only */
/* See colorref.h */
#include "colorref.h"

#include <math.h>
#include <stdbool.h>

#include "../theme.h"
#include "../ui.h"
#include "layout.h"

/* The classic 24-patch chart's colors in sRGB, as photographers know them:
 * skin dark and light, sky, foliage, flowers, the primaries and their mixes,
 * and six greys white to black. Four across, six down. */
static const Uint8 CHART[24][3] = {
	{ 115, 82, 68 },  { 194, 150, 130 }, { 98, 122, 157 }, { 87, 108, 67 },
	{ 133, 128, 177 }, { 103, 189, 170 }, { 214, 126, 44 }, { 80, 91, 166 },
	{ 193, 90, 99 },  { 94, 60, 108 },   { 157, 188, 64 }, { 224, 163, 46 },
	{ 56, 61, 150 },  { 70, 148, 73 },   { 175, 54, 60 },  { 231, 199, 31 },
	{ 187, 86, 149 }, { 8, 133, 161 },   { 243, 243, 242 }, { 200, 200, 200 },
	{ 160, 160, 160 }, { 122, 122, 121 }, { 85, 85, 85 },  { 52, 52, 52 },
};

#define REF_GAP DS_GRID * 2

static void hue_at(float h, Uint8 *r, Uint8 *g, Uint8 *b)
{
	float c[3];

	for (int k = 0; k < 3; k++) {
		float e = fmodf(h * 6.0f + (k == 0 ? 0.0f : k == 1 ? 4.0f : 2.0f), 6.0f);
		float v = fabsf(e - 3.0f) - 1.0f;

		c[k] = v < 0 ? 0 : v > 1 ? 1 : v;
	}
	*r = (Uint8)lroundf(c[0] * 255.0f);
	*g = (Uint8)lroundf(c[1] * 255.0f);
	*b = (Uint8)lroundf(c[2] * 255.0f);
}

void ds_color_ref_draw(SDL_Renderer *r, const SDL_Rect *box)
{
	/* Tall (a column): the chart four across, the ramp in two rows of
	 * eight; wide (a list's picture): the chart six across as it is
	 * printed, the ramp in one row. The patches square, as big as both the
	 * width and the height allow, the ramp and band as wide as the chart. */
	const bool wide = box->w > box->h;
	const int gap = DS_GRID, cols = wide ? 6 : 4, rws = 24 / cols;
	const float ramp_k = wide ? 0.5f : 1.0f, band_k = 0.5f;
	const int p_w = (box->w - (cols - 1) * gap) / cols;
	const int p_h = (int)((float)(box->h - (rws - 1) * gap - 2 * REF_GAP) / ((float)rws + ramp_k + band_k));
	const int p = p_w < p_h ? p_w : p_h;
	const int cw = cols * p + (cols - 1) * gap;
	const int ramp_h = (int)lroundf((float)p * ramp_k), band_h = p / 2;
	const int total = rws * p + (rws - 1) * gap + REF_GAP + ramp_h + REF_GAP + band_h;
	const int x0 = box->x + (box->w - cw) / 2;
	int y = box->y + (box->h - total) / 2, i, x;

	if (p <= 0) return;
	for (i = 0; i < 24; i++) {
		SDL_Rect q = { x0 + (i % cols) * (p + gap), y + (i / cols) * (p + gap), p, p };

		ui_round_rect(r, &q, DS_RADIUS_CHIP, (SDL_Color){ CHART[i][0], CHART[i][1], CHART[i][2], 255 });
	}
	y += rws * p + (rws - 1) * gap + REF_GAP;
	/* Sixteen greys, black to white */
	for (i = 0; i < 16; i++) {
		const int v = i * 255 / 15, per = wide ? 16 : 8, row = i / per, col = i % per;
		const int h = wide ? ramp_h : ramp_h / 2;
		const int a = x0 + cw * col / per, b = x0 + cw * (col + 1) / per;

		SDL_SetRenderDrawColor(r, (Uint8)v, (Uint8)v, (Uint8)v, 255);
		SDL_RenderFillRect(r, &(SDL_Rect){ a, y + row * h, b - a, h });
	}
	y += ramp_h + REF_GAP;
	/* The hues, red round to red */
	for (x = 0; x < cw; x++) {
		Uint8 cr, cg, cb;

		hue_at((float)x / (float)cw, &cr, &cg, &cb);
		SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
		SDL_RenderDrawLine(r, x0 + x, y, x0 + x, y + band_h - 1);
	}
}
