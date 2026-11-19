/* SPDX-License-Identifier: GPL-3.0-only */
/* The Border behind a game. See src/border.h. */
#include "border.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One pixel of the frame as 8-bit R, G, B */
static void px(const uint8_t *row, int x, bool rgb565, unsigned *r, unsigned *g, unsigned *b)
{
	if (rgb565) {
		unsigned v = (unsigned)row[x * 2] | ((unsigned)row[x * 2 + 1] << 8);

		*r = ((v >> 11) & 31) * 255 / 31;
		*g = ((v >> 5) & 63) * 255 / 63;
		*b = (v & 31) * 255 / 31;
	} else {
		*b = row[x * 4];
		*g = row[x * 4 + 1];
		*r = row[x * 4 + 2];
	}
}

/* A 3x3 box over the small picture, its edges repeated */
static void box3(float *c)
{
	static float t[BORDER_BLUR_W * BORDER_BLUR_H * 3];
	int x, y, k;

	for (y = 0; y < BORDER_BLUR_H; y++)
		for (x = 0; x < BORDER_BLUR_W; x++)
			for (k = 0; k < 3; k++) {
				float s = 0;
				int dx, dy;

				for (dy = -1; dy <= 1; dy++)
					for (dx = -1; dx <= 1; dx++) {
						int xx = x + dx, yy = y + dy;

						xx = xx < 0 ? 0 : xx >= BORDER_BLUR_W ? BORDER_BLUR_W - 1 : xx;
						yy = yy < 0 ? 0 : yy >= BORDER_BLUR_H ? BORDER_BLUR_H - 1 : yy;
						s += c[(yy * BORDER_BLUR_W + xx) * 3 + k];
					}
				t[(y * BORDER_BLUR_W + x) * 3 + k] = s / 9.0f;
			}
	memcpy(c, t, sizeof t);
}

bool border_blur(const void *frame, int w, int h, size_t pitch, bool rgb565, int surf_w, int surf_h,
                 uint8_t out[BORDER_BLUR_W * BORDER_BLUR_H * 4])
{
	static float c[BORDER_BLUR_W * BORDER_BLUR_H * 3];
	int cx0, cy0, cw, ch, x, y, i;

	if (!frame || w <= 0 || h <= 0 || surf_w <= 0 || surf_h <= 0) return false;
	/* The frame's middle in the surface's shape */
	if ((long)w * surf_h > (long)h * surf_w) {
		ch = h;
		cw = (int)((long)h * surf_w / surf_h);
	} else {
		cw = w;
		ch = (int)((long)w * surf_h / surf_w);
	}
	if (cw < 1) cw = 1;
	if (ch < 1) ch = 1;
	cx0 = (w - cw) / 2;
	cy0 = (h - ch) / 2;
	/* Each cell, the average of the frame's pixels it covers; a sample a
	 * cell's step apart in each direction is plenty for a picture this
	 * small and blurred after (up to 4x4 a cell) */
	for (y = 0; y < BORDER_BLUR_H; y++)
		for (x = 0; x < BORDER_BLUR_W; x++) {
			int fx0 = cx0 + x * cw / BORDER_BLUR_W, fx1 = cx0 + (x + 1) * cw / BORDER_BLUR_W;
			int fy0 = cy0 + y * ch / BORDER_BLUR_H, fy1 = cy0 + (y + 1) * ch / BORDER_BLUR_H;
			/* At most 4 x 4 samples a cell, whatever the frame's size */
			int sx = (fx1 - fx0 + 3) / 4 > 1 ? (fx1 - fx0 + 3) / 4 : 1;
			int sy = (fy1 - fy0 + 3) / 4 > 1 ? (fy1 - fy0 + 3) / 4 : 1;
			unsigned long sr = 0, sg = 0, sb = 0, n = 0;
			int fx, fy;

			if (fx1 <= fx0) fx1 = fx0 + 1;
			if (fy1 <= fy0) fy1 = fy0 + 1;
			for (fy = fy0; fy < fy1 && fy < h; fy += sy) {
				const uint8_t *row = (const uint8_t *)frame + (size_t)fy * pitch;

				for (fx = fx0; fx < fx1 && fx < w; fx += sx) {
					unsigned r, g, b;

					px(row, fx, rgb565, &r, &g, &b);
					sr += r; sg += g; sb += b; n++;
				}
			}
			i = (y * BORDER_BLUR_W + x) * 3;
			c[i] = n ? (float)sr / n : 0;
			c[i + 1] = n ? (float)sg / n : 0;
			c[i + 2] = n ? (float)sb / n : 0;
		}
	box3(c);
	box3(c);
	/* Stronger colors (1.3 from their grey), then darker (0.55) */
	for (i = 0; i < BORDER_BLUR_W * BORDER_BLUR_H; i++) {
		float r = c[i * 3], g = c[i * 3 + 1], b = c[i * 3 + 2];
		float l = 0.299f * r + 0.587f * g + 0.114f * b;
		float v[3] = { l + (r - l) * 1.3f, l + (g - l) * 1.3f, l + (b - l) * 1.3f };
		int k;

		for (k = 0; k < 3; k++) {
			float q = v[k] * 0.55f;

			v[k] = q < 0 ? 0 : q > 255 ? 255 : q;
		}
		out[i * 4]     = (uint8_t)(v[2] + 0.5f);
		out[i * 4 + 1] = (uint8_t)(v[1] + 0.5f);
		out[i * 4 + 2] = (uint8_t)(v[0] + 0.5f);
		out[i * 4 + 3] = 255;
	}
	return true;
}

uint8_t *border_read(const char *path, int *w, int *h)
{
	unsigned char hdr[8];
	uint8_t *px_ = NULL;
	FILE *f;
	size_t need, i;

	if (!path || !*path || !(f = fopen(path, "rb"))) return NULL;
	if (fread(hdr, 1, sizeof hdr, f) != sizeof hdr || memcmp(hdr, "DTOV", 4) != 0) goto out;
	*w = hdr[4] | (hdr[5] << 8);
	*h = hdr[6] | (hdr[7] << 8);
	if (*w <= 0 || *h <= 0 || *w > BORDER_MAX_W || *h > BORDER_MAX_H) goto out;
	need = (size_t)*w * (size_t)*h * 4;
	if (!(px_ = malloc(need))) goto out;
	if (fread(px_, 1, need, f) != need) {
		free(px_);
		px_ = NULL;
		goto out;
	}
	for (i = 3; i < need; i += 4) px_[i] = 255;
out:
	fclose(f);
	return px_;
}
