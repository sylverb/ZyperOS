/* SPDX-License-Identifier: GPL-3.0-only */
/* The graded bar (src/ds/scale.h): a row's value that is a scale reads back
 * as it was written and stays in its range; a string that is not one is
 * never taken for one; and the bar draws its knob where the value is - the
 * left end at -range, zero's mark under it at 0, the right end at +range -
 * with the panel's face in the knob, in both themes.
 *
 * And the bars and the colour chart (src/ds/colorref.h) are cheap to draw:
 * their many one-pixel columns are drawn once into a texture (ui_bake) and
 * copied, the very pixels they drew (each kind, both themes, two widths, two
 * places, focused or not, square corners, and the chart in a column and in a
 * list's picture), and a frame of Screen Colors' five bars and its chart makes
 * at most BUDGET drawing calls. Each column a call made the Pixel 2 draw the
 * page at 13 frames a second (#240, 2026-11-16). */
#include "../src/look.h"
#include "../src/ui.h"
#include "../src/ds/colorref.h"
#include "../src/ds/scale.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 640
#define H 720
#define BUDGET 40
static Uint32 px[W * H], live[W * H];
static int fails;

static void ck(int ok, const char *what)
{
	if (!ok) {
		fprintf(stderr, "FAIL %s\n", what);
		fails++;
	}
}

static Uint32 at(int x, int y)
{
	return px[y * W + x] & 0xffffff;
}

static Uint32 rgb(ds_rgb c)
{
	return (Uint32)c.r << 16 | (Uint32)c.g << 8 | c.b;
}

/* The frame `draw` makes, read into `out` */
static void frame(SDL_Renderer *r, const ds_theme *t, void (*draw)(SDL_Renderer *, int), int arg, Uint32 *out)
{
	SDL_SetRenderDrawColor(r, t->raised.r, t->raised.g, t->raised.b, 255);
	SDL_RenderClear(r);
	draw(r, arg);
	SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, out, W * 4);
}

/* Bars of each kind: a = kind * 8 + width * 4 + place * 2 + focused */
static void bar(SDL_Renderer *r, int a)
{
	const int kind = a / 8, wide = a / 4 % 2, place = a / 2 % 2, on_fill = a % 2;
	const SDL_Rect box = { 8 + place * 3, place ? 307 : 12, wide ? W - 16 : 420, 72 };

	ds_scale_draw(r, &box, (ds_scale_kind)kind, place ? 7 : -4, 10, on_fill);
}

/* The chart, in a column (0) and in a list's picture (1) */
static void chart(SDL_Renderer *r, int wide)
{
	ds_color_ref_draw(r, wide ? &(SDL_Rect){ 20, 40, 600, 300 } : &(SDL_Rect){ 200, 30, 230, 660 });
}

/* Drawn in place, then copied from its texture twice (made, then kept): the
 * same pixels every time */
static void same_baked(SDL_Renderer *r, const ds_theme *t, int th)
{
	int a, same = 0, all = 0, copied = 0;

	for (a = 0; a < DS_SCALE_KINDS * 8 + 2; a++) {
		void (*draw)(SDL_Renderer *, int) = a < DS_SCALE_KINDS * 8 ? bar : chart;
		const int arg = a < DS_SCALE_KINDS * 8 ? a : a - DS_SCALE_KINDS * 8;
		unsigned long in_place;

		ui_bake_on = false;
		ui_calls = 0;
		frame(r, t, draw, arg, live);
		in_place = ui_calls;
		ui_bake_on = true;
		for (int twice = 0; twice < 2; twice++) {
			ui_calls = 0;
			frame(r, t, draw, arg, px);
			all++;
			same += !memcmp(px, live, sizeof px);
		}
		/* the second from the texture kept, not drawn in place again */
		copied += ui_calls * 4 < in_place;
	}
	ck(copied == DS_SCALE_KINDS * 8 + 2, "every bar and chart drawn from its texture");
	ck(same == all, th == DS_LIGHT ? "the bars and the chart from their textures are the pixels they drew, in light"
	                               : "the bars and the chart from their textures are the pixels they drew, in dark");
}

/* Screen Colors' five bars and its chart, a frame: a few calls each */
static void page(SDL_Renderer *r, int unused)
{
	(void)unused;
	for (int k = 0; k < 5; k++)
		ds_scale_draw(r, &(SDL_Rect){ 20, 120 + k * 72, 420, 56 }, (ds_scale_kind)(k + 1), k - 2, 10, k == 1);
	ds_color_ref_draw(r, &(SDL_Rect){ 470, 120, 150, 560 });
}

static void budget(SDL_Renderer *r)
{
	unsigned long in_place, baked;

	theme_init(APPEAR_LIGHT);
	if (!ui_init(r, "res/fonts/menu.ttf")) return;
	ui_bake_on = false;
	ui_calls = 0;
	frame(r, theme(), page, 0, px);
	in_place = ui_calls;
	ui_bake_on = true;
	frame(r, theme(), page, 0, px);   /* the textures made */
	ui_calls = 0;
	frame(r, theme(), page, 0, px);
	baked = ui_calls;
	ui_quit();
	printf("scale: a frame of five bars and the chart, %lu drawing calls (%lu drawn in place)\n", baked, in_place);
	ck(baked <= BUDGET, "a frame of five bars and the chart within its budget of drawing calls");
	ck(in_place > 10 * baked, "the count sees the columns drawn in place");
}

int main(void)
{
	SDL_Surface *surface;
	SDL_Renderer *r;
	char buf[DS_SCALE_VALUE];
	ds_scale_kind k;
	int v, range, th;

	/* The value: written, read back, clamped to its range */
	for (int kind = 0; kind < DS_SCALE_KINDS; kind++)
		for (int x = -10; x <= 10; x++) {
			ds_scale_value(buf, (ds_scale_kind)kind, x, 10);
			ck(ds_scale_is(buf) && ds_scale_parse(buf, &k, &v, &range) && (int)k == kind && v == x && range == 10,
			   "a scale value reads back as written");
		}
	ds_scale_value(buf, DS_SCALE_GAMMA, 14, 10);
	ck(ds_scale_parse(buf, &k, &v, &range) && v == 10, "a value past the range is its end");
	ds_scale_value(buf, DS_SCALE_TINT, -30, 10);
	ck(ds_scale_parse(buf, &k, &v, &range) && v == -10, "a value past the range's other end is that end");
	ck(!ds_scale_is("+3") && !ds_scale_is(NULL) && !ds_scale_is("") && !ds_scale_is("scale 1 2 3"),
	   "a row's words are never a scale");
	ck(!ds_scale_parse("\x1bscale 9 0 10", &k, &v, &range), "a kind past the last is no scale");
	ck(!ds_scale_parse("\x1bscale 1 0 0", &k, &v, &range), "a range of nothing is no scale");

	/* The bar, drawn */
	if (SDL_Init(0) != 0) return 1;
	surface = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
	r = surface ? SDL_CreateSoftwareRenderer(surface) : NULL;
	if (!r) {
		fprintf(stderr, "scale: %s\n", SDL_GetError());
		return 1;
	}
	theme_set_file(NULL);
	for (th = 0; th < DS_THEMES; th++) {
		theme_init(th == DS_LIGHT ? APPEAR_LIGHT : APPEAR_DARK);
		if (!ui_init(r, "res/fonts/menu.ttf")) return 1;
		const ds_theme *t = theme();
		const SDL_Rect box = { 8, 12, W - 16, 72 };
		const int cy = box.y + box.h / 2, tx = box.x + DS_SCALE_KNOB / 2, tw = box.w - ds_scale_num_w() - DS_SCALE_KNOB;
		const int ends[3] = { -10, 0, 10 };

		ck(ds_scale_num_w() > 0, "the number has room");
		for (int e = 0; e < 3; e++) {
			const int kx = tx + (tw - 1) * (ends[e] + 10) / 20;

			SDL_SetRenderDrawColor(r, t->raised.r, t->raised.g, t->raised.b, 255);
			SDL_RenderClear(r);
			ds_scale_draw(r, &box, DS_SCALE_TEMPERATURE, ends[e], 10, false);
			SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, px, W * 4);
			ck(at(kx, cy) == rgb(t->raised), "the knob's face is where the value is");
			ck(at(kx - (DS_SCALE_KNOB / 2 - 2), cy) == rgb(t->text) || at(kx + (DS_SCALE_KNOB / 2 - 2), cy) == rgb(t->text),
			   "the knob's ring round it");
			if (ends[e] != 0)
				ck(at(tx + (tw - 1) / 2, cy) == rgb(t->text), "zero's mark through the track, away from the knob");
		}
		same_baked(r, t, th);
		ui_quit();
	}
	{
		/* Corners: Square, whose ring and patches have no round ends: a
		 * texture of its own, the same pixels again */
		look sq = LOOK_DEFAULT;

		sq.square = true;
		look_set(&sq);
		theme_init(APPEAR_LIGHT);
		if (ui_init(r, "res/fonts/menu.ttf")) {
			same_baked(r, theme(), DS_LIGHT);
			ui_quit();
		}
		sq = LOOK_DEFAULT;
		look_set(&sq);
	}
	budget(r);
	if (fails) {
		fprintf(stderr, "scale: %d failed\n", fails);
		return 1;
	}
	printf("scale: the value reads back and keeps its range, the knob is where it is, and the bars and the chart "
	       "copied from their textures are the pixels they drew, in both themes\n");
	return 0;
}
