/* SPDX-License-Identifier: GPL-3.0-only */
#include "cardart_draw.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float r, g, b; } rgbf;

static rgbf of(unsigned c)
{
	return (rgbf){ ((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f, (c & 255) / 255.f };
}

static rgbf of_px(Uint32 p) { return of(p & 0xFFFFFF); }

static float clamp1(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static Uint32 px(rgbf c)
{
	return 0xFF000000u | ((Uint32)(clamp1(c.r) * 255 + .5f) << 16) |
	       ((Uint32)(clamp1(c.g) * 255 + .5f) << 8) | (Uint32)(clamp1(c.b) * 255 + .5f);
}

static rgbf mix(rgbf a, rgbf b, float t)
{
	return (rgbf){ a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

static rgbf scale(rgbf a, float k) { return (rgbf){ a.r * k, a.g * k, a.b * k }; }

static Uint32 *row_of(SDL_Surface *s, int y) { return (Uint32 *)((Uint8 *)s->pixels + (size_t)y * s->pitch); }

static uint32_t next(uint32_t *st)
{
	*st ^= *st << 13;
	*st ^= *st >> 17;
	*st ^= *st << 5;
	return *st;
}

static float unit(uint32_t *st) { return (float)(next(st) & 0xffffff) / 16777216.f; }

/* Where along the gradient (x, y) is: 0 at its start, 1 at its end; `sa`
 * and `ca` the sine and cosine of its angle */
static float along(float sa, float ca, int W, int H, float x, float y)
{
	return clamp1(((x - W / 2.f) * sa + (y - H / 2.f) * ca) / (H * .9f) + .5f);
}

static void tri(SDL_Surface *s, const float *a, const float *b, const float *c, Uint32 color)
{
	int x0 = (int)fmaxf(0, floorf(fminf(a[0], fminf(b[0], c[0]))));
	int x1 = (int)fminf((float)s->w - 1, ceilf(fmaxf(a[0], fmaxf(b[0], c[0]))));
	int y0 = (int)fmaxf(0, floorf(fminf(a[1], fminf(b[1], c[1]))));
	int y1 = (int)fminf((float)s->h - 1, ceilf(fmaxf(a[1], fmaxf(b[1], c[1]))));
	float d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1]);

	if (fabsf(d) < 1e-3f) return;
	for (int y = y0; y <= y1; y++) {
		Uint32 *row = row_of(s, y);
		float py = y + .5f - c[1];

		for (int x = x0; x <= x1; x++) {
			float pxv = x + .5f - c[0];
			float l1 = ((b[1] - c[1]) * pxv + (c[0] - b[0]) * py) / d;
			float l2 = ((c[1] - a[1]) * pxv + (a[0] - c[0]) * py) / d;

			if (l1 >= -.001f && l2 >= -.001f && l1 + l2 <= 1.001f) row[x] = color;
		}
	}
}

/* The ground's light and vignette at (x, y), from the separable tables */
static rgbf lit(rgbf c, rgbf glow, float gl, float vig) { return scale(mix(c, glow, .22f * gl), vig); }

/* The hash's shape over `c` at (x, y), light, so the name still reads over
 * it: a broad band, a disc (on the CRT's era a sunset, its lower half cut by
 * lines that thicken toward its foot), or two stripes */
static rgbf shaped(rgbf c, int shape, ca_texture tex, rgbf lite, int x, int y, float cx, float cy,
                   float cs, float sn, float rad, float sw, int W, int scan)
{
	float v = -(x - cx) * sn + (y - cy) * cs, a = 0;

	if (shape == 0) {
		a = fabsf(v) < W * .16f ? .26f : 0;
	} else if (shape == 1) {
		float dx = x - cx, dy = y - cy;

		a = dx * dx + dy * dy < rad * rad ? .30f : 0;
		if (a > 0 && tex == CA_SCAN && dy > 0 &&
		    ((int)dy % (scan * 3)) > scan * 2 - (int)(dy / rad * scan * 4 / 3)) a = 0;
	} else {
		a = (fabsf(v - sw * 1.4f) < sw * .5f || fabsf(v + sw * 1.4f) < sw * .5f) ? .34f : 0;
	}
	return a > 0 ? mix(c, lite, a) : c;
}

void cardart_paint_ground(SDL_Surface *s, unsigned sys_rgb, ca_texture tex,
                          const ca_ground *g, SDL_Surface *art)
{
	static const int bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
	const int W = s->w, H = s->h;
	rgbf c0 = of(g->c0), c1 = of(g->c1), sys = of(sys_rgb), glow = mix(sys, (rgbf){ 1, 1, 1 }, .3f);
	float *gx = malloc(sizeof(float) * (size_t)W), *gy = malloc(sizeof(float) * (size_t)H);
	float *vx = malloc(sizeof(float) * (size_t)W), *vy = malloc(sizeof(float) * (size_t)H);
	const float sig = W * .45f, sa = sinf(g->angle), ca = cosf(g->angle);
	/* the textures' sizes are the width's, so a ground painted at half size
	 * (src/ui.c) and stretched is the one painted whole: 8 of 512 a dither
	 * cell and an LCD dot's pitch, 6 a scanline's period */
	const int cell = W / 64 > 2 ? W / 64 : 2, scan = W * 6 / 512 > 3 ? W * 6 / 512 : 3;

	if (!gx || !gy || !vx || !vy || SDL_LockSurface(s)) {
		free(gx); free(gy); free(vx); free(vy);
		return;
	}
	/* the light, a Gaussian, is a product of a row's and a column's; the
	 * vignette, 1 - (x^2 + y^2) / 2, a sum of them */
	for (int x = 0; x < W; x++) {
		float d = x - g->glow_x * W, v = (x - W / 2.f) / W;
		gx[x] = expf(-d * d / (2 * sig * sig));
		vx[x] = .5f * v * v;
	}
	for (int y = 0; y < H; y++) {
		float d = y - g->glow_y * H, v = (y - H / 2.f) / H;
		gy[y] = expf(-d * d / (2 * sig * sig));
		vy[y] = .5f * v * v;
	}
	const float scs = cosf(g->shape_angle), ssn = sinf(g->shape_angle);
	const float scx = g->shape_x * W, scy = g->shape_y * H, srad = g->shape_r * W, ssw = W * .045f;
	const rgbf lite = of(g->shape_rgb);

	for (int y = 0; y < H; y++) {
		Uint32 *row = row_of(s, y);

		for (int x = 0; x < W; x++) {
			rgbf c;

			if (tex == CA_DITHER) {
				/* four steps of the gradient, Bayer-dithered on 8-pixel cells */
				int cx = x / cell, cy = y / cell;
				float lv = along(sa, ca, W, H, cx * cell + cell / 2.f, cy * cell + cell / 2.f) * 3,
				      fr = lv - floorf(lv);
				int pick = (int)floorf(lv) + (fr * 16 > bayer[cy & 3][cx & 3] + .5f ? 1 : 0);

				c = mix(c0, c1, pick / 3.f);
			} else {
				c = mix(c0, c1, along(sa, ca, W, H, (float)x, (float)y));
			}
			c = lit(c, glow, gx[x] * gy[y], 1 - vx[x] - vy[y]);
			if (tex == CA_SCAN && (y % scan) >= scan * 2 / 3) c = scale(c, .72f);
			if (tex == CA_LCD) c = scale(c, (x % cell < cell - 1 && y % cell < cell - 1) ? 1.06f : .74f);
			if (tex != CA_SHARDS) c = shaped(c, g->shape, tex, lite, x, y, scx, scy, scs, ssn, srad, ssw, W, scan);
			row[x] = px(c);
		}
	}
	if (tex == CA_SHARDS) {
		/* low-poly: a jittered 4 x 5 grid, each cell two triangles, each the
		 * gradient at its middle, a little lighter or darker */
		enum { GX = 4, GY = 5 };
		float p[GY + 1][GX + 1][2];
		uint32_t st = g->seed;

		for (int j = 0; j <= GY; j++)
			for (int i = 0; i <= GX; i++) {
				float jx = (i == 0 || i == GX) ? 0 : (unit(&st) - .5f) * .7f;
				float jy = (j == 0 || j == GY) ? 0 : (unit(&st) - .5f) * .7f;

				p[j][i][0] = (i + jx) * W / (float)GX;
				p[j][i][1] = (j + jy) * H / (float)GY;
			}
		for (int j = 0; j < GY; j++)
			for (int i = 0; i < GX; i++) {
				bool flip = next(&st) & 1;
				const float *q[2][3] = {
					{ p[j][i], p[j][i + 1], flip ? p[j + 1][i] : p[j + 1][i + 1] },
					{ flip ? p[j][i + 1] : p[j][i], p[j + 1][i + 1], p[j + 1][i] },
				};

				for (int h = 0; h < 2; h++) {
					float mx = (q[h][0][0] + q[h][1][0] + q[h][2][0]) / 3;
					float my = (q[h][0][1] + q[h][1][1] + q[h][2][1]) / 3;
					int ix = (int)fminf(W - 1, fmaxf(0, mx)), iy = (int)fminf(H - 1, fmaxf(0, my));
					rgbf c = lit(mix(c0, c1, along(sa, ca, W, H, mx, my)), glow, gx[ix] * gy[iy],
					             1 - vx[ix] - vy[iy]);

					tri(s, q[h][0], q[h][1], q[h][2], px(scale(c, .84f + .3f * unit(&st))));
				}
			}
	}
	/* on the shards, drawn over the ground above, the shape is laid after */
	if (tex == CA_SHARDS)
		for (int y = 0; y < H; y++) {
			Uint32 *row = row_of(s, y);

			for (int x = 0; x < W; x++) {
				rgbf c = of_px(row[x]), d = shaped(c, g->shape, tex, lite, x, y, scx, scy, scs, ssn, srad, ssw, W, scan);

				if (d.r != c.r || d.g != c.g || d.b != c.b) row[x] = px(d);
			}
		}
	/* the system's own art, faint, bled off a corner */
	if (art && art->format->format == SDL_PIXELFORMAT_ARGB8888 && !SDL_LockSurface(art)) {
		int size = W * 300 / 512;
		int x0 = g->art_corner == 1 ? -size / 3 : W - size * 2 / 3;
		int y0 = g->art_corner == 2 ? -size / 4 : H - size * 3 / 4;

		for (int yy = 0; yy < size; yy++) {
			int dy = y0 + yy;
			const Uint32 *src;

			if (dy < 0 || dy >= H) continue;
			src = (const Uint32 *)((const Uint8 *)art->pixels + (size_t)(yy * art->h / size) * art->pitch);
			for (int xx = 0; xx < size; xx++) {
				int dx = x0 + xx;
				float a;

				if (dx < 0 || dx >= W) continue;
				a = (src[xx * art->w / size] >> 24) / 255.f * .13f;
				if (a > 0) row_of(s, dy)[dx] = px(mix(of_px(row_of(s, dy)[dx]), (rgbf){ 1, 1, 1 }, a));
			}
		}
		SDL_UnlockSurface(art);
	}
	SDL_UnlockSurface(s);
	free(gx); free(gy); free(vx); free(vy);
}

/* A box blur of radius `r`, three passes, on 8-bit channels by running sums */
static void blur(Uint32 *p, int W, int H, int r)
{
	Uint32 *t = malloc(sizeof(Uint32) * (size_t)W * H);

	if (!t || r < 1) { free(t); return; }
	for (int pass = 0; pass < 3; pass++) {
		for (int y = 0; y < H; y++) {
			int acc[3] = { 0, 0, 0 };

			for (int k = -r; k <= r; k++) {
				Uint32 v = p[y * W + (k < 0 ? 0 : k >= W ? W - 1 : k)];
				acc[0] += (v >> 16) & 255; acc[1] += (v >> 8) & 255; acc[2] += v & 255;
			}
			for (int x = 0; x < W; x++) {
				int xa = x + r + 1 >= W ? W - 1 : x + r + 1, xs = x - r < 0 ? 0 : x - r;
				Uint32 va = p[y * W + xa], vs = p[y * W + xs], n = (Uint32)(2 * r + 1);

				t[y * W + x] = 0xFF000000u | (acc[0] / n) << 16 | (acc[1] / n) << 8 | acc[2] / n;
				acc[0] += (int)((va >> 16) & 255) - (int)((vs >> 16) & 255);
				acc[1] += (int)((va >> 8) & 255) - (int)((vs >> 8) & 255);
				acc[2] += (int)(va & 255) - (int)(vs & 255);
			}
		}
		for (int x = 0; x < W; x++) {
			int acc[3] = { 0, 0, 0 };

			for (int k = -r; k <= r; k++) {
				Uint32 v = t[(k < 0 ? 0 : k >= H ? H - 1 : k) * W + x];
				acc[0] += (v >> 16) & 255; acc[1] += (v >> 8) & 255; acc[2] += v & 255;
			}
			for (int y = 0; y < H; y++) {
				int ya = y + r + 1 >= H ? H - 1 : y + r + 1, ys = y - r < 0 ? 0 : y - r;
				Uint32 va = t[ya * W + x], vs = t[ys * W + x], n = (Uint32)(2 * r + 1);

				p[y * W + x] = 0xFF000000u | (acc[0] / n) << 16 | (acc[1] / n) << 8 | acc[2] / n;
				acc[0] += (int)((va >> 16) & 255) - (int)((vs >> 16) & 255);
				acc[1] += (int)((va >> 8) & 255) - (int)((vs >> 8) & 255);
				acc[2] += (int)(va & 255) - (int)(vs & 255);
			}
		}
	}
	free(t);
}

bool cardart_paint_frame(SDL_Surface *s, SDL_Surface *frame, unsigned sys_rgb)
{
	/* blurred at a quarter of the card's size and stretched back: the same
	 * blur, a sixteenth of the work */
	const int SW = s->w / 4, SH = s->h / 4;
	SDL_Surface *fr, *small;
	rgbf tint = scale(of(sys_rgb), .22f);
	float k;
	int cw, ch;

	if (!frame || frame->w < 16 || frame->h < 16) return false;
	if (!(fr = SDL_ConvertSurfaceFormat(frame, SDL_PIXELFORMAT_ARGB8888, 0))) return false;
	if (!(small = SDL_CreateRGBSurfaceWithFormat(0, SW, SH, 32, SDL_PIXELFORMAT_ARGB8888))) {
		SDL_FreeSurface(fr);
		return false;
	}
	k = fmaxf((float)SW / fr->w, (float)SH / fr->h);
	cw = (int)(SW / k);
	ch = (int)(SH / k);
	SDL_SetSurfaceBlendMode(fr, SDL_BLENDMODE_NONE);
	if (SDL_SoftStretchLinear(fr, &(SDL_Rect){ (fr->w - cw) / 2, (fr->h - ch) / 2, cw, ch }, small, NULL)) {
		SDL_FreeSurface(fr);
		SDL_FreeSurface(small);
		return false;
	}
	SDL_FreeSurface(fr);
	if (!SDL_LockSurface(small)) {
		blur((Uint32 *)small->pixels, SW, SH, 2);
		/* dimmed toward the system's color, darker toward the foot: here,
		 * on a sixteenth of the pixels, before the stretch */
		for (int y = 0; y < SH; y++) {
			Uint32 *row = row_of(small, y);
			float foot = clamp1((y - SH * .45f) / (SH * .55f)), dark = 1 - .45f * foot * foot;

			for (int x = 0; x < SW; x++)
				row[x] = px(scale(mix(scale(of_px(row[x]), .52f), tint, .28f), dark));
		}
		SDL_UnlockSurface(small);
	}
	SDL_SetSurfaceBlendMode(small, SDL_BLENDMODE_NONE);
	if (SDL_SoftStretchLinear(small, NULL, s, NULL)) {
		SDL_FreeSurface(small);
		return false;
	}
	SDL_FreeSurface(small);
	return true;
}

void cardart_paint_band(SDL_Surface *s, int y0, int y1, int feather)
{
	if (SDL_LockSurface(s)) return;
	for (int y = y0 - feather; y < y1 + feather; y++) {
		int in, k;
		Uint32 *row;

		if (y < 0 || y >= s->h) continue;
		in = y - (y0 - feather) < (y1 + feather) - y ? y - (y0 - feather) : (y1 + feather) - y;
		if (in > feather) in = feather;
		/* 256 at the edge of the feather, 123 (0.48) inside */
		k = 256 - 133 * in / (feather > 0 ? feather : 1);
		row = row_of(s, y);
		for (int x = 0; x < s->w; x++) {
			Uint32 p = row[x];

			row[x] = (p & 0xFF000000u) | ((((p >> 16) & 255) * (Uint32)k >> 8) << 16) |
			         ((((p >> 8) & 255) * (Uint32)k >> 8) << 8) | ((p & 255) * (Uint32)k >> 8);
		}
	}
	SDL_UnlockSurface(s);
}
