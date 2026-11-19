/* SPDX-License-Identifier: GPL-3.0-only */
/* See covercolor.h. */
#include "covercolor.h"

#include <math.h>
#include <string.h>

#define SAMPLES 64                    /* a grid this wide, at most */
#define HUES    24                    /* 15 degrees each */
/* A sample with less than this saturation or value has no hue to count */
#define S_MIN   0.22f
#define V_MIN   0.18f
/* The vivid samples' weight, against all of them, under which the cover
 * is taken as grey: a photograph in black and white, a white sleeve */
#define VIVID_MIN 0.06f

static void hsv(unsigned r, unsigned g, unsigned b, float *h, float *s, float *v)
{
	unsigned mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
	unsigned mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
	float d = (float)(mx - mn);

	*v = (float)mx / 255.0f;
	*s = mx ? d / (float)mx : 0.0f;
	if (d <= 0.0f)      *h = 0.0f;
	else if (mx == r)   *h = fmodf(((float)g - (float)b) / d + 6.0f, 6.0f);
	else if (mx == g)   *h = ((float)b - (float)r) / d + 2.0f;
	else                *h = ((float)r - (float)g) / d + 4.0f;
	*h *= 60.0f;
}

unsigned cover_main_rgb(const uint32_t *px, int w, int h, int pitch)
{
	double wt[HUES], sum[HUES][3], all[3] = { 0, 0, 0 }, vivid = 0;
	int step_x = w > SAMPLES ? w / SAMPLES : 1, step_y = h > SAMPLES ? h / SAMPLES : 1;
	int x, y, n = 0, best = -1, b, k;
	double top = 0, acc[3] = { 0, 0, 0 }, aw = 0;

	memset(wt, 0, sizeof wt);
	memset(sum, 0, sizeof sum);
	for (y = step_y / 2; y < h; y += step_y) {
		const uint32_t *row = (const uint32_t *)((const uint8_t *)px + (long)y * pitch);

		for (x = step_x / 2; x < w; x += step_x) {
			uint32_t p = row[x];
			unsigned r = (p >> 16) & 255, g = (p >> 8) & 255, bl = p & 255;
			float hh, s, v, q;

			if ((p >> 24) < 128) continue;
			n++;
			all[0] += r; all[1] += g; all[2] += bl;
			hsv(r, g, bl, &hh, &s, &v);
			if (s < S_MIN || v < V_MIN) continue;
			/* The more vivid, the more it counts - a red title more than
			 * the faint warmth of a beige sleeve - but gently, so the color
			 * of most of the cover is still the main one: a muted purple on
			 * three fifths of it over a bright orange on the rest */
			q = sqrtf(s * v);
			b = (int)(hh / (360.0f / HUES)) % HUES;
			wt[b] += q;
			sum[b][0] += q * r; sum[b][1] += q * g; sum[b][2] += q * bl;
			vivid += q;
		}
	}
	if (!n) return 0x808080;
	if (vivid < VIVID_MIN * n)
		return ((unsigned)(all[0] / n + 0.5) << 16) | ((unsigned)(all[1] / n + 0.5) << 8) |
		       (unsigned)(all[2] / n + 0.5);
	/* The hue with the most, its neighbours counting half, so a color on
	 * the line between two buckets is not split in two and lost */
	for (b = 0; b < HUES; b++) {
		double score = wt[b] + 0.5 * (wt[(b + HUES - 1) % HUES] + wt[(b + 1) % HUES]);

		if (score > top) { top = score; best = b; }
	}
	for (k = -1; k <= 1; k++) {
		int c = (best + k + HUES) % HUES;

		acc[0] += sum[c][0]; acc[1] += sum[c][1]; acc[2] += sum[c][2];
		aw += wt[c];
	}
	return ((unsigned)(acc[0] / aw + 0.5) << 16) | ((unsigned)(acc[1] / aw + 0.5) << 8) |
	       (unsigned)(acc[2] / aw + 0.5);
}

static float hue_rgb(float p, float q, float t)
{
	if (t < 0) t += 1;
	if (t > 1) t -= 1;
	if (t < 1.0f / 6) return p + (q - p) * 6 * t;
	if (t < 0.5f) return q;
	if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
	return p;
}

unsigned cover_rgb_darken(unsigned rgb, float l, float s_max)
{
	float r = (float)((rgb >> 16) & 255) / 255, g = (float)((rgb >> 8) & 255) / 255,
	      b = (float)(rgb & 255) / 255;
	float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b));
	float hl = (mx + mn) / 2, d = mx - mn, s = 0, hh = 0, q, p;

	if (d > 0) {
		s = hl > 0.5f ? d / (2 - mx - mn) : d / (mx + mn);
		if (mx == r)      hh = (g - b) / d + (g < b ? 6 : 0);
		else if (mx == g) hh = (b - r) / d + 2;
		else              hh = (r - g) / d + 4;
		hh /= 6;
	}
	if (s > s_max) s = s_max;
	if (hl > l) hl = l;
	q = hl < 0.5f ? hl * (1 + s) : hl + s - hl * s;
	p = 2 * hl - q;
	r = hue_rgb(p, q, hh + 1.0f / 3);
	g = hue_rgb(p, q, hh);
	b = hue_rgb(p, q, hh - 1.0f / 3);
	return ((unsigned)(r * 255 + 0.5f) << 16) | ((unsigned)(g * 255 + 0.5f) << 8) |
	       (unsigned)(b * 255 + 0.5f);
}
