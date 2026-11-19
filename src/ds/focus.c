/* SPDX-License-Identifier: GPL-3.0-only */
/* The focus on the home and the shelves (src/ds/focus.h). */
#include "ds/focus.h"

#include <math.h>
#include <string.h>

#include "design.h"
#include "ds/layout.h"
#include "device/profile.h"
#include "platform.h"
#include "focusgeo.h"
#include "look.h"
#include "theme.h"
#include "ui.h"

/* The ring and its gap (Highlight's Border, fine), and Bar's bar, in units:
 * each drawn as the whole panel pixels they come to (src/focusgeo.h) */
#define RING_GAP    DS_FOCUS_GAP
#define RING_LINE   DS_FOCUS_RING
#define BAR_W       DS_FOCUS_BAR_W
#define BAR_H       DS_FOCUS_BAR_H
#define BAR_UNDER   DS_FOCUS_BAR_UNDER

ds_focus_marks ds_focus_marks_of(int kind)
{
	return ds_focus_marks_for(kind, look_focus());
}

ds_focus_marks ds_focus_marks_for(int kind, int f)
{
	const bool still = kind == DS_FOCUS_STILL;
	ds_focus_marks m = { f, 0, false, false, DS_FOCUS_LIFT_NONE, false };

	/* My picks in the focus lab: the cover grows less than a still, since a
	 * cover is larger and already a thing of its own; a still's lift is its
	 * own shape's (the home), a cover's its rect's */
	switch (f) {
	case LOOK_FOCUS_BORDER: m.grow = .03f; m.ring = true; m.lift = DS_FOCUS_LIFT_SOFT; break;
	case LOOK_FOCUS_LIFT:   m.grow = still ? .12f : .08f; m.lift = DS_FOCUS_LIFT_HIGH; m.sheen = !still; break;
	case LOOK_FOCUS_NONE:   m.grow = still ? .08f : .06f; break;
	default:                m.grow = still ? .06f : .04f; m.bar = true; m.lift = DS_FOCUS_LIFT_SOFT; break;
	}
	return m;
}

float ds_focus_weight(const ds_focus_weights *fw, int i, int cursor, Uint32 since)
{
	float from = 0, k = (float)since / DS_MS_QUICK;

	/* no move since the grid was placed: at rest, whatever the clock says */
	if (fw->n == 0 || k >= 1) return i == cursor ? 1 : 0;
	for (int j = 0; j < fw->n; j++) if (fw->i[j] == i) from = fw->w[j];
	return from + ((i == cursor ? 1.0f : 0.0f) - from) * ds_out(k < 0 ? 0 : k);
}

void ds_focus_weights_move(ds_focus_weights *fw, int cursor, Uint32 since)
{
	ds_focus_weights next = { { 0 }, 0, { 0 } };

	for (int j = 0; j < fw->n; j++) {
		float w = ds_focus_weight(fw, fw->i[j], cursor, since);

		if (fw->i[j] != cursor && w > .005f && next.n < 3) { next.i[next.n] = fw->i[j]; next.w[next.n++] = w; }
	}
	next.i[next.n] = cursor;
	next.w[next.n++] = ds_focus_weight(fw, cursor, cursor, since);
	*fw = next;
}

float ds_focus_k(void)
{
	const device_profile *p = plat_profile();
	int s;

	if (!p) return 1;
	s = p->screen.panel_w < p->screen.panel_h ? p->screen.panel_w : p->screen.panel_h;
	return s > 0 && s < ZYPEROS_SCREEN_H ? (float)s / ZYPEROS_SCREEN_H : 1;
}

SDL_FRect ds_focus_tile(SDL_FRect cell, const ds_focus_marks *m, float w)
{
	const float k = ds_focus_k();
	int side[2], foot[2], ns = 0, nf = 0;
	fg_px_rect p;
	fg_rect u;

	if (w <= 0) return cell;
	if (m->ring) {
		side[0] = fg_px_len(RING_GAP, k);
		side[1] = side[0] + fg_px_len(RING_LINE, k);
		ns = 2;
	}
	if (m->bar) {
		foot[0] = fg_px(BAR_UNDER, k);
		foot[1] = foot[0] + fg_px_len(BAR_H, k);
		nf = 2;
	}
	p = fg_tile((fg_rect){ cell.x, cell.y, cell.w, cell.h }, m->grow, k, side, ns, foot, nf);
	u = fg_units(p, k);
	if (w >= 1) return (SDL_FRect){ u.x, u.y, u.w, u.h };
	return (SDL_FRect){ cell.x + (u.x - cell.x) * w, cell.y + (u.y - cell.y) * w,
	                    cell.w + (u.w - cell.w) * w, cell.h + (u.h - cell.h) * w };
}

/* A rect's edges on the panel's pixels, then back in units, each drawn
 * where it lands cleanest: `grow` pixels out from the rect (in if under 0) */
static SDL_Rect px_out(SDL_FRect f, int grow, float k)
{
	int l = fg_px(f.x, k) - grow, t = fg_px(f.y, k) - grow;
	int r = fg_px(f.x + f.w, k) + grow, b = fg_px(f.y + f.h, k) + grow;
	int x = fg_unit(l, k), y = fg_unit(t, k);

	return (SDL_Rect){ x, y, fg_unit(r, k) - x, fg_unit(b, k) - y };
}

/* A rounded rect's inset at row `y` of `h` rows, its corners `rad`: the
 * same arithmetic as ui_round_rect's (round_rect_rects), so a ring and the
 * tile it goes round agree */
static int inset_at(int y, int h, int rad)
{
	int dy;

	if (rad <= 0) return 0;
	if (y >= rad && y < h - rad) return 0;
	dy = y < rad ? rad - y : y - (h - 1 - rad);
	if (dy > rad) dy = rad;
	return rad - (int)(sqrt((double)(rad * rad - dy * dy)) + 0.5);
}

#define RING_RECTS 96

void ds_round_ring(SDL_Renderer *r, SDL_Rect o, SDL_Rect in, int ro, int ri, SDL_Color c)
{
	SDL_Rect buf[RING_RECTS];
	int n = 0, run0 = 0, last[4] = { 0, 0, 0, 0 }, have = 0;

	if (o.w <= 0 || o.h <= 0) return;
	if (ro * 2 > o.w) ro = o.w / 2;
	if (ro * 2 > o.h) ro = o.h / 2;
	if (ri * 2 > in.w) ri = in.w / 2;
	if (ri * 2 > in.h) ri = in.h / 2;
	if (look_square()) ro = ri = 0;
	SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
	/* Row by row: the outer span less the hole's; rows that come out the
	 * same are one rect, so the straight sides are a rect each */
	for (int y = 0; y <= o.h; y++) {
		int span[4] = { 0, 0, 0, 0 };

		if (y < o.h) {
			int ox0 = o.x + inset_at(y, o.h, ro), ox1 = o.x + o.w - inset_at(y, o.h, ro);
			int iy = o.y + y - in.y;

			if (iy >= 0 && iy < in.h && in.w > 0) {
				int ix0 = in.x + inset_at(iy, in.h, ri), ix1 = in.x + in.w - inset_at(iy, in.h, ri);

				span[0] = ox0; span[1] = ix0 > ox0 ? ix0 : ox0;
				span[2] = ix1 < ox1 ? ix1 : ox1; span[3] = ox1;
			} else {
				span[0] = ox0; span[1] = ox1; span[2] = ox1; span[3] = ox1;
			}
		}
		if (have && y < o.h && !memcmp(span, last, sizeof span)) continue;
		if (have) {
			int hgt = y - run0;

			if (last[1] > last[0] && n < RING_RECTS) buf[n++] = (SDL_Rect){ last[0], o.y + run0, last[1] - last[0], hgt };
			if (last[3] > last[2] && n < RING_RECTS) buf[n++] = (SDL_Rect){ last[2], o.y + run0, last[3] - last[2], hgt };
			if (n >= RING_RECTS - 2) { SDL_RenderFillRects(r, buf, n); n = 0; }
		}
		memcpy(last, span, sizeof span);
		run0 = y;
		have = 1;
	}
	if (n) SDL_RenderFillRects(r, buf, n);
}

void ds_focus_ring(SDL_Renderer *r, SDL_FRect tile, int radius, Uint8 alpha)
{
	const float k = ds_focus_k();
	const int gap = fg_px_len(RING_GAP, k), line = fg_px_len(RING_LINE, k);
	const int rpx = (int)lroundf(radius * k);
	SDL_Rect outer = px_out(tile, gap + line, k), hole = px_out(tile, gap, k);
	int ro = radius > 0 ? (int)lroundf((rpx + gap + line) / k) : 0;
	int ri = radius > 0 ? (int)lroundf((rpx + gap) / k) : 0;

	ds_round_ring(r, outer, hole, ro, ri, ui_col(theme()->accent_ink, alpha));
}

void ds_focus_ring_in(SDL_Renderer *r, SDL_FRect row, int radius, Uint8 alpha)
{
	const float k = ds_focus_k();
	const int line = fg_px_len(RING_LINE, k);
	SDL_Rect outer = px_out(row, 0, k), hole = px_out(row, -line, k);

	ds_round_ring(r, outer, hole, radius, radius - (hole.x - outer.x), ui_col(theme()->accent_ink, alpha));
}

void ds_focus_bar(SDL_Renderer *r, SDL_FRect above, Uint8 alpha)
{
	const float k = ds_focus_k();
	const int bw = fg_px_len(BAR_W, k), bh = fg_px_len(BAR_H, k), off[1] = { bw };
	const int top = fg_px(above.y + above.h, k) + fg_px(BAR_UNDER, k);
	const int left = fg_edge((int)lroundf((above.x + above.w / 2) * k - bw / 2.0f), 1, off, 1, k);
	const int x = fg_unit(left, k), y = fg_unit(top, k);
	SDL_Rect q = { x, y, fg_unit(left + bw, k) - x, fg_unit(top + bh, k) - y };

	/* a pill stays a pill, Square or not */
	ui_round_rect(r, &q, q.h / 2, ui_col(theme()->accent_ink, alpha));
}

void ds_focus_hairline(SDL_Renderer *r, SDL_FRect tile, int radius)
{
	const float k = ds_focus_k();
	SDL_Rect o = px_out(tile, 0, k), in = px_out(tile, -1, k);
	const bool dark = theme_index() == DS_DARK;
	const SDL_Color c = dark ? (SDL_Color){ 255, 255, 255, 26 } : ui_col(theme()->ink, 41);

	ds_round_ring(r, o, in, radius, radius - (in.x - o.x), c);
}

void ds_focus_dim(SDL_Renderer *r, SDL_FRect tile, int radius, float w)
{
	const float a = fg_dim_cover(w);
	SDL_Rect q = { (int)lroundf(tile.x), (int)lroundf(tile.y), (int)lroundf(tile.w), (int)lroundf(tile.h) };

	if (a <= 0.004f) return;
	ui_round_rect(r, &q, radius, ui_col(theme()->ground, (Uint8)lroundf(255 * a)));
}

/* As the cover row's (src/games.c): a band leaning as a name does not, white
 * at its middle and clear at its edges, clipped to the cover's sides and
 * faded in and out a corner from its top and foot */
void ds_focus_sheen(SDL_Renderer *r, SDL_FRect c, float t)
{
	const float lean = .45f, half = c.w * .27f, rad = DS_RADIUS_CARD;
	const float cx = c.x - .35f * c.w + t * 1.7f * c.w, mid = c.y + c.h * .5f;
	const float ys[4] = { c.y + rad, c.y + 3 * rad, c.y + c.h - 3 * rad, c.y + c.h - rad }, on[4] = { 0, 1, 1, 0 };
	const Uint8 peak = (Uint8)lroundf(255 * .24f * sinf(3.14159265f * (t < 0 ? 0 : t > 1 ? 1 : t)));
	SDL_Vertex v[12];
	int idx[36], n = 0;
	SDL_Rect clip, was;
	SDL_bool clipped;

	if (t <= 0 || t >= 1 || c.h < 8 * rad) return;
	for (int row = 0; row < 4; row++) {
		float sx = (ys[row] - mid) * lean;

		for (int col = 0; col < 3; col++) {
			Uint8 al = col == 1 ? (Uint8)lroundf(peak * on[row]) : 0;

			v[row * 3 + col] = (SDL_Vertex){ { cx + (col - 1) * half + sx, ys[row] }, { 255, 255, 255, al }, { 0, 0 } };
		}
	}
	for (int row = 0; row < 3; row++)
		for (int col = 0; col < 2; col++) {
			int a = row * 3 + col, b = a + 1, c2 = a + 3, d = a + 4;

			idx[n++] = a; idx[n++] = b; idx[n++] = d; idx[n++] = a; idx[n++] = d; idx[n++] = c2;
		}
	clipped = SDL_RenderIsClipEnabled(r);
	SDL_RenderGetClipRect(r, &was);
	clip = (SDL_Rect){ (int)ceilf(c.x), (int)ceilf(c.y), (int)floorf(c.w), (int)floorf(c.h) };
	if (clipped && !SDL_IntersectRect(&clip, &was, &clip)) return;
	SDL_RenderSetClipRect(r, &clip);
	SDL_RenderGeometry(r, NULL, v, 12, idx, n);
	SDL_RenderSetClipRect(r, clipped ? &was : NULL);
}
