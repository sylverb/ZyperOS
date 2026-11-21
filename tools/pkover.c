/* SPDX-License-Identifier: GPL-3.0-only */
/* See pkover.h: portkeys's one picture on the cursor window, and the frames
 * a second it counts for On-Screen Stats */
#include "pkover.h"

#include <string.h>

pk_rect pkover_disc_rect(int lw)
{
	pk_rect r = { lw - PK_DISC_IN - PK_DISC_R, PK_DISC_IN - PK_DISC_R, 2 * PK_DISC_R + 1, 2 * PK_DISC_R + 1 };

	return r;
}

bool pkover_disc_on(int level, uint64_t now_ms)
{
	return level == 1 || (level == 2 && (now_ms / 1000u) % 2 == 0);
}

static void grow(pk_rect *u, int x, int y, int w, int h)
{
	int x1, y1;

	if (w <= 0 || h <= 0) return;
	if (u->w <= 0) {
		*u = (pk_rect){ x, y, w, h };
		return;
	}
	x1 = u->x + u->w > x + w ? u->x + u->w : x + w;
	y1 = u->y + u->h > y + h ? u->y + u->h : y + h;
	if (x < u->x) u->x = x;
	if (y < u->y) u->y = y;
	u->w = x1 - u->x;
	u->h = y1 - u->y;
}

pk_rect pkover_union(const pk_scene *s)
{
	pk_rect u = { 0, 0, 0, 0 };
	int i;

	for (i = 0; i < PK_LAYERS; i++)
		if (s->layer[i].px) grow(&u, s->layer[i].x, s->layer[i].y, s->layer[i].w, s->layer[i].h);
	if (s->disc) {
		pk_rect d = pkover_disc_rect(s->lw);

		grow(&u, d.x, d.y, d.w, d.h);
	}
	return u;
}

pk_window pkover_window(pk_rect u, bool turned, int ph)
{
	pk_window w;

	w.w = turned ? u.h : u.w;
	w.h = turned ? u.w : u.h;
	w.bw = (w.w + 15) / 16 * 16;
	w.bh = w.h;
	w.x = turned ? u.y : u.x;
	w.y = turned ? ph - u.x - u.w : u.y;
	return w;
}

/* src over dst, both premultiplied */
static inline uint32_t over(uint32_t src, uint32_t dst)
{
	uint32_t a = src >> 24, k = 255 - a, out = 0;
	int c;

	if (a == 255 || !dst) return src;
	if (!a && !(src & 0xFFFFFF)) return dst;
	for (c = 0; c < 32; c += 8) {
		uint32_t v = ((src >> c) & 255) + (((dst >> c) & 255) * k + 127) / 255;

		out |= (v > 255 ? 255 : v) << c;
	}
	return out;
}

void pkover_render(const pk_scene *s, pk_rect u, pk_window win, bool turned, uint32_t *out,
                   uint32_t *scratch)
{
	int i, x, y;

	if (u.w <= 0 || u.h <= 0) return;
	memset(scratch, 0, (size_t)u.w * u.h * 4);
	for (i = 0; i < PK_LAYERS; i++) {
		const pk_layer *l = &s->layer[i];

		if (!l->px) continue;
		for (y = 0; y < l->h; y++) {
			int sy = l->y + y - u.y;

			if (sy < 0 || sy >= u.h) continue;
			for (x = 0; x < l->w; x++) {
				int sx = l->x + x - u.x;
				uint32_t v = l->px[(size_t)y * l->w + x];

				if (sx < 0 || sx >= u.w || !v) continue;
				scratch[(size_t)sy * u.w + sx] = over(v, scratch[(size_t)sy * u.w + sx]);
			}
		}
	}
	/* The disc as Diatom draws it: horizontal spans, each sqrt(v) rounded,
	 * the largest dx with (2dx - 1)^2 <= 4v, so the two match to the pixel */
	if (s->disc) {
		const int cx = s->lw - PK_DISC_IN, cy = PK_DISC_IN;
		int dy;

		for (dy = -PK_DISC_R; dy <= PK_DISC_R; dy++) {
			const int v = PK_DISC_R * PK_DISC_R - dy * dy, sy = cy + dy - u.y;
			int dx = 0;

			while ((2 * (dx + 1) - 1) * (2 * (dx + 1) - 1) <= 4 * v) dx++;
			if (sy < 0 || sy >= u.h) continue;
			for (x = cx - dx; x <= cx + dx; x++)
				if (x - u.x >= 0 && x - u.x < u.w) scratch[(size_t)sy * u.w + (x - u.x)] = PK_DISC_ARGB;
		}
	}
	/* Turned onto the panel as the bar always was: landscape (x, y) of the
	 * picture is the buffer's row u.w - 1 - x, column y */
	memset(out, 0, (size_t)win.bw * win.bh * 4);
	for (y = 0; y < u.h; y++)
		for (x = 0; x < u.w; x++) {
			uint32_t v = scratch[(size_t)y * u.w + x];

			if (!v) continue;
			if (turned) out[(size_t)(u.w - 1 - x) * win.bw + y] = v;
			else        out[(size_t)y * win.bw + x] = v;
		}
}

/* ---- frames a second ------------------------------------------------------ */

void pkover_fps_init(pk_fps *f)
{
	memset(f, 0, sizeof *f);
	f->fps = -1.0;
}

bool pkover_fps_blank(pk_fps *f, uint32_t fb_id, uint64_t now_ms)
{
	double was = f->fps;

	if (!f->seen) {
		f->seen = true;
		f->last_fb = fb_id;
		f->frames = 0;
		f->since = now_ms;
		return false;
	}
	if (fb_id != f->last_fb) f->frames++;
	f->last_fb = fb_id;
	if (now_ms - f->since >= PK_FPS_EVERY_MS) {
		f->fps = f->frames * 1000.0 / (double)(now_ms - f->since);
		f->frames = 0;
		f->since = now_ms;
	}
	return f->fps != was;
}

void pkover_fps_lost(pk_fps *f)
{
	pkover_fps_init(f);
}
