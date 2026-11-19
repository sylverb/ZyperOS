/* SPDX-License-Identifier: GPL-3.0-only */
/* make check-focusgeo: src/focusgeo.h against the Pixel 2's filter.
 *
 * The Pixel 2 shows the 1024 x 768 picture at 640 x 480 through linear
 * filtering: its pixel i reads the picture at 1.6 i + 0.3 (pixel centres,
 * texel centres), the two units either side blended. A line of units in two
 * colors is filtered here as the GPU does, and what fg_unit and fg_tile give
 * is held to whole pixels: each exact edge with one color on each side of it
 * and nothing of the other, the others a tenth at most; the focused tile and
 * its ring's two edges and its bar's two edges exact, the tile grown a
 * little and no more; a unit a pixel exact everywhere and changed by none of
 * it. Plain C, no SDL. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/focusgeo.h"

static int failed;

static void ck(int ok, const char *what)
{
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) failed = 1;
}

/* The filter's pixel i over a line of units, `a` up to unit `edge` and `b`
 * from it: 1 where the pixel is all `a`, 0 all `b` */
static double pixel(int i, int edge)
{
	double pos = 1.6 * i + 0.3, f = pos - floor(pos);
	int j = (int)floor(pos);
	double t0 = j < edge ? 1 : 0, t1 = j + 1 < edge ? 1 : 0;

	return (1 - f) * t0 + f * t1;
}

/* How much of the other color pixel edge `p`, drawn at fg_unit's unit, puts
 * into the pixels either side of it */
static double bleed(int p)
{
	int u = fg_unit(p, FG_K_PIXEL2);
	double before = 1 - pixel(p - 1, u), after = pixel(p, u);

	return before > after ? before : after;
}

int main(void)
{
	const float k = FG_K_PIXEL2;
	int bad = 0, worst_exact = 0;
	double worst = 0;

	/* Every pixel edge of the panel's width, and some under 0 */
	for (int p = -12; p <= 640; p++) {
		double b = bleed(p);

		if (fg_exact(p, k) && b > 1e-9) worst_exact = 1;
		if (b > worst) worst = b;
		if (fg_px(fg_unit(p, k), k) != p) bad++;
	}
	ck(!worst_exact, "an exact pixel edge has one color on each side, nothing of the other");
	ck(worst <= 0.1 + 1e-9, "every other pixel edge puts a tenth of the next color in at most");
	ck(bad == 0, "the unit of a pixel edge rounds back to that pixel");
	ck(fg_exact(0, k) && fg_exact(2, k) && fg_exact(3, k) && !fg_exact(1, k) && !fg_exact(4, k) &&
	   fg_exact(-5, k) && fg_exact(-3, k), "edges 0, 2 and 3 of each 5 pixels are the exact ones");

	/* The shelf grid's focused cover, Border fine: a gap and a ring of 2
	 * pixels each, round a cell 160 x 224 at 64, 96, grown 3% */
	{
		const int ring[2] = { 2, 4 };
		fg_rect cell = { 64, 96, 160, 224 };
		fg_px_rect t = fg_tile(cell, .03f, k, ring, 2, NULL, 0);
		int l = t.x, r = t.x + t.w, top = t.y, b = t.y + t.h;
		int base_l = fg_px(64, k), base_r = fg_px(224, k), base_t = fg_px(96, k), base_b = fg_px(320, k);

		ck(fg_exact(l, k) && fg_exact(l - 2, k) && fg_exact(l - 4, k) && fg_exact(r, k) && fg_exact(r + 2, k) &&
		   fg_exact(r + 4, k) && fg_exact(top, k) && fg_exact(top - 2, k) && fg_exact(top - 4, k) &&
		   fg_exact(b, k) && fg_exact(b + 2, k) && fg_exact(b + 4, k),
		   "the focused cover, its ring's gap and the ring land on whole pixels");
		ck(base_l - l >= 1 && base_l - l <= 4 && r - base_r >= 1 && r - base_r <= 4 &&
		   base_t - top >= 1 && base_t - top <= 5 && b - base_b >= 1 && b - base_b <= 5,
		   "and it grows a few pixels each side, no more");
	}
	/* Bar: the cover's foot, then the bar 5 pixels under it (8 units), 4
	 * high (6 units) */
	{
		const int bar[2] = { 5, 9 };
		fg_rect cell = { 248, 344, 160, 224 };
		fg_px_rect t = fg_tile(cell, .04f, k, NULL, 0, bar, 2);
		int b = t.y + t.h;

		ck(fg_exact(b, k) && fg_exact(b + 5, k) && fg_exact(b + 9, k) && fg_exact(t.x, k) &&
		   fg_exact(t.x + t.w, k), "the bar's top and foot and the cover's edges land on whole pixels");
	}
	/* Some cells of other shapes (the grid's 129, 206, 282 widths, the
	 * home Grid's stills): every edge lands exact or a tenth out, never
	 * further than two pixels from where it would have been */
	{
		const int ring[2] = { 2, 4 };
		const float xs[5] = { 64, 217, 335.5f, 610, 900 }, ws[5] = { 129, 206, 282, 153, 104 };
		int far = 0;

		for (int i = 0; i < 5; i++) {
			fg_rect cell = { xs[i], 96 + i * 37.f, ws[i], ws[i] * 1.2f };
			fg_px_rect t = fg_tile(cell, .03f, k, ring, 2, NULL, 0);
			int g = (int)lroundf((fg_px(xs[i] + ws[i], k) - fg_px(xs[i], k)) * .03f / 2);

			if (abs(fg_px(xs[i], k) - g - t.x) > 2) far = 1;
		}
		ck(!far, "a cell of any shape moves two pixels at most to land exact");
	}
	/* A unit a pixel: nothing moves */
	{
		const int ring[2] = { 3, 6 };
		fg_rect cell = { 64, 96, 160, 224 };
		fg_px_rect t = fg_tile(cell, .03f, 1, ring, 2, NULL, 0);
		fg_rect u = fg_units(t, 1);
		int same = 1;

		for (int p = -20; p < 1024; p++) same &= fg_unit(p, 1) == p && fg_exact(p, 1);
		ck(same, "a unit a pixel: every edge is exact where it is");
		ck(t.x == 64 - 2 && t.w == 160 + 4 && t.y == 96 - 3 && t.h == 224 + 6 && u.x == t.x && u.w == t.w,
		   "a unit a pixel: the tile grows the same pixels each side, by its share");
	}
	/* The lengths, the glide, the step back */
	ck(fg_px_len(3, k) == 2 && fg_px_len(5, k) == 3 && fg_px_len(3, 1) == 3 && fg_px_len(0.2f, k) == 1,
	   "a mark's width: fine ring 2 pixels on the Pixel 2, 3 on the Brick, never under one");
	ck(fg_grow_at(.06f, 1) == .06f && fg_grow_at(.06f, 0) == 0 && fabsf(fg_grow_at(.06f, .5f) - .03f) < 1e-6f,
	   "a tile grows with its weight in the glide");
	ck(fabsf(fg_dim_cover(0) - .2f) < 1e-6f && fg_dim_cover(1) == 0 && fabsf(fg_dim_still(0) - .7f) < 1e-6f &&
	   fg_dim_still(1) == 1, "the others step back a fifth of the way to the ground, stills to 70%");
	return failed;
}
