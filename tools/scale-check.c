/* SPDX-License-Identifier: GPL-3.0-only */
/* The graded bar (src/ds/scale.h): a row's value that is a scale reads back
 * as it was written and stays in its range; a string that is not one is
 * never taken for one; and the bar draws its knob where the value is - the
 * left end at -range, zero's mark under it at 0, the right end at +range -
 * with the panel's face in the knob, in both themes. */
#include "../src/ui.h"
#include "../src/ds/scale.h"
#include <stdio.h>
#include <string.h>

#define W 640
#define H 96
static Uint32 px[W * H];
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
		ui_quit();
	}
	if (fails) {
		fprintf(stderr, "scale: %d failed\n", fails);
		return 1;
	}
	printf("scale: the value reads back and keeps its range, and the knob is where it is, in both themes\n");
	return 0;
}
