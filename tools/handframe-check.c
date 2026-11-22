/* SPDX-License-Identifier: GPL-3.0-only */
/* The handheld frames (src/handframe.h): each placement is where Sharp
 * Pixels puts the game (Handheld) or the largest step smaller where the
 * whole lens and some body fit, the lens in the middle (Whole, never under
 * 2x): 2x on the Pixel 2, 3x on the Brick; one game pixel is one LCD dot; straight
 * edges are whole pixels, each side of an edge exactly one color; an arc
 * stands where the model says; every part is inside the one it sits in; the
 * Game Boy Color's six colors are named. On the Pixel 2's 640x480 and the
 * Brick's 1024x768. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/dmodefit.h"
#include "../src/handframe.h"

static int checks, fails;

static void ck(int ok, const char *what)
{
	checks++;
	if (!ok) { fails++; printf("  FAIL %s\n", what); }
}

static const uint8_t *at(const uint8_t *px, int w, int x, int y) { return px + ((size_t)y * w + x) * 4; }
static int is(const uint8_t *p, hf_rgb c) { return p[2] == c.r && p[1] == c.g && p[0] == c.b; }

int main(void)
{
	static const char *const TAGS[] = { "GB", "GBC", "GBA" };
	static const int SIZES[2][2] = { { 640, 480 }, { 1024, 768 } };
	char what[160];
	int t, s, i;

	for (t = 0; t < 3; t++) {
		const hf_model *m = hf_model_for(TAGS[t]);

		ck(m != NULL, "a frame for each of the three Game Boys");
		if (!m) continue;
		/* every part in the body, the window in the lens */
		for (i = 1; i < m->nparts; i++) {
			const hf_rrect *a = &m->part[i].shape, *b = &m->part[0].shape;

			snprintf(what, sizeof what, "%s: part %d inside the body", TAGS[t], i);
			ck(a->x0 >= b->x0 && a->x1 <= b->x1 && a->y0 >= b->y0 && a->y1 <= b->y1, what);
		}
		snprintf(what, sizeof what, "%s: the LCD inside the lens", TAGS[t]);
		ck(m->lens.x0 < 0 && m->lens.y0 < 0 && m->lens.x1 > m->lcd_w && m->lens.y1 > m->lcd_h, what);
		for (s = 0; s < 2; s++) {
			const int w = SIZES[s][0], h = SIZES[s][1];
			int sc, gx, gy, gw, gh, wsc, wx, wy, ww, wh;
			const dmode_rect sp = dmode_fit("integer", m->cols, m->lines, 0, w, h);
			uint8_t *px = malloc((size_t)w * h * 4);

			if (!px) return 1;
			snprintf(what, sizeof what, "%s on %dx%d: Handheld is where Sharp Pixels puts the game", TAGS[t], w, h);
			ck(hf_place(m, HF_CUT, w, h, &sc, &gx, &gy, &gw, &gh) && gx == sp.x && gy == sp.y && gw == sp.w &&
			   gh == sp.h, what);
			snprintf(what, sizeof what, "%s on %dx%d: a game pixel a dot (%.2f px/mm across)", TAGS[t], w, h,
			         gw / m->lcd_w);
			ck(fabsf((float)gw / m->lcd_w - (float)sc * m->cols / m->lcd_w) < 0.001f, what);
			if (hf_place(m, HF_WHOLE, w, h, &wsc, &wx, &wy, &ww, &wh)) {
				const float cx = wx + (m->lens.x0 + m->lens.x1) / 2 * ww / m->lcd_w;
				const float cy = wy + (m->lens.y0 + m->lens.y1) / 2 * wh / m->lcd_h;

				const float pw = (float)ww / m->lcd_w, ph = (float)wh / m->lcd_h;
				int lx0, ly0, lx1, ly1;

				snprintf(what, sizeof what, "%s on %dx%d: Whole at %dx (%s), 2x or more, the lens in the middle",
				         TAGS[t], w, h, wsc, w == 640 ? "2x on the Pixel 2" : "3x on the Brick");
				ck(wsc < sc && wsc >= 2 && wsc == (w == 640 ? 2 : 3) && fabsf(cx - w / 2.0f) <= 1.0f &&
				   fabsf(cy - h / 2.0f) <= 1.0f, what);
				/* the whole lens on the panel, with body round it */
				hf_px_rect(m, &m->lens, wx, wy, ww, wh, &lx0, &ly0, &lx1, &ly1);
				snprintf(what, sizeof what, "%s on %dx%d: Whole shows the whole lens and %.0f mm of body round it",
				         TAGS[t], w, h, HF_WHOLE_BODY_MM);
				ck(lx0 >= (int)(HF_WHOLE_BODY_MM * pw) && ly0 >= (int)(HF_WHOLE_BODY_MM * ph) &&
				   w - lx1 >= (int)(HF_WHOLE_BODY_MM * pw) && h - ly1 >= (int)(HF_WHOLE_BODY_MM * ph), what);
				/* and a step larger would not have */
				if (wsc + 1 < sc) {
					const float pw1 = (float)(m->cols * (wsc + 1)) / m->lcd_w;
					const float ph1 = (float)(m->lines * (wsc + 1)) / m->lcd_h;

					snprintf(what, sizeof what, "%s on %dx%d: a step larger would cut the lens", TAGS[t], w, h);
					ck((m->lens.x1 - m->lens.x0 + 2 * HF_WHOLE_BODY_MM) * pw1 > (float)w ||
					   (m->lens.y1 - m->lens.y0 + 2 * HF_WHOLE_BODY_MM) * ph1 > (float)h, what);
				}
			} else {
				snprintf(what, sizeof what, "%s on %dx%d: no Whole only for the Advance on the Pixel 2", TAGS[t], w, h);
				ck(t == 2 && w == 640, what);
			}
			/* Crisp edges: the lens's left side, at its middle, one
			 * pixel the body's color and the next the lens's */
			hf_render(m, 0, w, h, gx, gy, gw, gh, px);
			{
				int x0, y0, x1, y1, ym;
				const hf_part *lens = NULL;

				for (i = 0; i < m->nparts; i++)
					if (!memcmp(&m->part[i].shape, &m->lens, sizeof m->lens)) lens = &m->part[i];
				hf_px_rect(m, &m->lens, gx, gy, gw, gh, &x0, &y0, &x1, &y1);
				ym = (gy + gh / 2);
				if (lens && x0 > 0) {
					snprintf(what, sizeof what, "%s on %dx%d: the lens's left edge a whole pixel", TAGS[t], w, h);
					ck(is(at(px, w, x0, ym), lens->color) && !is(at(px, w, x0 - 1, ym), lens->color), what);
				}
				/* An arc: the Color's lens's bottom stands sag_bot lower at
				 * its middle than near its ends */
				if (m->lens.sag_bot > 0 && y1 < h) {
					const int xm = (x0 + x1) / 2, xe = x0 + (x1 - x0) / 8;
					int ym2 = y1 - 1, ye = y1 - 1;

					while (ye > y0 && !is(at(px, w, xe, ye), lens->color)) ye--;
					snprintf(what, sizeof what, "%s on %dx%d: the lens's bottom arc stands at its middle", TAGS[t], w, h);
					ck(is(at(px, w, xm, ym2), lens->color) && !is(at(px, w, xm, y1), lens->color) &&
					   ye < ym2 - 2, what);
				}
			}
			free(px);
		}
	}
	{
		const hf_model *c = hf_model_for("GBC");
		static const char *const NAMES[] = { "yellow", "red", "purple", "green", "teal", "clear_purple" };

		ck(c && c->ncolors == 6, "the Game Boy Color in six colors");
		for (i = 0; c && i < 6; i++) ck(!strcmp(c->color_name[i], NAMES[i]), "each by its plain word, in order");
		ck(hf_model_for("GB")->ncolors == 1 && hf_model_for("GBA")->ncolors == 1, "the others in one");
	}
	ck(!hf_model_for("GG") && !hf_model_for(NULL), "no frame drawn yet for the other handhelds");
	if (fails) {
		printf("handframe: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("handframe: %d checks passed\n", checks);
	return 0;
}
