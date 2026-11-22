/* SPDX-License-Identifier: GPL-3.0-only */
/* The focus's marks on whole panel pixels (src/focusgeo.h). */
#include "focusgeo.h"

#include <math.h>

/* Over each 8 units of the Pixel 2's picture, the unit each of the 5 pixel
 * edges in them is drawn at: 0, 2 and 3 exact, 1 and 4 a tenth out */
static const int PX2_EDGE[5] = { 0, 2, 3, 5, 6 };

static bool is_px2(float k)
{
	return fabsf(k - FG_K_PIXEL2) < 1e-4f;
}

static bool is_one(float k)
{
	return fabsf(k - 1.0f) < 1e-4f;
}

int fg_px(float u, float k)
{
	return (int)lroundf(u * k);
}

int fg_px_len(float u, float k)
{
	int n = fg_px(u, k);

	return n < 1 ? 1 : n;
}

int fg_unit(int p, float k)
{
	if (is_px2(k)) {
		int n = p >= 0 ? p / 5 : -((4 - p) / 5);

		return 8 * n + PX2_EDGE[p - 5 * n];
	}
	if (is_one(k) || k <= 0) return p;
	return (int)lroundf(p / k);
}

bool fg_exact(int p, float k)
{
	if (is_px2(k)) {
		int r = ((p % 5) + 5) % 5;

		return r == 0 || r == 2 || r == 3;
	}
	return is_one(k);
}

int fg_edge(int p, int dir, const int *off, int n, float k)
{
	static const int TRY[5] = { 0, -1, 1, -2, 2 };
	int best = p, best_n = -1;

	if (is_one(k)) return p;
	for (int t = 0; t < 5; t++) {
		int q = p + TRY[t], good = fg_exact(q, k);

		for (int i = 0; i < n; i++) good += fg_exact(q + dir * off[i], k);
		if (good > best_n) { best_n = good; best = q; }
	}
	return best;
}

fg_px_rect fg_tile(fg_rect cell, float grow, float k, const int *side_out, int side_n,
                   const int *foot_out, int foot_n)
{
	int l = fg_px(cell.x, k), r = fg_px(cell.x + cell.w, k);
	int t = fg_px(cell.y, k), b = fg_px(cell.y + cell.h, k);
	int gx = (int)lroundf((r - l) * grow / 2), gy = (int)lroundf((b - t) * grow / 2);
	fg_px_rect q;

	l = fg_edge(l - gx, -1, side_out, side_n, k);
	r = fg_edge(r + gx, 1, side_out, side_n, k);
	t = fg_edge(t - gy, -1, side_out, side_n, k);
	b = foot_n > 0 ? fg_edge(b + gy, 1, foot_out, foot_n, k) : fg_edge(b + gy, 1, side_out, side_n, k);
	q.x = l; q.y = t; q.w = r - l; q.h = b - t;
	return q;
}

fg_rect fg_units(fg_px_rect p, float k)
{
	int x = fg_unit(p.x, k), y = fg_unit(p.y, k);

	return (fg_rect){ (float)x, (float)y, (float)(fg_unit(p.x + p.w, k) - x),
	                  (float)(fg_unit(p.y + p.h, k) - y) };
}

float fg_grow_at(float grow, float w)
{
	return grow * (w < 0 ? 0 : w > 1 ? 1 : w);
}

float fg_dim_cover(float w)
{
	return .20f * (1 - (w < 0 ? 0 : w > 1 ? 1 : w));
}

float fg_dim_still(float w)
{
	return 1 - .30f * (1 - (w < 0 ? 0 : w > 1 ? 1 : w));
}
