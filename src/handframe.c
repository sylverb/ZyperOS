/* SPDX-License-Identifier: GPL-3.0-only */
/* A handheld's frame around its game. See src/handframe.h. */
#include "handframe.h"

#include <math.h>
#include <string.h>

bool hf_place(const hf_model *m, hf_kind kind, int w, int h, int *scale, int *gx, int *gy, int *gw, int *gh)
{
	int s;

	if (!m || m->cols <= 0 || m->lines <= 0 || w <= 0 || h <= 0) return false;
	s = w / m->cols < h / m->lines ? w / m->cols : h / m->lines;
	if (kind == HF_WHOLE) s--;
	if (s < 1) return false;
	*scale = s;
	*gw = m->cols * s;
	*gh = m->lines * s;
	if (kind == HF_CUT) {
		/* Where Sharp Pixels puts it: the middle, rounded down, as
		 * diatom/src/scale.c's centred() */
		*gx = (w - *gw) / 2;
		*gy = (h - *gh) / 2;
	} else {
		/* The lens's middle on the panel's, the game where the lens has it,
		 * then held on the panel */
		const float px = (float)*gw / m->lcd_w, py = (float)*gh / m->lcd_h;
		const float cx = (m->lens.x0 + m->lens.x1) / 2.0f, cy = (m->lens.y0 + m->lens.y1) / 2.0f;

		*gx = (int)lroundf((float)w / 2.0f - cx * px);
		*gy = (int)lroundf((float)h / 2.0f - cy * py);
		if (*gx < 0) *gx = 0;
		if (*gy < 0) *gy = 0;
		if (*gx + *gw > w) *gx = w - *gw;
		if (*gy + *gh > h) *gy = h - *gh;
	}
	return true;
}

void hf_px_rect(const hf_model *m, const hf_rrect *r, int gx, int gy, int gw, int gh,
                int *x0, int *y0, int *x1, int *y1)
{
	const float px = (float)gw / m->lcd_w, py = (float)gh / m->lcd_h;

	*x0 = gx + (int)lroundf(r->x0 * px);
	*y0 = gy + (int)lroundf(r->y0 * py);
	*x1 = gx + (int)lroundf(r->x1 * px);
	*y1 = gy + (int)lroundf(r->y1 * py);
}

/* How much of pixel (x, y) the rounded rect covers, 0 to 1: whole inside its
 * straight edges, by distance from the centre of a corner's arc in a corner */
static float cover(int x, int y, int x0, int y0, int x1, int y1, const float rad[4])
{
	const float cx = (float)x + 0.5f, cy = (float)y + 0.5f;
	int k;

	if (x < x0 || x >= x1 || y < y0 || y >= y1) return 0.0f;
	for (k = 0; k < 4; k++) {
		const float r = rad[k];
		/* The arc's centre for corner k: top left, top right, bottom right,
		 * bottom left */
		const float ax = k == 0 || k == 3 ? (float)x0 + r : (float)x1 - r;
		const float ay = k < 2 ? (float)y0 + r : (float)y1 - r;
		const bool in_x = k == 0 || k == 3 ? cx < ax : cx > ax;
		const bool in_y = k < 2 ? cy < ay : cy > ay;

		if (r > 0.0f && in_x && in_y) {
			const float d = sqrtf((cx - ax) * (cx - ax) + (cy - ay) * (cy - ay));
			const float c = r - d + 0.5f;

			return c <= 0.0f ? 0.0f : c >= 1.0f ? 1.0f : c;
		}
	}
	return 1.0f;
}

static hf_rgb paint_of(const hf_model *m, const hf_part *p, int color)
{
	if (p->paint == HF_BODY) return m->body[color];
	if (p->paint == HF_BODY_DARK) return m->body_dark[color];
	return p->color;
}

void hf_render(const hf_model *m, int color, int w, int h, int gx, int gy, int gw, int gh, uint8_t *bgra)
{
	const float px = (float)gw / m->lcd_w, py = (float)gh / m->lcd_h;
	int i, x, y;

	if (color < 0 || color >= m->ncolors) color = 0;
	/* The body, the whole panel */
	for (i = 0; i < w * h; i++) {
		bgra[i * 4]     = m->body[color].b;
		bgra[i * 4 + 1] = m->body[color].g;
		bgra[i * 4 + 2] = m->body[color].r;
		bgra[i * 4 + 3] = 255;
	}
	for (i = 0; i < m->nparts; i++) {
		const hf_part *p = &m->part[i];
		const hf_rgb c = paint_of(m, p, color);
		int x0, y0, x1, y1, k;
		float rad[4];

		hf_px_rect(m, &p->shape, gx, gy, gw, gh, &x0, &y0, &x1, &y1);
		/* A radius in pixels, the smaller of its two scales (a corner is
		 * round on the face), no larger than half the side */
		for (k = 0; k < 4; k++) {
			float r = p->shape.r[k] * (px < py ? px : py);

			if (r > (float)(x1 - x0) / 2.0f) r = (float)(x1 - x0) / 2.0f;
			if (r > (float)(y1 - y0) / 2.0f) r = (float)(y1 - y0) / 2.0f;
			rad[k] = r < 0.0f ? 0.0f : r;
		}
		for (y = y0 < 0 ? 0 : y0; y < y1 && y < h; y++)
			for (x = x0 < 0 ? 0 : x0; x < x1 && x < w; x++) {
				const float a = cover(x, y, x0, y0, x1, y1, rad);
				uint8_t *d = bgra + ((size_t)y * w + x) * 4;

				if (a <= 0.0f) continue;
				d[0] = (uint8_t)lroundf(c.b * a + d[0] * (1.0f - a));
				d[1] = (uint8_t)lroundf(c.g * a + d[1] * (1.0f - a));
				d[2] = (uint8_t)lroundf(c.r * a + d[2] * (1.0f - a));
			}
	}
}
