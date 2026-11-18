/* SPDX-License-Identifier: GPL-3.0-only */
/* See screenfx.h. The arithmetic is diatom/port/pixel2.c's GLSL for a white
 * picture, line for line as tools/shaderdemo.c ports it to C, so the two can
 * be held to each other (make check-screenfx). */
#include "screenfx.h"

#include <math.h>
#include <string.h>

static float stepf(float edge, float x) { return x < edge ? 0.0f : 1.0f; }
static float mixf(float a, float b, float t) { return a * (1.0f - t) + b * t; }
static float fractf(float x) { return x - floorf(x); }
static float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }

static unsigned char byte_of(float v)
{
	return (unsigned char)(clampf(v, 0.0f, 1.0f) * 255.0f + 0.5f);
}

screenfx_kind screenfx_kind_of(const char *name)
{
	static const char *const NAME[SFX_KINDS] = { "", "crt", "crt-curved", "scanlines", "lcd" };
	int k;

	if (!name) return SFX_OFF;
	for (k = 1; k < SFX_KINDS; k++)
		if (!strcmp(name, NAME[k])) return (screenfx_kind)k;
	return SFX_OFF;
}

bool screenfx_curved(screenfx_kind k)
{
	return k == SFX_CRT_CURVED;
}

void screenfx_warp(float x, float y, float *u, float *v)
{
	float qx = x * 2.0f - 1.0f, qy = y * 2.0f - 1.0f;
	float mx = 1.0f + qy * qy * 0.02f, my = 1.0f + qx * qx * 0.015f;

	*u = (qx * mx + 1.0f) * 0.5f;
	*v = (qy * my + 1.0f) * 0.5f;
}

static float crt_lines(float posy, float vin);
static void crt_finish(float l, float vin, int x, float strength, unsigned char *px);

/* CRT Curved's tables, made once a mask: the two scanlines' light by where
 * a pixel falls between them (0 to 1, in LINE_STEPS), and the finished
 * pixel by that light (0 to 1, the most two lines give, in LIGHT_STEPS) and
 * its grille column, inside the curve's dark edge */
#define LINE_STEPS 4096
#define LIGHT_STEPS 4096
static float g_line_lut[LINE_STEPS + 1];
static unsigned char g_light_lut[3][LIGHT_STEPS][4];
/* Each column's place across the glass, -1 to 1, and its square, before
 * the curve: CRT Curved's, once a mask, for a screen up to COLS_MAX wide;
 * Scanlines' and the LCD's light of each column in the first */
#define COLS_MAX 2048
static float g_col_q[COLS_MAX], g_col_q2[COLS_MAX];

static void curved_tables(float strength)
{
	int i, g;

	for (i = 0; i <= LINE_STEPS; i++)
		g_line_lut[i] = crt_lines(0.5f + (float)i / (float)LINE_STEPS, 1.0f);
	for (g = 0; g < 3; g++)
		for (i = 0; i < LIGHT_STEPS; i++)
			crt_finish((float)i / (float)(LIGHT_STEPS - 1), 1.0f, g, strength, g_light_lut[g][i]);
}

/* Scanlines' and the LCD's band along one axis (GRID_SHADER_SRC): how much
 * of pixel i is lit, the old screen's pixel `s` screen pixels across. Its
 * `t` (which texel it reads) does not matter for a white picture. */
static float grid_axis(int i, float s, float ratio, float opacity, float strength)
{
	const float RATIO = 1.0f - ratio, ALPHA = 1.0f - opacity * strength, inv = 1.0f / s;
	const float p = (float)i + 0.5f;
	float tl = inv * floorf(p), br = inv * ceilf(p);
	float t_tl = floorf(tl), t_br = floorf(br), ftl = fractf(tl), fbr = fractf(br);
	float ff = (fbr + clampf(RATIO - ftl, 0.0f, 1.0f)) * s;

	ff = mixf(ff, RATIO * s, stepf(RATIO, fbr));
	ff = mixf(ff, clampf((RATIO - ftl) * s, 0.0f, 1.0f), stepf(t_br, t_tl));
	return ALPHA + (1.0f - ALPHA) * ff;
}

/* The CRT's light at one pixel for a white picture, before the grille and
 * the tone: its two scanlines' windows at `posy` (in lines), times the
 * curve's dark edge `vin` */
static float crt_lines(float posy, float vin)
{
	const float THIN = 0.75f;
	float y0 = floorf(posy - 0.5f) + 0.5f;
	float off = posy - y0;
	float sa = cosf(fminf(0.5f, off * THIN) * 6.2831853f) * 0.5f + 0.5f;
	float sb = cosf(fminf(0.5f, (1.0f - off) * THIN) * 6.2831853f) * 0.5f + 0.5f;

	return (sa + sb) * vin;
}

/* The grille, the tone and the strength on the CRT's light `l` at screen
 * column x, into px (its three colors and 255). The grille's column is
 * fract((x + 0.5) / 3), which repeats every three columns exactly. */
static void crt_finish(float l, float vin, int x, float strength, unsigned char *px)
{
	const float DARK = 0.5f, TX = 0.419207f, TY = 0.580793f;
	float c[3] = { l, l, l };
	float mm = fractf(((float)x + 0.5f) * (1.0f / 3.0f));
	float peak, k;
	int ch;

	if (mm < 1.0f / 3.0f) c[0] *= DARK; else if (mm < 2.0f / 3.0f) c[1] *= DARK; else c[2] *= DARK;
	peak = fmaxf(1.0f / (256.0f * 65536.0f), fmaxf(c[0], fmaxf(c[1], c[2])));
	k = 1.0f / peak * (peak / (peak * TX + TY));
	for (ch = 0; ch < 3; ch++) {
		c[ch] *= k;
		/* Under 100%: toward the plain picture, white times the curve's
		 * edge (CRT_SOFT_MIX, whose mix(a, b, qy) is white) */
		if (strength < 1.0f) c[ch] = mixf(vin, c[ch], strength);
		px[ch] = byte_of(sqrtf(fmaxf(c[ch], 0.0f)));
	}
	px[3] = 255;
}

void screenfx_mask_grid(screenfx_kind k, int strength, int w, int h,
                        float cols, float lines, float phase, unsigned char *rgba)
{
	const float s = (float)(strength < 10 ? 10 : strength > 100 ? 100 : strength) / 100.0f;
	const float sx = (float)w / cols, sy = (float)h / lines;
	int x, y;

	if (k == SFX_OFF || k >= SFX_KINDS || w <= 0 || h <= 0) return;
	if (k == SFX_CRT_CURVED) {
		if (w > COLS_MAX) return;
		curved_tables(s);
		for (x = 0; x < w; x++) {
			g_col_q[x] = ((float)x + 0.5f) / (cols * sx) * 2.0f - 1.0f;
			g_col_q2[x] = g_col_q[x] * g_col_q[x];
		}
	}
	if (k == SFX_SCANLINES || k == SFX_LCD) {
		const float r = k == SFX_LCD ? 0.3f : 0.0f;

		if (w > COLS_MAX) return;
		/* Separable: a row's light times a column's, each column's
		 * worked out once */
		for (x = 0; x < w; x++) g_col_q[x] = grid_axis(x, sx, r, r, s);
		for (y = 0; y < h; y++) {
			float fy = grid_axis(y, sy, 0.3f, 0.3f, s);
			unsigned char *row = rgba + (size_t)y * (size_t)w * 4;

			for (x = 0; x < w; x++) {
				unsigned char v = byte_of(g_col_q[x] * fy);

				row[x * 4 + 0] = row[x * 4 + 1] = row[x * 4 + 2] = v;
				row[x * 4 + 3] = 255;
			}
		}
		return;
	}
	for (y = 0; y < h; y++) {
		unsigned char *row = rgba + (size_t)y * (size_t)w * 4;
		float py = (float)y + 0.5f;

		if (k == SFX_CRT) {
			/* Flat glass: a row's light is one value, and the grille
			 * repeats every three columns */
			unsigned char three[3][4];
			float l = crt_lines(py / sy + phase, 1.0f);

			for (x = 0; x < 3 && x < w; x++) crt_finish(l, 1.0f, x, s, three[x]);
			for (x = 0; x < w; x++) memcpy(row + x * 4, three[x % 3], 4);
			continue;
		}
		{
			/* Curved: the same, at the curve's point, with its dark edge.
			 * A column's place across is worked out once a mask and a
			 * row's once a row; inside the edge the tables do the rest:
			 * a pixel at a time with a cosine and a root, a mask took
			 * 148 ms on the Pixel 2, and 60 with the tables alone */
			float qy0 = py / (lines * sy) * 2.0f - 1.0f;
			float mx = 1.0f + qy0 * qy0 * 0.02f;
			int g = 0;

			for (x = 0; x < w; x++, g = g == 2 ? 0 : g + 1) {
				float qx = g_col_q[x] * mx, qy = qy0 * (1.0f + g_col_q2[x] * 0.015f);
				float vin, l, posy, y0;
				int i;

				vin = (1.0f - (1.0f - clampf(qx * qx, 0.0f, 1.0f)) * (1.0f - clampf(qy * qy, 0.0f, 1.0f))) * 1.001f;
				vin = clampf(lines - vin * lines, 0.0f, 1.0f);
				posy = (qy + 1.0f) * 0.5f * lines + phase;
				if (vin < 1.0f) {
					/* The edge's soft line, worked out whole */
					crt_finish(crt_lines(posy, vin), vin, x, s, row + x * 4);
					continue;
				}
				y0 = floorf(posy - 0.5f) + 0.5f;
				i = (int)((posy - y0) * (float)LINE_STEPS + 0.5f);
				l = g_line_lut[i > LINE_STEPS ? LINE_STEPS : i];
				i = (int)(l * (float)(LIGHT_STEPS - 1) + 0.5f);
				memcpy(row + x * 4, g_light_lut[g][i < 0 ? 0 : i >= LIGHT_STEPS ? LIGHT_STEPS - 1 : i], 4);
			}
		}
	}
}

void screenfx_mask(screenfx_kind k, int strength, int w, int h, unsigned char *rgba)
{
	float sy = (float)h / (float)SCREENFX_LINES;

	screenfx_mask_grid(k, strength, w, h, (float)SCREENFX_COLS, (float)SCREENFX_LINES,
	                   0.5f - 0.5f / sy, rgba);
}
