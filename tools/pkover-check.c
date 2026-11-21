/* SPDX-License-Identifier: GPL-3.0-only */
/* portkeys's picture on the cursor window (tools/pkover.h), on the host:
 * the bar alone and the heat notice alone go up exactly as they did before
 * the window held more than one thing (put and notice_put's turn and place);
 * the low-battery disc is Diatom's to the pixel (diatom/port/gles_draw.h's
 * draw_low_battery: radius 9, 34 px in from the top and right, blinking
 * when critical); On-Screen Stats' line is where src/hud.h's hud_place puts
 * it, in hud_render's pixels, clear of the bar and the disc; several at
 * once share one buffer, the bar over the line and the disc over all, both
 * turned onto the Pixel 2's portrait panel and as they are on a landscape
 * one; and the frames a second, counted from the framebuffer at each
 * vertical blank as px2-shot --fps counts them. No DRM, no device.
 *
 *     make check-pkover */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/hud.h"
#include "../src/osdbar.h"
#include "pkover.h"

#define PANEL_S 0.625f
#define LW 640
#define LH 480

static int fails;
#define CHECK(cond, what) do { if (!(cond)) { printf("  FAIL %s\n", what); fails++; } } while (0)

static uint32_t *buf_out, *scratch;

/* What the window shows at landscape (x, y), read back out of its buffer */
static uint32_t at(pk_rect u, pk_window w, bool turned, int x, int y)
{
	int rx = x - u.x, ry = y - u.y;

	if (rx < 0 || ry < 0 || rx >= u.w || ry >= u.h) return 0;
	return turned ? buf_out[(size_t)(u.w - 1 - rx) * w.bw + ry] : buf_out[(size_t)ry * w.bw + rx];
}

static void solid(uint32_t *px, int w, int h, uint32_t v)
{
	int i;

	for (i = 0; i < w * h; i++) px[i] = v;
}

int main(void)
{
	static uint32_t bar[OSDBAR_W * OSDBAR_H], note[400 * 80], hudpx[600 * 80];
	static uint32_t old[64 * OSDBAR_W];
	int bw, bh, x, y, i, turn;

	buf_out = calloc((size_t)(LH + 32) * (LW + 32), 4);
	scratch = calloc((size_t)(LH + 32) * (LW + 32), 4);
	osdbar_size(PANEL_S, &bw, &bh);

	printf("the bar alone, as put() put it:\n");
	{
		pk_scene s = { .lw = LW, .lh = LH };
		int lx = (int)(OSDBAR_X * PANEL_S + 0.5f), ly = (int)((OSDBAR_Y - 6) * PANEL_S + 0.5f);
		int cw = (bh + 15) / 16 * 16;
		pk_rect u;
		pk_window w;

		osdbar_render(bar, bw, bh, bw, PANEL_S, DS_LIGHT, OSDBAR_VOLUME, 12, 20, 1.0f, true);
		s.layer[PK_LAYER_BAR] = (pk_layer){ bar, lx, ly, bw, bh };
		u = pkover_union(&s);
		w = pkover_window(u, true, LW);
		pkover_render(&s, u, w, true, buf_out, scratch);
		/* put(): dst[(bw - 1 - x) * stride + y] = flat[y * bw + x], the
		 * window at (ly, ph - lx - bw), cw x bw */
		memset(old, 0, sizeof old);
		for (y = 0; y < bh; y++)
			for (x = 0; x < bw; x++) old[(size_t)(bw - 1 - x) * cw + y] = bar[(size_t)y * bw + x];
		CHECK(u.x == lx && u.y == ly && u.w == bw && u.h == bh, "its picture is the bar's rect");
		CHECK(w.x == ly && w.y == LW - lx - bw && w.bw == cw && w.bh == bw, "the window where put() put it, cw x bw");
		CHECK(!memcmp(old, buf_out, (size_t)cw * bw * 4), "the same pixels, byte for byte");
	}

	printf("the heat notice alone, as notice_put() put it:\n");
	for (turn = 0; turn < 2; turn++) {
		pk_scene s = { .lw = turn ? LW : 800, .lh = turn ? LH : 600 };
		int nw = 400, nh = 80, lw = s.lw, lh = s.lh;
		int lx = (lw - nw) / 2, ly = lh - nh - lh / 24;
		pk_rect u;
		pk_window w;
		bool turned = turn == 1;
		char what[128];

		for (i = 0; i < nw * nh; i++) note[i] = 0x80000000u | (uint32_t)i;
		s.layer[PK_LAYER_NOTICE] = (pk_layer){ note, lx, ly, nw, nh };
		u = pkover_union(&s);
		w = pkover_window(u, turned, turned ? LW : 600);
		pkover_render(&s, u, w, turned, buf_out, scratch);
		/* notice_put(): turned, dst[(w - 1 - x) * bw + y], at (ly, ph - lx - w);
		 * landscape, dst[y * bw + x], at (lx, ly) */
		snprintf(what, sizeof what, "%s: the window at notice_put's place, its size",
		         turned ? "turned" : "landscape");
		CHECK(turned ? (w.x == ly && w.y == LW - lx - nw && w.bw == (nh + 15) / 16 * 16 && w.bh == nw)
		             : (w.x == lx && w.y == ly && w.bw == (nw + 15) / 16 * 16 && w.bh == nh), what);
		{
			bool same = true;

			for (y = 0; y < nh && same; y++)
				for (x = 0; x < nw && same; x++) {
					uint32_t want = note[(size_t)y * nw + x];
					uint32_t got = turned ? buf_out[(size_t)(nw - 1 - x) * w.bw + y] : buf_out[(size_t)y * w.bw + x];

					same = want == got;
				}
			snprintf(what, sizeof what, "%s: the same pixels", turned ? "turned" : "landscape");
			CHECK(same, what);
		}
	}

	printf("the low-battery disc, Diatom's:\n");
	{
		pk_scene s = { .lw = LW, .lh = LH, .disc = true };
		pk_rect u = pkover_union(&s), d = pkover_disc_rect(LW);
		pk_window w = pkover_window(u, true, LW);
		int n = 0;

		pkover_render(&s, u, w, true, buf_out, scratch);
		CHECK(d.x == 597 && d.y == 25 && d.w == 19 && d.h == 19, "its box: centered at (606, 34), radius 9");
		CHECK(at(u, w, true, 606, 34) == PK_DISC_ARGB && at(u, w, true, 597, 34) == PK_DISC_ARGB &&
		      at(u, w, true, 615, 34) == PK_DISC_ARGB, "the middle row 19 wide");
		CHECK(at(u, w, true, 606, 25) == PK_DISC_ARGB && !at(u, w, true, 605, 25) && !at(u, w, true, 607, 25) &&
		      at(u, w, true, 606, 43) == PK_DISC_ARGB && !at(u, w, true, 605, 43), "the top and bottom rows one pixel");
		for (y = 25; y <= 43; y++)
			for (x = 597; x <= 615; x++) n += at(u, w, true, x, y) == PK_DISC_ARGB;
		/* Diatom's spans: 1 + 2 * sum over dy of (2 dx + 1), dx the rounded
		 * square root, for radius 9: 1 7 9 11 13 15 17 17 19 19 ... = 285 */
		{
			int want = 0, dy;

			for (dy = -9; dy <= 9; dy++) {
				int v = 81 - dy * dy, dx = 0;

				while ((2 * dx + 1) * (2 * dx + 1) <= 4 * v) dx++;
				want += 2 * dx + 1;
			}
			CHECK(n == want, "every span as Diatom's");
		}
		CHECK(pkover_disc_on(1, 1000) && pkover_disc_on(1, 2500), "low: always");
		CHECK(pkover_disc_on(2, 0) && !pkover_disc_on(2, 1000) && pkover_disc_on(2, 2999) &&
		      !pkover_disc_on(2, 3001), "critical: the even seconds, blinking");
		CHECK(!pkover_disc_on(0, 0) && !pkover_disc_on(0, 1000), "fine or charging: never");
	}

	printf("On-Screen Stats' line, where hud_place puts it:\n");
	for (i = 0; i < HUD_CORNERS; i++) {
		hud_style st = hud_default;
		hud_values v = { 59.7, -1, 52, 34, 1300, 87, 10, 8, true };
		char text[160], what[128];
		int hw, hh, hx, hy;
		pk_scene s = { .lw = LW, .lh = LH, .disc = true };
		pk_rect u, d = pkover_disc_rect(LW), r;
		pk_window w;
		bool same = true;

		st.corner = (uint8_t)i;
		st.items = (1u << HUD_FPS) | (1u << HUD_TEMP) | (1u << HUD_BATTERY) | (1u << HUD_TIME);
		hud_text(text, sizeof text, &v, st.items);
		hud_size(text, PANEL_S, &st, &hw, &hh);
		hud_place(&st, PANEL_S, hw, hh, LW, LH, &hx, &hy);
		hud_render(hudpx, hw, hh, hw, PANEL_S, text, &st, NULL, true);
		s.layer[PK_LAYER_HUD] = (pk_layer){ hudpx, hx, hy, hw, hh };
		u = pkover_union(&s);
		w = pkover_window(u, true, LW);
		pkover_render(&s, u, w, true, buf_out, scratch);
		for (y = 0; y < hh && same; y++)
			for (x = 0; x < hw && same; x++)
				same = at(u, w, true, hx + x, hy + y) == hudpx[(size_t)y * hw + x];
		snprintf(what, sizeof what, "%s: hud_render's pixels at hud_place's (%d, %d)", hud_corner_id[i], hx, hy);
		CHECK(same && strstr(text, "59.7 fps"), what);
		r = (pk_rect){ hx, hy, hw, hh };
		snprintf(what, sizeof what, "%s: clear of the disc", hud_corner_id[i]);
		CHECK(r.x + r.w <= d.x || d.x + d.w <= r.x || r.y + r.h <= d.y || d.y + d.h <= r.y, what);
		snprintf(what, sizeof what, "%s: the disc and the line in one buffer, the disc still whole",
		         hud_corner_id[i]);
		CHECK(at(u, w, true, 606, 34) == PK_DISC_ARGB && at(u, w, true, 606, 25) == PK_DISC_ARGB, what);
	}
	{
		hud_values v = { 59.7, -1, 52, -1, -1, -1, -1, -1, true };
		char text[160];

		hud_text(text, sizeof text, &v, (1u << HUD_FPS) | (1u << HUD_SPEED));
		CHECK(!strcmp(text, "59.7 fps"), "no Speed over a program: the item left out, FPS alone");
		v.fps = -1;
		hud_text(text, sizeof text, &v, 1u << HUD_FPS);
		CHECK(!text[0], "no frames counted yet: no line");
	}

	printf("all at once:\n");
	for (turn = 0; turn < 2; turn++) {
		pk_scene s = { .lw = LW, .lh = LH, .disc = true };
		int lx = (int)(OSDBAR_X * PANEL_S + 0.5f), ly = (int)(OSDBAR_Y * PANEL_S + 0.5f);
		pk_rect u;
		pk_window w;
		bool turned = turn == 1;
		char what[128];

		solid(hudpx, 100, 20, 0xFF102030u);
		solid(bar, bw, bh, 0xFF405060u);
		solid(note, 300, 50, 0x80404040u);
		s.layer[PK_LAYER_HUD] = (pk_layer){ hudpx, lx + 10, ly + 10, 100, 20 };   /* under the bar */
		s.layer[PK_LAYER_BAR] = (pk_layer){ bar, lx, ly, bw, bh };
		s.layer[PK_LAYER_NOTICE] = (pk_layer){ note, 170, 400, 300, 50 };
		u = pkover_union(&s);
		w = pkover_window(u, turned, LW);
		pkover_render(&s, u, w, turned, buf_out, scratch);
		snprintf(what, sizeof what, "%s: one picture from the bar's top to the notice's bottom, out to the disc",
		         turned ? "turned" : "landscape");
		CHECK(u.x == lx && u.y == ly && u.x + u.w == 616 && u.y + u.h == 450, what);
		snprintf(what, sizeof what, "%s: the bar over the line, the disc over all, the notice its own",
		         turned ? "turned" : "landscape");
		CHECK(at(u, w, turned, lx + 20, ly + 15) == 0xFF405060u && at(u, w, turned, 606, 34) == PK_DISC_ARGB &&
		      at(u, w, turned, 200, 420) == 0x80404040u && !at(u, w, turned, 100, 300), what);
		snprintf(what, sizeof what, "%s: the buffer's width padded to 16", turned ? "turned" : "landscape");
		CHECK(w.bw % 16 == 0 && w.bw >= w.w && w.w == (turned ? u.h : u.w), what);
	}
	{
		pk_scene s = { .lw = LW, .lh = LH };

		CHECK(pkover_union(&s).w == 0, "nothing up: no picture");
	}

	printf("frames a second, from the framebuffer at each blank:\n");
	{
		pk_fps f;
		uint64_t t = 1000;
		int k;

		pkover_fps_init(&f);
		CHECK(f.fps < 0, "none before the first half second");
		for (k = 0; k <= 60; k++) pkover_fps_blank(&f, 100u + (unsigned)k, t + (uint64_t)k * 1000 / 60);
		CHECK(f.fps > 59.0 && f.fps <= 60.5, "a new frame at every blank: 60");
		for (k = 61; k <= 120; k++) pkover_fps_blank(&f, 100u + (unsigned)(k / 2), t + (uint64_t)k * 1000 / 60);
		CHECK(f.fps > 29.0 && f.fps < 31.0, "a new frame every second blank: 30");
		for (k = 121; k <= 180; k++) pkover_fps_blank(&f, 7, t + (uint64_t)k * 1000 / 60);
		CHECK(f.fps == 0.0, "the same frame at every blank: 0");
		pkover_fps_lost(&f);
		CHECK(f.fps < 0, "blanks lost: not known again");
	}

	free(buf_out);
	free(scratch);
	if (fails) { printf("pkover: %d FAILED\n", fails); return 1; }
	printf("pkover: ok\n");
	return 0;
}
