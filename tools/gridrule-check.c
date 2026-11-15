/* SPDX-License-Identifier: GPL-3.0-only */
/* The grid's rule (src/gridrule.h, DESIGN.md 11.47), held to a table.
 *
 *     make check-gridrule
 *
 * 1. The shape a shelf takes from its art: every common shape alone, a shelf
 *    mostly of one shape, an even mix (square), cut-outs alone, no art, and
 *    the clamp at 1:2 and 2:1.
 * 2. Where the cells go on the 1024 x 768 canvas for each, as DESIGN.md's
 *    table gives them, and on a wider canvas: inside the room, the cells
 *    filling its width, at least six covers, every cell within 12% of the
 *    shape, the columns the fewest that are.
 * 3. Which art is a cut-out: a logo on nothing, a box in 3D on nothing, and
 *    neither a cover nor a cover with a clear corner. */
#include "gridrule.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failed;

#define NEED(cond, ...) do { if (!(cond)) { failed++; printf("FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void shape(const char *what, const float *art, int n, int cutouts, float want)
{
	float got = gg_shape(art, n, cutouts);

	NEED(fabsf(got - want) < .005f, "shape of %s: %.3f, want %.3f", what, got, want);
}

/* The room on a canvas `sw` wide: as src/gamegrid.c's gg_geom_for has it */
#define LEFT 64
#define TOP  96
#define FOOT 568
static void cells(const char *what, int sw, float s, int cols, int cw, int ch)
{
	gg_geom g = gg_layout(LEFT, TOP, sw - 2 * LEFT, FOOT - TOP, s, 24, 6);
	int gw = g.cols * g.cw + (g.cols - 1) * g.gap, gh = g.rows * g.ch + (g.rows - 1) * g.gap;
	float a = (float)g.cw / g.ch;

	if (cols) NEED(g.cols == cols && g.cw == cw && g.ch == ch, "%s: %d across, %d x %d; want %d, %d x %d",
	               what, g.cols, g.cw, g.ch, cols, cw, ch);
	NEED(g.rows == 2, "%s: %d rows", what, g.rows);
	NEED(g.cols * g.rows >= 6, "%s: %d covers", what, g.cols * g.rows);
	NEED(g.x >= LEFT && g.x + gw <= sw - LEFT, "%s: the grid leaves its room across", what);
	NEED(g.y >= TOP && g.y + gh <= FOOT, "%s: the grid leaves its room down (%d to %d)", what, g.y, g.y + gh);
	NEED(sw - 2 * LEFT - gw < g.cols, "%s: the cells do not fill the width (%d of %d)", what, gw, sw - 2 * LEFT);
	NEED(a <= s * GG_NEAR + .01f && a >= s / GG_NEAR - .01f, "%s: a cell of %.3f for a shelf of %.3f", what, a, s);
	/* the fewest columns: one fewer is not within 12%, or shows too few */
	if (g.cols > 3) {
		float cw1 = (float)(sw - 2 * LEFT - (g.cols - 2) * 24) / (g.cols - 1);
		float ch1 = fminf(cw1 / s, (FOOT - TOP - 24) / 2.0f);

		NEED(cw1 / ch1 > s * GG_NEAR, "%s: %d columns would do", what, g.cols - 1);
	}
}

static SDL_Surface *surface(int w, int h)
{
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);

	if (s) SDL_FillRect(s, NULL, 0xff336699u);
	return s;
}

static void clear(SDL_Surface *s, int x, int y, int w, int h)
{
	SDL_FillRect(s, &(SDL_Rect){ x, y, w, h }, 0x00000000u);
}

int main(void)
{
	static const float p23[] = { .667f, .667f, .667f, .667f, .667f, .667f };
	static const float boxes[] = { .70f, .71f, .72f, .70f, .72f };
	static const float psp[] = { .58f, .57f, .58f, .58f, .58f };
	static const float sq[] = { 1.0f, 1.0f, .98f, 1.0f, 1.02f };
	static const float ds[] = { 1.11f, 1.11f, 1.10f };
	static const float snes[] = { 1.40f, 1.39f, 1.40f, 1.41f };
	static const float wide[] = { 1.778f, 1.778f, 1.778f };
	static const float mostly[] = { .667f, .667f, .667f, .667f, .667f, .667f, .667f, 1.43f, 1.43f };
	static const float even[] = { .667f, .667f, .70f, .58f, 1.0f, 1.0f, 1.0f, 1.11f, 1.40f, 1.40f, 1.37f, 1.778f };
	static const float half[] = { .667f, .667f, 1.0f, 1.40f };
	static const float hero[] = { 3.1f, 3.1f, 3.1f };
	static const float thin[] = { .3f, .3f };
	SDL_Surface *s;

	/* 1. the shape */
	shape("2:3 grids", p23, 6, 0, .667f);
	shape("portrait boxes", boxes, 5, 0, .71f);
	shape("PSP cases", psp, 5, 0, .58f);
	shape("square art", sq, 5, 0, 1.0f);
	shape("DS boxes", ds, 3, 0, 1.11f);
	shape("SNES boxes", snes, 4, 0, 1.40f);
	shape("16:9 art", wide, 3, 0, 1.778f);
	shape("mostly 2:3, two screenshots", mostly, 9, 1, .667f);
	shape("an even mix", even, 12, 1, 1.0f);
	shape("a group with half the art", half, 4, 0, .667f);
	shape("cut-outs alone", NULL, 0, 8, GG_SHAPE_CUTOUTS);
	shape("no art", NULL, 0, 0, GG_SHAPE_NONE);
	shape("3:1 heroes (kept to 2:1)", hero, 3, 0, 2.0f);
	shape("thin art (kept to 1:2)", thin, 2, 0, .5f);

	/* 2. the cells, DESIGN.md 11.47's table */
	cells("2:3", 1024, .667f, 5, 160, 224);
	cells("portrait boxes", 1024, .71f, 5, 160, 224);
	cells("PSP cases", 1024, .58f, 6, 129, 222);
	cells("square", 1024, 1.0f, 4, 206, 206);
	cells("DS boxes", 1024, 1.11f, 4, 206, 185);
	cells("SNES boxes", 1024, 1.40f, 3, 282, 201);
	cells("4:3 screenshots", 1024, 1.333f, 3, 282, 212);
	cells("16:9 art", 1024, 16.0f / 9, 3, 282, 159);
	cells("2:1 art", 1024, 2.0f, 3, 282, 141);
	cells("thin art", 1024, .5f, 0, 0, 0);
	/* a wider canvas (a 16:9 screen in the 768-high design space) takes
	 * more columns, by the same rule */
	for (float a = .5f; a <= 2.0f; a += .05f) {
		char what[48];

		snprintf(what, sizeof what, "1365 wide, shape %.2f", a);
		cells(what, 1365, a, 0, 0, 0);
		snprintf(what, sizeof what, "1024 wide, shape %.2f", a);
		cells(what, 1024, a, 0, 0, 0);
	}

	/* 3. cut-outs */
	s = surface(400, 600);
	NEED(s && !gg_cutout(s), "a cover is no cut-out");
	clear(s, 0, 0, 40, 40);
	NEED(!gg_cutout(s), "a cover with a clear corner is no cut-out");
	SDL_FreeSurface(s);
	s = surface(800, 300);
	clear(s, 0, 0, 800, 300);
	SDL_FillRect(s, &(SDL_Rect){ 60, 80, 680, 140 }, 0xffcc3322u);
	NEED(gg_cutout(s), "a logo on nothing is a cut-out");
	SDL_FreeSurface(s);
	s = surface(500, 500);
	clear(s, 0, 0, 500, 500);
	SDL_FillRect(s, &(SDL_Rect){ 40, 20, 420, 470 }, 0xff226644u);
	NEED(gg_cutout(s), "a box in 3D on nothing is a cut-out");
	SDL_FreeSurface(s);
	s = surface(900, 300);
	clear(s, 0, 0, 900, 300);
	for (int x = 0; x < 900; x += 60) SDL_FillRect(s, &(SDL_Rect){ x, 0, 40, 300 }, 0xffffffffu);
	NEED(gg_cutout(s), "a logo that reaches its edges, clear between letters, is a cut-out");
	SDL_FreeSurface(s);
	NEED(!gg_cutout(NULL), "no picture is no cut-out");

	/* the key is a file name's, and tells two apart */
	NEED(gg_key("Metroid (USA).nes") == gg_key("Metroid (USA).nes"), "a key is the same twice");
	NEED(gg_key("Metroid (USA).nes") != gg_key("Metroid (Europe).nes"), "two names, two keys");

	/* L1 and R1: a screenful in the same column, else to the end, and
	 * from the end round to the other one */
	NEED(gg_page(0, 23, 10, +1) == 10 && gg_page(10, 23, 10, +1) == 20, "R1 a screenful, the same column");
	NEED(gg_page(15, 23, 10, +1) == 22, "R1 with less than a screenful left goes to the last");
	NEED(gg_page(22, 23, 10, +1) == 0, "R1 on the last comes round to the first");
	NEED(gg_page(20, 23, 10, -1) == 10 && gg_page(5, 23, 10, -1) == 0, "L1 a screenful, else to the first");
	NEED(gg_page(0, 23, 10, -1) == 22, "L1 on the first comes round to the last");
	NEED(gg_page(0, 12, 18, +1) == 11 && gg_page(11, 12, 18, +1) == 0, "a grid on one screen: R1 last, then first");
	NEED(gg_page(4, 12, 18, -1) == 0 && gg_page(0, 12, 18, -1) == 11, "a grid on one screen: L1 first, then last");
	NEED(gg_page(0, 1, 10, +1) == 0 && gg_page(0, 0, 10, +1) == 0, "one cover, or none, stays put");

	if (failed) { printf("gridrule: %d failed\n", failed); return 1; }
	printf("gridrule: every shape, mix and cell as DESIGN.md 11.47 says\n");
	return 0;
}
