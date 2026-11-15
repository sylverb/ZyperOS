/* SPDX-License-Identifier: GPL-3.0-only */
/* Signed distances, negative inside, for marks drawn as distance fields: the
 * status corner's (src/status.c) and the cover badges' (src/artbadge.c),
 * which each carried these same lines. Header-only, so a check that builds
 * one of those files needs nothing more. src/osdbar.c keeps its own: Diatom
 * carries that file whole (diatom/tools/check-copies.py). Part of the design
 * system (src/ds/, DESIGN.md "Where each part lives"). */
#ifndef ZYPEROS_DS_SDF_H
#define ZYPEROS_DS_SDF_H

#include <math.h>

static inline float ds_sd_clamp(float v, float lo, float hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}

/* To the segment (x0, y0)-(x1, y1) */
static inline float ds_sd_segment(float x, float y, float x0, float y0, float x1, float y1)
{
	float dx = x1 - x0, dy = y1 - y0;
	float len2 = dx * dx + dy * dy;
	float t = len2 > 0 ? ds_sd_clamp(((x - x0) * dx + (y - y0) * dy) / len2, 0, 1) : 0;
	float ex = x - (x0 + dx * t), ey = y - (y0 + dy * t);

	return sqrtf(ex * ex + ey * ey);
}

/* To the polygon of `n` points p (x, y pairs); inside by crossings, so one
 * that is not convex (a bolt, a heart's notch) is still exact */
static inline float ds_sd_poly(const float *p, int n, float x, float y)
{
	float d = 1e9f, sign = 1;
	int i, j;

	for (i = 0, j = n - 1; i < n; j = i, i++) {
		float vx = p[i * 2], vy = p[i * 2 + 1];
		float ex = p[j * 2] - vx, ey = p[j * 2 + 1] - vy;
		float wx = x - vx, wy = y - vy;
		float t = ds_sd_clamp((wx * ex + wy * ey) / (ex * ex + ey * ey), 0, 1);
		float bx = wx - ex * t, by = wy - ey * t;
		int up = y >= vy, down = y < vy + ey, left = ex * wy > ey * wx;

		if (bx * bx + by * by < d) d = bx * bx + by * by;
		if ((up && down && left) || (!up && !down && !left)) sign = -sign;
	}
	return sign * sqrtf(d);
}

/* To the box (x0, y0)-(x1, y1) with corners of radius r */
static inline float ds_sd_rbox(float x, float y, float x0, float y0, float x1, float y1, float r)
{
	float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
	float hx = (x1 - x0) / 2 - r, hy = (y1 - y0) / 2 - r;
	float qx = fabsf(x - cx) - hx, qy = fabsf(y - cy) - hy;
	float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
	float in = qx > qy ? qx : qy;

	return sqrtf(ox * ox + oy * oy) + (in < 0 ? in : 0) - r;
}

#endif
