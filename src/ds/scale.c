/* SPDX-License-Identifier: GPL-3.0-only */
/* See scale.h */
#include "scale.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../look.h"
#include "../theme.h"
#include "../ui.h"
#include "layout.h"

/* A scale value: this mark, then "kind v range" */
#define SCALE_MARK "\x1bscale "

const char *ds_scale_value(char buf[DS_SCALE_VALUE], ds_scale_kind kind, int v, int range)
{
	snprintf(buf, DS_SCALE_VALUE, SCALE_MARK "%d %d %d", (int)kind, v, range);
	return buf;
}

bool ds_scale_is(const char *value)
{
	return value && !strncmp(value, SCALE_MARK, sizeof SCALE_MARK - 1);
}

bool ds_scale_parse(const char *value, ds_scale_kind *kind, int *v, int *range)
{
	int k, a, b;

	if (!ds_scale_is(value) || sscanf(value + sizeof SCALE_MARK - 1, "%d %d %d", &k, &a, &b) != 3) return false;
	if (k < 0 || k >= DS_SCALE_KINDS || b <= 0) return false;
	*kind = (ds_scale_kind)k;
	*range = b;
	*v = a < -b ? -b : a > b ? b : a;
	return true;
}

/* The track's height and a knob's, the marks' under it (the scale-* tokens);
 * the track's ring a stroke, its width its own */
#define TRACK_H DS_SCALE_TRACK
#define TRACK_RING 2
#define KNOB_D DS_SCALE_KNOB
#define MARK_H DS_SCALE_MARK
#define ZERO_H DS_SCALE_ZERO
#define NUM_GAP (DS_GRID * 2)

int ds_scale_num_w(void)
{
	TTF_Font *f = ui_font(UI_F_MENU);
	int a = ui_text_tabular_width(f, "+10"), b = ui_text_tabular_width(f, "\xE2\x88\x92" "10");

	return (a > b ? a : b) + NUM_GAP;
}

static ds_rgb mixc(ds_rgb a, ds_rgb b, float t)
{
	return (ds_rgb){ (Uint8)lroundf(a.r + (b.r - a.r) * t), (Uint8)lroundf(a.g + (b.g - a.g) * t),
	                 (Uint8)lroundf(a.b + (b.b - a.b) * t) };
}

/* Two ends and a middle, as the scale reads left to right */
static ds_rgb three(ds_rgb lo, ds_rgb mid, ds_rgb hi, float t)
{
	return t < 0.5f ? mixc(lo, mid, t * 2.0f) : mixc(mid, hi, (t - 0.5f) * 2.0f);
}

/* The track's color at t (0 the scale's left end, 1 its right), in its
 * top half or its bottom (`low`: the contrast's two greys) */
static ds_rgb track_at(ds_scale_kind kind, float t, bool low)
{
	const ds_rgb white = { 244, 244, 240 };

	switch (kind) {
	case DS_SCALE_TEMPERATURE:
		return three((ds_rgb){ 116, 166, 255 }, white, (ds_rgb){ 255, 160, 72 }, t);
	case DS_SCALE_TINT:
		return three((ds_rgb){ 104, 200, 104 }, white, (ds_rgb){ 214, 104, 206 }, t);
	case DS_SCALE_CONTRAST: {
		/* A dark grey over a light one, one grey at the left, parting to
		 * black and white at the right */
		const float a = 0.04f + 0.46f * t;
		const float v = low ? 0.5f + a : 0.5f - a;
		const Uint8 g = (Uint8)lroundf(255.0f * (v < 0 ? 0 : v > 1 ? 1 : v));

		return (ds_rgb){ g, g, g };
	}
	case DS_SCALE_SATURATION: {
		/* A warm red from its own grey to its fullest */
		const ds_rgb full = { 255, 92, 52 }, grey = { 140, 140, 140 };

		return mixc(grey, full, 0.15f + 0.85f * t);
	}
	case DS_SCALE_GAMMA:
	default: {
		const Uint8 g = (Uint8)lroundf(255.0f * (0.22f + 0.56f * t));

		return (ds_rgb){ g, g, g };
	}
	}
}

/* The track's colors, its columns `w` by `h` at (x0, y0), each cut to the
 * round ends: a line for each column's top half and one for its bottom (the
 * contrast's two greys). Drawn once into a texture (ui_bake) and copied after;
 * in place when there is none. The column's ends are counted from the track's
 * middle, so they come out the same wherever the track is (tools/scale-check.c
 * holds the copy to the drawing). */
static void track_columns(SDL_Renderer *r, ds_scale_kind kind, int x0, int y0, int w, int h)
{
	const float rad = h / 2.0f;
	const int cy = y0 + h / 2;
	int x;

	for (x = 0; x < w; x++) {
		const float t = (float)x / (float)(w - 1);
		const ds_rgb c = track_at(kind, t, false), lo = track_at(kind, t, true);
		float d = x < rad ? rad - (float)x - 0.5f : x > w - rad ? (float)x + 0.5f - (w - rad) : 0.0f;
		float half = d > 0 ? sqrtf(fmaxf(0.0f, rad * rad - d * d)) : rad;
		int a = (int)lroundf((float)cy - half), b = (int)lroundf((float)cy + half);

		SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
		SDL_RenderDrawLine(r, x0 + x, a, x0 + x, (a + b) / 2 - 1);
		SDL_SetRenderDrawColor(r, lo.r, lo.g, lo.b, 255);
		SDL_RenderDrawLine(r, x0 + x, (a + b) / 2, x0 + x, b - 1);
		ui_calls += 2;
	}
}

static void track_bake(SDL_Renderer *r, int k, int w, int h)
{
	track_columns(r, (ds_scale_kind)k, 0, 0, w, h);
}

/* The parts of a bar that hold still: its ring, its colors and its marks
 * under it, for the track at `track`; the colors from their texture
 * (`columns_baked`) or drawn in place */
static void bar_still(SDL_Renderer *r, ds_scale_kind kind, const SDL_Rect *track, SDL_Color ring,
                      SDL_Color ink, SDL_Color dim, bool columns_baked)
{
	const int cy = track->y + TRACK_H / 2;
	SDL_Texture *baked;
	int k;

	/* Its ring, then its colors inside, each column cut to the round ends */
	ui_round_rect(r, &(SDL_Rect){ track->x - TRACK_RING, track->y - TRACK_RING, track->w + 2 * TRACK_RING,
	                                track->h + 2 * TRACK_RING }, TRACK_H / 2 + TRACK_RING, ring);
	ui_calls++;
	if (columns_baked && (baked = ui_bake(r, "scale", (int)kind, track->w, track->h, track_bake))) {
		SDL_RenderCopy(r, baked, NULL, track);
		ui_calls++;
	} else {
		track_columns(r, kind, track->x, track->y, track->w, track->h);
	}
	/* The marks under it: the ends, the halves and zero, zero's the tallest
	 * and through the track */
	for (k = 0; k <= 4; k++) {
		const int mx = track->x + (track->w - 1) * k / 4;
		const bool zero = k == 2;
		const int top = zero ? cy - TRACK_H / 2 - ZERO_H / 2 : cy + TRACK_H / 2 + 4;
		const int h = zero ? TRACK_H + ZERO_H : MARK_H;

		ui_round_rect(r, &(SDL_Rect){ mx - 1, top, 2, h }, 1, zero ? ink : dim);
		ui_calls++;
	}
}

/* An unfocused bar's still parts, all opaque, in one texture: the ring and
 * the marks reach PAD_X either side of the track, PAD_TOP over it (zero's
 * mark) and PAD_BOTTOM under it (the others). A focused bar's ring and marks
 * are see-through over the focus fill, and a texture cannot hold them so that
 * its copy is the same pixels, so they are drawn in place. */
#define PAD_X TRACK_RING
#define PAD_TOP (ZERO_H / 2)
#define PAD_BOTTOM (4 + MARK_H)

/* Its texture's key: the kind and every color and shape it is drawn in */
static int still_key(ds_scale_kind kind, const ds_theme *th)
{
	const Uint8 v[] = { th->edge.r, th->edge.g, th->edge.b, th->text.r, th->text.g, th->text.b,
	                    th->text_dim.r, th->text_dim.g, th->text_dim.b, (Uint8)look_square() };
	Uint32 h = 2166136261u;

	for (size_t i = 0; i < sizeof v; i++) h = (h ^ v[i]) * 16777619u;
	return (int)((h << 3 | (Uint32)kind) & 0x7fffffffu);
}

static void still_bake(SDL_Renderer *r, int k, int w, int h)
{
	const ds_theme *th = theme();

	(void)h;
	bar_still(r, (ds_scale_kind)(k & 7), &(SDL_Rect){ PAD_X, PAD_TOP, w - 2 * PAD_X, TRACK_H },
	          ui_col(th->edge, 255), ui_col(th->text, 255), ui_col(th->text_dim, 255), false);
}

void ds_scale_draw(SDL_Renderer *r, const SDL_Rect *box, ds_scale_kind kind, int v, int range, bool on_fill)
{
	const ds_theme *th = theme();
	const SDL_Color ink = on_fill ? ui_col(th->on_accent, 255) : ui_col(th->text, 255);
	const SDL_Color dim = on_fill ? ui_col(th->on_accent, 200) : ui_col(th->text_dim, 255);
	const int num_w = ds_scale_num_w();
	const int cy = box->y + box->h / 2;
	/* The track, its knob's half in from each end so the knob at -10 or
	 * +10 stays over it */
	const SDL_Rect track = { box->x + KNOB_D / 2, cy - TRACK_H / 2, box->w - num_w - KNOB_D, TRACK_H };
	const SDL_Rect still = { track.x - PAD_X, track.y - PAD_TOP, track.w + 2 * PAD_X,
	                         PAD_TOP + TRACK_H + PAD_BOTTOM };
	SDL_Texture *baked;
	char num[16];

	if (range <= 0 || track.w <= 0) return;
	if (!on_fill && (baked = ui_bake(r, "scale-still", still_key(kind, th), still.w, still.h, still_bake))) {
		SDL_RenderCopy(r, baked, NULL, &still);
		ui_calls++;
	} else {
		bar_still(r, kind, &track, on_fill ? ui_col(th->on_accent, 90) : ui_col(th->edge, 255), ink, dim, true);
	}
	/* The knob: the value, a ring in the text's color round the panel's face */
	{
		const int kx = track.x + (int)lroundf((float)(track.w - 1) * (float)(v + range) / (float)(2 * range));
		SDL_Rect o = { kx - KNOB_D / 2, cy - KNOB_D / 2, KNOB_D, KNOB_D };
		SDL_Rect in = { o.x + 4, o.y + 4, KNOB_D - 8, KNOB_D - 8 };

		ui_round_rect(r, &o, KNOB_D / 2, ink);
		ui_round_rect(r, &in, KNOB_D / 2 - 4, on_fill ? ui_col(th->accent, 255) : ui_col(th->raised, 255));
		ui_calls += 2;
	}
	/* The number, signed, its digits tabular: "-3" with a minus sign */
	if (v > 0) snprintf(num, sizeof num, "+%d", v);
	else if (v < 0) snprintf(num, sizeof num, "\xE2\x88\x92%d", -v);
	else snprintf(num, sizeof num, "0");
	{
		TTF_Font *f = ui_font(UI_F_MENU);

		ui_text_tabular(r, f, num, box->x + box->w, ui_cap_y(f, cy), 1, ink);
		ui_calls++;
	}
}
