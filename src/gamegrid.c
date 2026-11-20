/* SPDX-License-Identifier: GPL-3.0-only */
/* A shelf as a grid of covers: see gamegrid.h, DESIGN.md 11.47 */
#include "gamegrid.h"

#include "artbadge_draw.h"

#include "cardart.h"
#include "cardgen.h"
#include "config.h"
#include "design.h"
#include "home.h"
#include "keyhints_draw.h"
#include "look.h"
#include "i18n.h"
#include "platform.h"
#include "theme.h"
#include "ui.h"
#include "ds/layout.h"
#include "ds/mark.h"
#include "ds/focus.h"
#include "zyperdraw.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The page around the grid, on the canvas: the margins the clock and the
 * keys keep; the room from the top bar's band and a section (72 + 24) to
 * where the band under it must start to keep DS_FOOT_GAP (24) over the
 * keys; the band, a band gap under the room: the name in game-title type in
 * a 40-unit line box, 8 (related), the facts in support type in a 36-unit
 * line box; the keys on the one foot (DS_FOOT_Y). Design system
 * round 3's spacing (2026-11-15), its rule engine's to take over. */
#define GG_MARGIN     64
#define GG_ROOM_TOP   96
#define GG_FOOT(sh)   ((sh) - (ZYPEROS_SCREEN_H - DS_FOOT_Y))   /* 720 */
#define GG_NAME_BOX   40
#define GG_RELATED    8
#define GG_FACTS_BOX  36
#define GG_BAND_TOP(sh) (((GG_FOOT(sh) - 16 - 24 - GG_NAME_BOX - GG_RELATED - GG_FACTS_BOX) / 8) * 8)  /* 592 */
#define GG_ROOM_FOOT(sh) (GG_BAND_TOP(sh) - DS_GRID_BAND_GAP)                                         /* 568 */
#define GG_LEAST      6       /* covers a shelf shows at the least */

gg_geom gg_geom_for(int sw, int sh, float shape)
{
	return gg_layout(GG_MARGIN, GG_ROOM_TOP, sw - 2 * GG_MARGIN,
	                 GG_ROOM_FOOT(sh) - GG_ROOM_TOP, shape, DS_GRID_GAP, GG_LEAST);
}

/* ---- what is known of a cover ------------------------------------------- */

static Uint32 px_at(const SDL_Surface *s, int x, int y)
{
	return *(const Uint32 *)((const Uint8 *)s->pixels + y * s->pitch + x * 4);
}

/* One pass of a box `rad` each side along a line of channel `ch`, the ends
 * held: what a box reaches past them is the end's own color, so the blur
 * keeps its light to the edge of the cell */
/* An art badge on a cell (MP4's All, src/artbadge.h): at 0.8 of its size on
 * the row's focused card, a disc of 32 units on a cell of about 220 */
#define GG_BADGE_K 0.8f

static void box_pass(Uint32 *p, int w, int h, int ch, int rad, bool along_x, int *line)
{
	int n = along_x ? w : h, m = along_x ? h : w, shift = ch * 8;

	for (int j = 0; j < m; j++) {
		int sum = 0;

		for (int i = 0; i < n; i++)
			line[i] = (int)((p[along_x ? j * w + i : i * w + j] >> shift) & 255);
		for (int k = -rad; k <= rad; k++) sum += line[k < 0 ? 0 : k >= n ? n - 1 : k];
		for (int i = 0; i < n; i++) {
			Uint32 *q = &p[along_x ? j * w + i : i * w + j];
			int add = i + rad + 1, drop = i - rad;

			*q = (*q & ~(255u << shift)) | ((Uint32)(sum / (2 * rad + 1)) << shift);
			sum += line[add >= n ? n - 1 : add] - line[drop < 0 ? 0 : drop];
		}
	}
}

/* A picture's colors blurred small: sampled to at most 48 across, then three
 * passes of a box each way. Drawn stretched behind art of another shape, its
 * middle at the cell's shape. */
static SDL_Surface *blur_small(const SDL_Surface *s)
{
	enum { MOST = 48 };
	int w, h, x, y;
	SDL_Surface *b;
	int line[MOST * 4];

	if (!s || s->w <= 0 || s->h <= 0 || s->format->format != SDL_PIXELFORMAT_ARGB8888) return NULL;
	w = s->w >= s->h ? MOST : (int)lroundf((float)MOST * s->w / s->h);
	h = s->h >= s->w ? MOST : (int)lroundf((float)MOST * s->h / s->w);
	if (w < 4) w = 4;
	if (h < 4) h = 4;
	b = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
	if (!b) return NULL;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			/* four samples a texel, so a thin line of the art is not lost */
			unsigned r = 0, g = 0, bl = 0;

			for (int k = 0; k < 4; k++) {
				int sx = (int)((x + .25f + .5f * (k & 1)) * s->w / w);
				int sy = (int)((y + .25f + .5f * (k >> 1)) * s->h / h);
				Uint32 c = px_at(s, sx < s->w ? sx : s->w - 1, sy < s->h ? sy : s->h - 1);

				r += (c >> 16) & 255; g += (c >> 8) & 255; bl += c & 255;
			}
			((Uint32 *)((Uint8 *)b->pixels + y * b->pitch))[x] =
				0xff000000u | ((r / 4) << 16) | ((g / 4) << 8) | (bl / 4);
		}
	if (b->pitch != w * 4) return b;  /* never with this format; unblurred is still a fill */
	for (int pass = 0; pass < 3; pass++)
		for (int ch = 0; ch < 3; ch++) {
			box_pass(b->pixels, w, h, ch, 3, true, line);
			box_pass(b->pixels, w, h, ch, 3, false, line);
		}
	return b;
}

static bool arts_fit(gg_arts *t, int count)
{
	if (t->a && t->n == count) return true;
	gg_arts_free(t);
	if (count <= 0 || !(t->a = calloc((size_t)count, sizeof *t->a))) return false;
	t->n = count;
	return true;
}

void gg_note(gg_arts *t, SDL_Renderer *r, int count, int i, SDL_Texture *of,
             unsigned key, SDL_Surface *s, bool generated)
{
	gg_art *e;

	if (!of || !arts_fit(t, count) || i < 0 || i >= t->n) return;
	e = &t->a[i];
	if (e->blur) SDL_DestroyTexture(e->blur);
	if (e->card) SDL_DestroyTexture(e->card);
	*e = (gg_art){ .of = of, .key = key, .generated = generated };
	if (!s) return;
	if (generated) {
		/* A game's card's ground, without its name (src/ui.c's
		 * ui_card_ground_take), for the cell to set the name on at its
		 * own size */
		if ((e->blur = SDL_CreateTextureFromSurface(r, s))) {
			SDL_SetTextureScaleMode(e->blur, SDL_ScaleModeLinear);
			SDL_SetTextureBlendMode(e->blur, SDL_BLENDMODE_NONE);
		}
		return;
	}
	e->cutout = gg_cutout(s);
	if (!e->cutout) {
		SDL_Surface *b = blur_small(s);

		if (b) {
			e->blur = SDL_CreateTextureFromSurface(r, b);
			if (e->blur) {
				SDL_SetTextureScaleMode(e->blur, SDL_ScaleModeLinear);
				SDL_SetTextureBlendMode(e->blur, SDL_BLENDMODE_BLEND);
			}
			SDL_FreeSurface(b);
		}
	}
}

gg_art *gg_art_of(const gg_arts *t, int i, const SDL_Texture *of, unsigned key)
{
	if (!t->a || i < 0 || i >= t->n || !of) return NULL;
	return t->a[i].of == of && t->a[i].key == key ? &t->a[i] : NULL;
}

void gg_drop(gg_arts *t, int i)
{
	if (!t->a || i < 0 || i >= t->n) return;
	if (t->a[i].blur) SDL_DestroyTexture(t->a[i].blur);
	if (t->a[i].card) SDL_DestroyTexture(t->a[i].card);
	t->a[i] = (gg_art){ 0 };
}

void gg_arts_free(gg_arts *t)
{
	for (int i = 0; t->a && i < t->n; i++) {
		if (t->a[i].blur) SDL_DestroyTexture(t->a[i].blur);
		if (t->a[i].card) SDL_DestroyTexture(t->a[i].card);
	}
	free(t->a);
	*t = (gg_arts){ 0 };
}

/* ---- motion -------------------------------------------------------------- */

static gg_geom geom_of(const gg_motion *m)
{
	return gg_geom_for(ZYPEROS_SCREEN_W, ZYPEROS_SCREEN_H, m->shape);
}

int gg_cols(const gg_motion *m)
{
	return geom_of(m).cols;
}

static int window_top(int cursor, int count, int cols, int top)
{
	int row = cursor / cols, last = (count + cols - 1) / cols - GG_ROWS;

	if (last < 0) last = 0;
	if (top < 0) top = row - GG_ROWS / 2;
	if (row < top) top = row;
	if (row > top + GG_ROWS - 1) top = row - GG_ROWS + 1;
	if (top > last) top = last;
	if (top < 0) top = 0;
	return top;
}

/* Placed again on the cursor with no motion, the shape left as it is */
static void place(gg_motion *m, int cursor, int count)
{
	int cols = gg_cols(m), top = window_top(cursor, count, cols, -1);

	m->set = true;
	m->cursor = cursor;
	m->count = count;
	m->cols = cols;
	m->top_from = m->top_to = (float)top;
	m->fx_from = (float)(cursor % cols);
	m->fy_from = (float)(cursor / cols);
	m->fw.n = 0;
}

void gg_reset(gg_motion *m, int cursor, int count)
{
	*m = (gg_motion){ 0 };
	place(m, cursor, count);
}

bool gg_moving(const gg_motion *m, uint32_t now)
{
	return m->set && (now - m->moved < DS_MS_MOVE || now - m->ring_moved < DS_MS_QUICK) &&
	       (m->top_from != m->top_to || m->fx_from != (float)(m->cursor % (m->cols ? m->cols : 1)) ||
	        m->fy_from != (float)(m->cursor / (m->cols ? m->cols : 1)));
}

static float eased(float from, float to, uint32_t since, int ms)
{
	float k = (float)since / ms;

	return k >= 1 ? to : from + (to - from) * ds_out(k);
}

/* The cursor moved: the window glides in ms.move, the ring in ms.quick, both
 * from where they are now. A jump of more than a row - a page, round an
 * end - shows the place it lands at and a short glide into it, as the row's
 * jumps do, so no row races past. */
static void follow(gg_motion *m, int cursor, int count, uint32_t now)
{
	int cols = gg_cols(m);

	if (!m->set || m->count != count || m->cols != cols) { place(m, cursor, count); return; }
	if (m->cursor == cursor) return;
	m->top_from = eased(m->top_from, m->top_to, now - m->moved, DS_MS_MOVE);
	m->fx_from = eased(m->fx_from, (float)(m->cursor % cols), now - m->ring_moved, DS_MS_QUICK);
	m->fy_from = eased(m->fy_from, (float)(m->cursor / cols), now - m->ring_moved, DS_MS_QUICK);
	m->top_to = (float)window_top(cursor, count, cols, (int)lroundf(m->top_to));
	ds_focus_weights_move(&m->fw, m->cursor, now - m->ring_moved);
	if (abs(cursor / cols - m->cursor / cols) > 1) {
		int dir = cursor > m->cursor ? 1 : -1;

		m->top_from = m->top_to - (m->top_to != m->top_from ? dir * .2f : 0);
		m->fx_from = (float)(cursor % cols);
		m->fy_from = (float)(cursor / cols) - dir * .2f;
	}
	m->cursor = cursor;
	m->moved = m->ring_moved = now;
}

/* The shape, from the covers in hand, until it settles */
static void measure(gg_motion *m, const gg_info *in, void *ctx, uint32_t now)
{
	float art[256];
	int n = 0, cutouts = 0, missing = 0;
	gg_geom g = geom_of(m);
	int first = (int)m->top_to * g.cols, last = first + GG_ROWS * g.cols;

	if (m->settled) return;
	for (int k = 0; k < in->count && n < 256; k++) {
		/* nearest the cursor first, so a long shelf is measured where it is
		 * looked at */
		int i = (in->cursor + (k + 1) / 2 * (k % 2 ? 1 : -1) + in->count) % in->count;
		int w = 0, h = 0;
		float cb = 1;
		SDL_Texture *t;
		const gg_art *e;

		if (in->folder_of && in->folder_of(ctx, i) > 0) continue;
		t = in->peek ? in->peek(ctx, i, &w, &h, &cb) : NULL;
		if (!t || w <= 0 || h <= 0) {
			if (i >= first && i < last) missing++;
			continue;
		}
		e = in->art_of ? in->art_of(ctx, i) : NULL;
		if ((e && e->generated) || (!e && ui_card_theme(t) >= 0)) continue;
		if (e && e->cutout) { cutouts++; continue; }
		art[n++] = w / (h * (cb > 0 && cb <= 1 ? cb : 1));
	}
	m->measured = gg_shape(art, n, cutouts);
	if (in->kept <= 0) m->shape = m->measured;
	if (now - m->opened >= GG_SETTLE_MS || (in->kept > 0 && !missing) ||
	    (!missing && now - m->opened >= 300))
		m->settled = true;
}

/* ---- drawing ------------------------------------------------------------- */

/* A texture coordinate held to the texture, 0 to 1.
 *
 * SDL 2's SDL_RenderGeometry draws NOTHING when one coordinate of one vertex
 * is outside 0..1: it returns -1 and the whole shape is gone (SDL_render.c,
 * "Values of 'uv' out of bounds"). tex_round works a vertex's coordinate out
 * from its place on screen, (py - dst.y) / dst.h, and while the grid glides a
 * row's y is fractional: py is dst.y + rad - rad, which in floats is not
 * always dst.y, so the edge's coordinate came out a hair past 1 (or under 0).
 * The handhelds run SDL 2.32.10, and there a whole row of cells lost its
 * picture for that frame and had it back the next: a name card's ground,
 * whose middle is cut to the cell's shape from its full height, so its top
 * and foot are the texture's edges; a cover whose art fills the cell's
 * height, now and then. The Grid's flicker while it scrolled (2026-11-18).
 * A computer's sdl2-compat (SDL 3) checks nothing, so shots never showed it. */
static float uv_in(float t)
{
	return t < 0 ? 0 : t > 1 ? 1 : t;
}

/* A texture's part `src` into `dst` with its corners rounded by `rad`: a fan
 * of triangles round the outline, the texture mapped across it */
static void tex_round(SDL_Renderer *r, SDL_Texture *t, int tw, int th, SDL_FRect src,
                      SDL_FRect dst, float rad, Uint8 alpha)
{
	enum { SEG = 6, NP = 4 * (SEG + 1) };
	SDL_Vertex v[NP + 1];
	int idx[NP * 3], n = 0, k;
	SDL_Color c = { 255, 255, 255, alpha };
	static const float cx[4] = { 1, 1, 0, 0 }, cy[4] = { 0, 1, 1, 0 };

	/* MENU > Display > Theme's Square corners square a cover as every panel
	 * (DESIGN.md 3.5), as ui_round_rect squares the ring round it */
	if (look_square() && rad * 2 < fminf(dst.w, dst.h)) rad = 0;
	if (rad > dst.w / 2) rad = dst.w / 2;
	if (rad > dst.h / 2) rad = dst.h / 2;
	v[0] = (SDL_Vertex){ { dst.x + dst.w / 2, dst.y + dst.h / 2 }, c,
	                     { uv_in((src.x + src.w / 2) / tw), uv_in((src.y + src.h / 2) / th) } };
	for (k = 0; k < 4; k++) {
		float ox = dst.x + (cx[k] ? dst.w - rad : rad), oy = dst.y + (cy[k] ? dst.h - rad : rad);

		for (int s = 0; s <= SEG; s++) {
			float a = (float)M_PI * (-0.5f + 0.5f * k + 0.5f * s / SEG);
			float px = ox + cosf(a) * rad, py = oy + sinf(a) * rad;

			v[1 + n++] = (SDL_Vertex){ { px, py }, c,
			                           { uv_in((src.x + (px - dst.x) / dst.w * src.w) / tw),
			                             uv_in((src.y + (py - dst.y) / dst.h * src.h) / th) } };
		}
	}
	for (k = 0; k < NP; k++) {
		idx[k * 3] = 0;
		idx[k * 3 + 1] = 1 + k;
		idx[k * 3 + 2] = 1 + (k + 1) % NP;
	}
	SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
	SDL_RenderGeometry(r, t, v, NP + 1, idx, NP * 3);
}

static SDL_Rect whole(SDL_FRect f)
{
	return (SDL_Rect){ (int)lroundf(f.x), (int)lroundf(f.y), (int)lroundf(f.w), (int)lroundf(f.h) };
}

/* The rect a picture of w x h takes whole in `cell`, `pad` from its sides */
static SDL_FRect fit_in(SDL_FRect cell, float w, float h, float pad)
{
	float f = fminf((cell.w - 2 * pad) / w, (cell.h - 2 * pad) / h);

	return (SDL_FRect){ cell.x + (cell.w - w * f) / 2, cell.y + (cell.h - h * f) / 2, w * f, h * f };
}

/* A name broken into lines no wider than `w` in face `f`, word by word, a
 * word wider than a line in pieces as long as fit with a hyphen, cut at a
 * whole UTF-8 character, `split` then true. Returns the lines, at most
 * `most`, or most + 1 when the name needs more. */
static int wrap(TTF_Font *f, const char *title, int w, char lines[][LB_LINE], int most, bool *split)
{
	int n = 0;

	*split = false;

	lines[0][0] = '\0';
	for (const char *p = title; *p; ) {
		char try[LB_LINE];
		int len = 0;

		while (*p == ' ') p++;
		if (!*p) break;
		if (n >= most) return most + 1;
		while (p[len] && p[len] != ' ' && len < LB_LINE - 2) len++;
		snprintf(try, sizeof try, "%s%s%.*s", lines[n], lines[n][0] ? " " : "", len, p);
		if (ui_text_width(f, try) <= w) {
			snprintf(lines[n], LB_LINE, "%s", try);
			p += len;
			continue;
		}
		if (lines[n][0]) { if (++n < most) lines[n][0] = '\0'; continue; }
		{
			int cut = len;

			while (cut > 1) {
				do cut--; while (cut > 1 && ((unsigned char)p[cut] & 0xC0) == 0x80);
				snprintf(try, sizeof try, "%.*s-", cut, p);
				if (ui_text_width(f, try) <= w) break;
			}
			snprintf(lines[n], LB_LINE, "%.*s-", cut, p);
			*split = true;
			p += cut;
			if (++n < most) lines[n][0] = '\0';
		}
	}
	if (n < most && lines[n][0]) n++;
	return n;
}

/* The name on a drawn card, `w` across and `h` down: in the largest of
 * support, action and chrome type it fits in whole words, in lines of that
 * type's box; in chrome, words split, as a last resort. Never cut (DESIGN.md
 * 1, 8). Returns the lines; the face and the line box in `font`, `box`. */
#define GG_CARD_LINES 6
static int card_lines(const char *title, int w, int h, char lines[][LB_LINE],
                      TTF_Font **font, int *box)
{
	static const struct { page_font_role role; int box; } faces[] = {
		{ PAGE_META, 36 }, { PAGE_ACTION, 32 }, { PAGE_CHROME, 28 },
	};
	int n = 0;

	for (unsigned k = 0; k < sizeof faces / sizeof *faces; k++) {
		TTF_Font *f = page_font(faces[k].role);
		int most = h / faces[k].box;
		bool split;

		if (most > GG_CARD_LINES) most = GG_CARD_LINES;
		if (most < 1) most = 1;
		*font = f;
		*box = faces[k].box;
		n = wrap(f, title, w, lines, most, &split);
		if (n <= most && !split) return n;
		if (k + 1 == sizeof faces / sizeof *faces) return n > most ? most : n;
	}
	return n;
}

/* ---- a game's card in a cell (src/cardart.h) ----
 *
 * The card's ground at the cell's shape, and its name set here at the cell's
 * own size by the card's rules: hard left at CA_NAME_X of CA_CARD_W of the
 * cell, the first line at CA_NAME_TOP% of it, near-white on a dark band, the system's rule under it.
 * One size in every cell, chrome type: a cell is 160 wide (100 on the Pixel
 * 2), and in support or action type a word as plain as "Castlevania" is
 * wider than that, so the names of a Grid would be in three sizes. */
static const page_font_role GG_NAME_ROLE[CA_SIZES] = { PAGE_CHROME, PAGE_CHROME, PAGE_CHROME };
static const int GG_CARD_BOX[CA_SIZES] = { 28, 28, 28 };

/* cardart_name's sizes as the grid's faces: CA_SIZE_STEPS[k] is role k; the
 * part after " - " (6/10 of one) is chrome */
static int gg_size_ix(int size)
{
	for (int k = 0; k < CA_SIZES; k++)
		if (size == CA_SIZE_STEPS[k]) return k;
	return CA_SIZES - 1;
}

static int gg_name_width(void *ctx, int size, const char *s)
{
	(void)ctx;
	return ui_text_width(page_font(GG_NAME_ROLE[gg_size_ix(size)]), s);
}

/* The shade behind a card's name: black at 133 from y0 to y1, `feather`
 * rows fading out each way, never in the rows of the cell's round corners.
 *
 * ONE DRAW, each run of rows of one shade a flat quad in it. It was a fill a
 * row, some 90 a card, and the Pixel 2's GPU (panfrost) takes each as a draw
 * of its own (src/ui.c's ui_bake says the same of Screen Colors' bars): a
 * Grid of games with no art, a dozen cards in sight, glided at 20 frames a
 * second where covers glide at 60 (2026-11-17). The same rows, the same
 * shades, so the same pixels. */
static void gg_band(SDL_Renderer *r, SDL_Rect q, int y0, int y1, int feather)
{
	enum { QUADS = 16 };
	static const int ix[6] = { 0, 1, 2, 0, 2, 3 };
	SDL_Vertex v[QUADS * 4];
	int idx[QUADS * 6], n = 0, f = feather > 0 ? feather : 1;
	int top = q.y + DS_RADIUS_CELL, foot = q.y + q.h - DS_RADIUS_CELL;

	SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
	for (int y = y0 - feather; y < y1 + feather;) {
		int in = y < y0 ? y - (y0 - feather) : y >= y1 ? (y1 + feather) - y : feather;
		Uint8 a = (Uint8)(133 * in / f);
		int end = y + 1;

		/* the rows on of the same shade */
		while (end < y1 + feather) {
			int k = end < y0 ? end - (y0 - feather) : end >= y1 ? (y1 + feather) - end : feather;

			if ((Uint8)(133 * k / f) != a) break;
			end++;
		}
		{
			int from = y > top ? y : top, to = end < foot ? end : foot;
			SDL_Color c = { 0, 0, 0, a };

			/* an alpha of 0 leaves the pixels as they are */
			if (a && from < to) {
				float x0 = (float)q.x, x1 = (float)(q.x + q.w);

				v[n * 4 + 0] = (SDL_Vertex){ { x0, (float)from }, c, { 0, 0 } };
				v[n * 4 + 1] = (SDL_Vertex){ { x1, (float)from }, c, { 0, 0 } };
				v[n * 4 + 2] = (SDL_Vertex){ { x1, (float)to }, c, { 0, 0 } };
				v[n * 4 + 3] = (SDL_Vertex){ { x0, (float)to }, c, { 0, 0 } };
				for (int k = 0; k < 6; k++) idx[n * 6 + k] = n * 4 + ix[k];
				n++;
			}
		}
		/* full: what is gathered goes, and the rest gathers anew */
		if (n == QUADS) {
			SDL_RenderGeometry(r, NULL, v, n * 4, idx, n * 6);
			n = 0;
		}
		y = end;
	}
	if (n) SDL_RenderGeometry(r, NULL, v, n * 4, idx, n * 6);
}

static void grid_named(SDL_Renderer *r, SDL_FRect c, const char *title, unsigned rgb,
                       SDL_Texture *ground)
{
	SDL_Rect q = whole(c);
	int gw = 0, gh = 0, pad = (int)lroundf(c.w * CA_NAME_X / CA_CARD_W), y0, y, box, sbox = GG_CARD_BOX[CA_SIZES - 1];
	int room;
	unsigned rule = ui_sys_mark_in(rgb, DS_DARK);
	const SDL_Color ink = { 0xF6, 0xF5, 0xF1, 255 }, ink2 = { 0xE2, 0xE0, 0xDA, 255 }, shade = { 0, 0, 0, 110 };
	TTF_Font *f, *g = page_font(GG_NAME_ROLE[CA_SIZES - 1]);
	ca_name n;

	/* the ground, its middle at the cell's shape */
	SDL_QueryTexture(ground, NULL, NULL, &gw, &gh);
	{
		float ca = c.w / c.h;
		SDL_FRect src = { 0, 0, (float)gw, (float)gh };

		if ((float)gw / gh > ca) { src.w = gh * ca; src.x = (gw - src.w) / 2; }
		else { src.h = gw / ca; src.y = (gh - src.h) / 2; }
		tex_round(r, ground, gw, gh, src, c, DS_RADIUS_CELL, 255);
	}
	y0 = q.y + (int)lroundf(c.h * CA_NAME_TOP / 100);
	/* what the cell holds below the first line, at the largest box, with
	 * the rule and its margin */
	room = (q.y + q.h - pad - 16 - y0) / GG_CARD_BOX[0];
	/* the name may run nearer the right edge than the left: nothing is
	 * there, and "Hedgehog 2" then fits whole beside its 2 */
	if (!cardart_name(title && *title ? title : "?", q.w - pad - pad / 4, room < 1 ? 1 : room,
	                  gg_name_width, NULL, &n))
		return;
	f = page_font(GG_NAME_ROLE[gg_size_ix(n.size)]);
	box = GG_CARD_BOX[gg_size_ix(n.size)];
	y = y0 + n.n * box + (n.sub_n ? 4 + n.sub_n * sbox : 0);
	gg_band(r, q, y0 - 8, y + 8 + DS_RULE + 8, 6);
	y = y0;
	for (int k = 0; k < n.n; k++, y += box) {
		ui_text(r, f, n.line[k], q.x + pad, ui_cap_y(f, y + box / 2) + 2, -1, shade);
		ui_text(r, f, n.line[k], q.x + pad, ui_cap_y(f, y + box / 2), -1, ink);
	}
	if (n.sub_n) {
		y += 4;
		for (int k = 0; k < n.sub_n; k++, y += sbox) {
			ui_text(r, g, n.sub[k], q.x + pad, ui_cap_y(g, y + sbox / 2) + 2, -1, shade);
			ui_text(r, g, n.sub[k], q.x + pad, ui_cap_y(g, y + sbox / 2), -1, ink2);
		}
	}
	SDL_SetRenderDrawColor(r, (Uint8)(rule >> 16), (Uint8)(rule >> 8), (Uint8)rule, 255);
	SDL_RenderFillRect(r, &(SDL_Rect){ q.x + pad, y + 8, (int)lroundf(c.w * .14f), DS_RULE });
}

/* ---- a game's card, drawn once at its cell's size ----
 *
 * grid_named is about seven draws a card (its ground, the shade, the name's
 * lines and their shadows, the rule) in two kinds, a texture's and a
 * color's, and the Pixel 2's GPU (panfrost) pays for each draw and each
 * change of kind on the frame's own thread: a Grid of games with no art
 * glided at 30 frames a second where one of covers, a draw a cell, glides at
 * 60 (2026-11-18: 33.1 ms frames against 16.6 on the same taps). So the card
 * is drawn once into a texture of the cell's size and that is copied, one
 * draw, as a cover is. Whole pixels both ways (the copy lands on the cell's
 * whole rect, as the name always did), so a card at rest is the very pixels
 * it was; while the grid glides its ground moves with its name, a whole pixel
 * at a time, where it moved by fractions under it before.
 *
 * Kept with what the grid knows of the game (gg_art), so it goes when the
 * game's cover does (evict_grid, evict_far: gg_drop) or its card is made
 * again (gg_note). 160 x 224 on the canvas, 140 KB. */

/* At most this many drawn a frame: each one turns the GPU to another
 * target and back. The rest are drawn as before until their turn; a grid at
 * rest goes on drawing until the cards in sight and the rows either side are
 * done (named_ahead). */
#define GG_CARDS_A_FRAME 2
static int  g_cards_left;
static bool g_cards_wanted, g_cards_off;

static unsigned card_for(const char *title, unsigned rgb)
{
	unsigned h = 2166136261u ^ rgb;

	for (const char *p = title ? title : ""; *p; p++) h = (h ^ (unsigned char)*p) * 16777619u;
	return h ^ (look_square() ? 0x9e3779b9u : 0);
}

static bool card_current(const gg_art *e, int w, int h, unsigned key)
{
	return e->card && e->card_w == w && e->card_h == h && e->card_for == key &&
	       e->card_face == (const void *)page_font(PAGE_CHROME);
}

/* grid_named into a texture of w x h, clear where it draws nothing (the
 * round corners), the renderer's target, viewport, clip, scale, blend and
 * color put back after, as src/ui.c's ui_bake does */
static SDL_Texture *card_draw(SDL_Renderer *r, int w, int h, const char *title, unsigned rgb,
                              SDL_Texture *ground)
{
	SDL_Texture *t = SDL_CreateTexture(r, PLAT_TARGET_FORMAT, SDL_TEXTUREACCESS_TARGET, w, h), *was;
	SDL_Rect view, clip;
	SDL_bool clipped;
	SDL_BlendMode mode;
	Uint8 cr, cg, cb, ca;
	float sx, sy;

	if (!t) return NULL;
	was = SDL_GetRenderTarget(r);
	SDL_RenderGetViewport(r, &view);
	clipped = SDL_RenderIsClipEnabled(r);
	SDL_RenderGetClipRect(r, &clip);
	SDL_RenderGetScale(r, &sx, &sy);
	SDL_GetRenderDrawBlendMode(r, &mode);
	SDL_GetRenderDrawColor(r, &cr, &cg, &cb, &ca);
	if (SDL_SetRenderTarget(r, t) != 0) {
		SDL_DestroyTexture(t);
		return NULL;
	}
	SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
	SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
	SDL_RenderClear(r);
	grid_named(r, (SDL_FRect){ 0, 0, (float)w, (float)h }, title, rgb, ground);
	SDL_SetRenderTarget(r, was);
	SDL_RenderSetScale(r, sx, sy);
	SDL_RenderSetViewport(r, &view);
	SDL_RenderSetClipRect(r, clipped ? &clip : NULL);
	SDL_SetRenderDrawBlendMode(r, mode);
	SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
	SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
	return t;
}

/* Game `e`'s card drawn at w x h if it is not, while this frame may draw
 * one: whether it is there to copy */
static bool card_ready(SDL_Renderer *r, gg_art *e, int w, int h, const char *title, unsigned rgb)
{
	unsigned key = card_for(title, rgb);

	if (card_current(e, w, h, key)) return true;
	if (g_cards_off) return false;
	if (g_cards_left <= 0) {
		g_cards_wanted = true;
		return false;
	}
	g_cards_left--;
	if (e->card) SDL_DestroyTexture(e->card);
	e->card = card_draw(r, w, h, title, rgb, e->blur);
	if (!e->card) {
		/* no texture to draw into: drawn as before, every frame */
		g_cards_off = true;
		return false;
	}
	e->card_w = w;
	e->card_h = h;
	e->card_for = key;
	e->card_face = page_font(PAGE_CHROME);
	return true;
}

/* A game's card in `cell`: copied, or drawn as before until it is */
static void named_card(SDL_Renderer *r, gg_art *e, SDL_FRect cell, SDL_FRect base, const char *title, unsigned rgb)
{
	SDL_Rect q = whole(cell), b = whole(base);

	/* Made at the cell's own size and drawn at the size the focus grows it
	 * to, so a growing card neither lays its name out again nor is made
	 * again on every frame of the glide */
	if (card_ready(r, e, b.w, b.h, title, rgb)) SDL_RenderCopy(r, e->card, NULL, &q);
	else grid_named(r, cell, title, rgb, e->blur);
}

/* A game with no art: the generated card, drawn here at the cell's shape -
 * the theme's slab falling toward its ground, the system's color as a band
 * across the top, the name in support type a fifth of the way down, a rule
 * of the system's mark color under it (src/ui.c's game_card_surface, at the
 * size the grid shows it, so its name reads) */
static void grid_card(SDL_Renderer *r, SDL_FRect c, const char *title, unsigned rgb)
{
	int ti = theme_index();
	ds_rgb top = cardgen_slab(ti, 0), foot = cardgen_slab(ti, 1);
	SDL_Color ct = { top.r, top.g, top.b, 255 }, cf = { foot.r, foot.g, foot.b, 255 };
	SDL_Rect q = whole(c);
	TTF_Font *f;
	char lines[GG_CARD_LINES + 1][LB_LINE];
	int pad = (int)lroundf(c.w * .1f), n, y, box;
	unsigned band = rgb == CFG_NO_COLOR ? ui_rgb(theme()->text_dim) : rgb;
	unsigned rule = ui_sys_mark(rgb);
	SDL_Vertex v[4] = {
		{ { c.x, c.y }, ct, { 0, 0 } }, { { c.x + c.w, c.y }, ct, { 0, 0 } },
		{ { c.x + c.w, c.y + c.h }, cf, { 0, 0 } }, { { c.x, c.y + c.h }, cf, { 0, 0 } },
	};
	static const int idx[6] = { 0, 1, 2, 0, 2, 3 };

	/* The slab, its corners the cell's: the round rect in the slab's middle
	 * color, the gradient over its straight middle */
	ui_round_rect(r, &q, DS_RADIUS_CELL, ui_col(cardgen_slab(ti, .5f), 255));
	v[0].position.y = v[1].position.y = c.y + DS_RADIUS_CELL;
	v[2].position.y = v[3].position.y = c.y + c.h - DS_RADIUS_CELL;
	SDL_RenderGeometry(r, NULL, v, 4, idx, 6);
	ui_round_rect(r, &(SDL_Rect){ q.x, q.y, q.w, DS_RADIUS_CELL * 2 }, DS_RADIUS_CELL, ct);
	SDL_SetRenderDrawColor(r, ct.r, ct.g, ct.b, 255);
	SDL_RenderFillRect(r, &(SDL_Rect){ q.x, q.y + DS_RADIUS_CELL, q.w, DS_RADIUS_CELL });
	ui_round_rect(r, &(SDL_Rect){ q.x, q.y + q.h - DS_RADIUS_CELL * 2, q.w, DS_RADIUS_CELL * 2 }, DS_RADIUS_CELL, cf);
	SDL_SetRenderDrawColor(r, cf.r, cf.g, cf.b, 255);
	SDL_RenderFillRect(r, &(SDL_Rect){ q.x, q.y + q.h - DS_RADIUS_CELL * 2, q.w, DS_RADIUS_CELL });
	/* The band, inside the corners */
	SDL_SetRenderDrawColor(r, (Uint8)(band >> 16), (Uint8)(band >> 8), (Uint8)band, 255);
	SDL_RenderFillRect(r, &(SDL_Rect){ q.x + DS_RADIUS_CELL, q.y, q.w - 2 * DS_RADIUS_CELL, 4 });
	y = q.y + (int)lroundf(c.h * .22f);
	n = card_lines(title && *title ? title : "?", q.w - 2 * pad, q.y + q.h - pad - 16 - y,
	               lines, &f, &box);
	/* a name of many lines starts higher, so it ends inside the card */
	if (y + n * box + 16 > q.y + q.h - pad) y = q.y + q.h - pad - 16 - n * box;
	if (y < q.y + pad) y = q.y + pad;
	for (int k = 0; k < n; k++) {
		ui_text(r, f, lines[k], q.x + pad, ui_cap_y(f, y + box / 2), -1,
		        ui_col(cardgen_title(ti), 255));
		y += box;
	}
	SDL_SetRenderDrawColor(r, (Uint8)(rule >> 16), (Uint8)(rule >> 8), (Uint8)rule, 220);
	SDL_RenderFillRect(r, &(SDL_Rect){ q.x + pad, y + 8, (int)lroundf(c.w * .18f), DS_RULE });
}

/* One cover in its cell, grown from `base` by the focus (the cell itself
 * when out of focus); returns the rect the ring goes round */
static SDL_FRect cover(SDL_Renderer *r, const gg_info *in, void *ctx, int i,
                       SDL_Texture *t, int tw, int th, float cb, SDL_FRect cell, SDL_FRect base)
{
	gg_art *e = in->art_of ? in->art_of(ctx, i) : NULL;
	bool folder = in->folder_of && in->folder_of(ctx, i) > 0;
	float ah = th * (cb > 0 && cb <= 1 ? cb : 1), ca = cell.w / cell.h, rel = tw / ah / ca;
	SDL_FRect src = { 0, 0, (float)tw, ah };

	/* A folder is its picture, not a card: whole, on the ground */
	if (folder) {
		SDL_FRect in_ = fit_in(cell, (float)tw, ah, 0);

		SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
		SDL_RenderCopyF(r, t, &(SDL_Rect){ 0, 0, tw, (int)ah }, &in_);
		return in_;
	}
	if (e && e->generated && e->blur) {
		/* a game's card: its ground, and its name at the cell's size */
		named_card(r, e, cell, base, in->title_of(ctx, i), in->accent_of ? in->accent_of(ctx, i) : in->accent);
		return cell;
	}
	if ((e && e->generated) || (!e && ui_card_theme(t) >= 0)) {
		if (tw > 0 && th > 0 && tw != th) {
			/* a game's card with no ground at hand: the card itself, its
			 * name with it, at the cell's shape */
			if (rel > 1) { src.w = ah * ca; src.x = (tw - src.w) / 2; }
			else { src.h = tw / ca; src.y = (ah - src.h) / 2; }
			tex_round(r, t, tw, th, src, cell, DS_RADIUS_CELL, 255);
			return cell;
		}
		grid_card(r, cell, in->title_of(ctx, i), in->accent_of ? in->accent_of(ctx, i) : in->accent);
		return cell;
	}
	if (e && e->cutout) {
		/* A logo, a box on nothing: whole, on a plain card, a margin round it */
		SDL_Rect q = whole(cell);
		SDL_FRect in_ = fit_in(cell, (float)tw, ah, cell.w * .12f);

		ui_round_rect(r, &q, DS_RADIUS_CELL, ui_col(theme()->surface, 255));
		SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
		SDL_RenderCopyF(r, t, &(SDL_Rect){ 0, 0, tw, (int)ah }, &in_);
		return cell;
	}
	if (rel > 1 / GG_NEAR && rel < GG_NEAR) {
		/* Near the cell's shape: fills it, the sliver over cut from its middle */
		if (rel > 1) { src.w = ah * ca; src.x = (tw - src.w) / 2; }
		else { src.h = tw / ca; src.y = (ah - src.h) / 2; }
		tex_round(r, t, tw, th, src, cell, DS_RADIUS_CELL, 255);
		return cell;
	}
	/* Another shape: whole in the middle of the cell, over its own colors,
	 * blurred and toned toward the ground so the art stands in front; a plain
	 * card behind it while its colors are not in hand */
	{
		SDL_Rect q = whole(cell);
		SDL_FRect in_ = fit_in(cell, (float)tw, ah, 0);
		int bw, bh;

		if (e && e->blur && !SDL_QueryTexture(e->blur, NULL, NULL, &bw, &bh)) {
			/* the blur's middle at the cell's shape */
			SDL_FRect bs = { 0, 0, (float)bw, (float)bh };
			ds_rgb g = theme()->ground;

			if ((float)bw / bh > ca) { bs.w = bh * ca; bs.x = (bw - bs.w) / 2; }
			else { bs.h = bw / ca; bs.y = (bh - bs.h) / 2; }
			tex_round(r, e->blur, bw, bh, bs, cell, DS_RADIUS_CELL, 255);
			ui_round_rect(r, &q, DS_RADIUS_CELL,
			              (SDL_Color){ g.r, g.g, g.b, theme_index() == DS_DARK ? 90 : 50 });
		} else {
			ui_round_rect(r, &q, DS_RADIUS_CELL, ui_col(theme()->surface, 255));
		}
		tex_round(r, t, tw, th, src, in_, DS_RADIUS_CELL / 2.0f, 255);
		return cell;
	}
}

static SDL_FRect cell_rect(const gg_geom *g, float col, float row, float top)
{
	return (SDL_FRect){ (float)(g->x + col * (g->cw + g->gap)), (float)(g->y + (row - top) * (g->ch + g->gap)),
	                    (float)g->cw, (float)g->ch };
}

/* Highlight's marks round the focus `f` where it is now, gliding (src/ds/
 * focus.h, DESIGN.md 11.47): Border's ring, Bar's bar under it, both on
 * whole pixels round the cover grown as the focus grows it; Lift and None
 * mark nothing, the cover itself grows (and rises) */
static void marks_round(SDL_Renderer *r, const ds_focus_marks *mk, SDL_FRect f, Uint8 alpha)
{
	SDL_FRect t = ds_focus_tile(f, mk, 1);

	if (mk->ring) ds_focus_ring(r, t, DS_RADIUS_CELL, alpha);
	if (mk->bar) ds_focus_bar(r, t, alpha);
}

/* The rail at the right, 23 from the edge: where the window is in the grid */
static void rail(SDL_Renderer *r, const gg_geom *g, float top, int count)
{
	int rows = (count + g->cols - 1) / g->cols;
	int h = GG_ROWS * g->ch + (GG_ROWS - 1) * g->gap, x = ZYPEROS_SCREEN_W - DS_RAIL_INSET - DS_BAR;
	int mh, my;

	if (rows <= GG_ROWS) return;
	mh = h * GG_ROWS / rows;
	if (mh < DS_RAIL_MIN) mh = DS_RAIL_MIN;
	my = g->y + (int)lroundf((h - mh) * top / (rows - GG_ROWS));
	ui_round_rect(r, &(SDL_Rect){ x, g->y, DS_BAR, h }, DS_BAR / 2, ui_col(theme()->track, 255));
	ui_round_rect(r, &(SDL_Rect){ x, my, DS_BAR, mh }, DS_BAR / 2, ui_col(theme()->accent_ink, 255));
}

/* A part of the facts' line at x in support type, or nothing once a part did
 * not fit (x < 0): returns where the next one starts */
static int fact_in(SDL_Renderer *r, const char *s, int x, int cy, int right, SDL_Color c)
{
	TTF_Font *f = page_font(PAGE_META);
	int w;

	if (x < 0 || !s || !*s) return x;
	w = ui_text_width(f, s);
	if (x + w > right) return -1;
	ui_text(r, f, s, x, ui_cap_y(f, cy), -1, c);
	return x + w + 32;
}

static int fact(SDL_Renderer *r, const char *s, int x, int cy, int right)
{
	return fact_in(r, s, x, cy, right, UI_TEXT_DIM);
}

static const char *a_word(const gg_info *in, int games)
{
	if (in->a_word) return in->a_word;
	return T(games > 0 || in->opens ? S_GAMES_OPEN : in->howto ? S_PICO8_HOW_TO_ADD
	         : in->install ? S_PORTMASTER_INSTALL
	         : in->resumable ? S_GAMES_RESUME : S_GAMES_PLAY);
}

/* The band: the name with a favorite's heart before it, sliding when too
 * long, A at its right; the facts under it, the heading (the system, the
 * folder) first */
static void band(SDL_Renderer *r, const gg_info *in, void *ctx, int games, int sh,
                 uint32_t phase, uint32_t *wait)
{
	TTF_Font *title = page_font(PAGE_GAME_TITLE), *meta = page_font(PAGE_META);
	int top = GG_BAND_TOP(sh), ny = top + GG_NAME_BOX / 2;
	int fy = top + GG_NAME_BOX + GG_RELATED + GG_FACTS_BOX / 2;
	int right = ZYPEROS_SCREEN_W - GG_MARGIN, x = GG_MARGIN, room;
	int aw = in->count ? page_hint_width("A", a_word(in, games), true) : 0;
	const char *name = in->count ? in->title_of(ctx, in->cursor) : T(S_GAMES_NO_GAMES);
	char line[96];
	int hw;

	room = right - (aw ? aw + 32 : 0) - x;
	if (in->count && in->fav_of && in->fav_of(ctx, in->cursor)) {
		float hr = ui_cap_height(title) * .55f;

		draw_heart(r, x + hr, (float)ny, hr, ui_col(theme()->favorite, 255));
		x += (int)lroundf(hr * 2) + 16;
		room -= (int)lroundf(hr * 2) + 16;
	}
	if (ui_text_width(title, name) > room) {
		unsigned next;

		ui_text_marquee(r, title, name, x, ui_cap_y(title, ny), room, phase, UI_TEXT);
		next = ui_pingpong_wait(ui_text_width(title, name) - room, phase);
		if (next < *wait) *wait = next;
	} else {
		ui_text(r, title, name, x, ui_cap_y(title, ny), -1, UI_TEXT);
	}
	if (in->count) page_hint(r, "A", a_word(in, games), right - aw, ny, true, false);

	/* The heading, sliding over the whole line when it is wider than it */
	x = GG_MARGIN;
	hw = ui_text_width(meta, in->heading ? in->heading : "");
	if (hw > right - x) {
		unsigned next;

		ui_text_marquee(r, meta, in->heading, x, ui_cap_y(meta, fy), right - x, phase, UI_TEXT_DIM);
		next = ui_pingpong_wait(hw - (right - x), phase);
		if (next < *wait) *wait = next;
		return;
	}
	x = fact(r, in->heading, x, fy, right);
	if (games > 0) {
		snprintf(line, sizeof line, TN(S_SHELF_GAMES, games), games);
		fact(r, line, x, fy, right);
		return;
	}
	if (!in->count) return;
	x = fact(r, in->origin, x, fy, right);
	if (in->install || in->howto) x = fact(r, T(S_PORTMASTER_NOT_INSTALLED), x, fy, right);
	/* Firmware it cannot start without, in **warning** */
	x = fact_in(r, in->needs, x, fy, right, ui_col(theme()->warning, 255));
	if (in->played && *in->played) {
		snprintf(line, sizeof line, T(S_GAMES_PLAYED), in->played);
		x = fact(r, line, x, fy, right);
	}
	if (in->has_achievements) {
		snprintf(line, sizeof line, T(S_GAMES_ACHIEVEMENTS), in->earned, in->total);
		x = fact(r, line, x, fy, right);
	}
	/* and how many of its cheats are on (DESIGN.md 11.66) */
	if (in->cheats && *in->cheats) fact(r, in->cheats, x, fy, right);
}

/* The video snap: the focused cell opens to two cells across (no wider than
 * the grid, no taller than it) over its neighbors, at the video's own shape,
 * growing from the focused cover as Highlight grew it, as the cover row's
 * turns wide, with Highlight's lift and marks going with it, round the
 * video itself */
static void snap(SDL_Renderer *r, const gg_info *in, const gg_geom *g, SDL_FRect cell)
{
	const ds_focus_marks mk = ds_focus_marks_of(DS_FOCUS_COVER);
	const SDL_FRect c = ds_focus_tile(cell, &mk, 1);
	float gw = (float)(g->cols * g->cw + (g->cols - 1) * g->gap);
	float gh = (float)(GG_ROWS * g->ch + (GG_ROWS - 1) * g->gap);
	int sw = in->snap_src.w > 0 ? in->snap_src.w : in->snap_w;
	int sh = in->snap_src.h > 0 ? in->snap_src.h : in->snap_h;
	float vw = fminf(2.0f * g->cw + g->gap, gw), vh, k = fminf(in->snap_k, 1);
	SDL_FRect v, d, lo;
	SDL_Rect lift;
	int qw, qh;

	if (sw <= 0 || sh <= 0) return;
	vh = vw * sh / sw;
	if (vh > gh) { vh = gh; vw = vh * sw / sh; }
	/* in the grid, or as far as the grown cover goes, so the video covers it */
	lo = (SDL_FRect){ fminf((float)g->x, c.x), fminf((float)g->y, c.y), 0, 0 };
	lo.w = fmaxf(g->x + gw, c.x + c.w) - lo.x;
	lo.h = fmaxf(g->y + gh, c.y + c.h) - lo.y;
	v = (SDL_FRect){ c.x + c.w / 2 - vw / 2, c.y + c.h / 2 - vh / 2, vw, vh };
	if (v.x < lo.x) v.x = lo.x;
	if (v.x + v.w > lo.x + lo.w) v.x = lo.x + lo.w - v.w;
	if (v.y < lo.y) v.y = lo.y;
	if (v.y + v.h > lo.y + lo.h) v.y = lo.y + lo.h - v.h;
	d = (SDL_FRect){ c.x + (v.x - c.x) * k, c.y + (v.y - c.y) * k,
	                 c.w + (v.w - c.w) * k, c.h + (v.h - c.h) * k };
	lift = whole(d);
	if (mk.lift) ui_lift_focus(r, &lift, mk.lift == DS_FOCUS_LIFT_HIGH, k);
	/* the marks the covers had, from where they were: no blink */
	if (mk.ring) ds_focus_ring(r, d, DS_RADIUS_CELL, 255);
	if (mk.bar) ds_focus_bar(r, d, 255);
	if (SDL_QueryTexture(in->snap, NULL, NULL, &qw, &qh)) return;
	tex_round(r, in->snap, qw, qh,
	          (SDL_FRect){ (float)in->snap_src.x, (float)in->snap_src.y, (float)sw, (float)sh },
	          d, DS_RADIUS_CELL, 255);
}

/* At rest, the cards of the rows just over and under the window drawn too,
 * as many as the frame may: the row a press brings in is copied from its
 * first frame of the glide */
static void named_ahead(SDL_Renderer *r, const gg_geom *g, const gg_info *in, void *ctx, int top)
{
	const int rows[2] = { top - 1, top + GG_ROWS };

	for (int k = 0; k < 2; k++) {
		if (rows[k] < 0) continue;
		for (int i = rows[k] * g->cols; i < (rows[k] + 1) * g->cols && i < in->count; i++) {
			int w = 0, h = 0;
			float cb = 1;
			gg_art *e;

			if (!in->peek || !in->peek(ctx, i, &w, &h, &cb) || (in->folder_of && in->folder_of(ctx, i) > 0))
				continue;
			e = in->art_of ? in->art_of(ctx, i) : NULL;
			if (e && e->generated && e->blur)
				card_ready(r, e, g->cw, g->ch, in->title_of(ctx, i),
				           in->accent_of ? in->accent_of(ctx, i) : in->accent);
		}
	}
}

uint32_t gg_draw(gg_motion *m, SDL_Renderer *r, const gg_info *in,
                 cf_tex_fn texture, void *ctx, uint32_t now, uint32_t phase)
{
	SDL_BlendMode blend;
	Uint8 red, green, blue, alpha;
	uint32_t wait = UINT32_MAX;
	float top, fx, fy;
	int first, games, sh = ZYPEROS_SCREEN_H;
	gg_geom g;
	SDL_Rect clip;
	SDL_FRect focus = { 0 };
	const ds_focus_marks mk = ds_focus_marks_of(DS_FOCUS_COVER);

	SDL_GetRenderDrawBlendMode(r, &blend);
	SDL_GetRenderDrawColor(r, &red, &green, &blue, &alpha);
	SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
	g_cards_left = GG_CARDS_A_FRAME;
	g_cards_wanted = false;
	if (!m->set || m->count != in->count) {
		gg_reset(m, in->cursor, in->count);
		m->shape = in->kept > 0 ? in->kept : GG_SHAPE_NONE;
		m->opened = now;
	}
	measure(m, in, ctx, now);
	if (!m->settled) wait = 0;
	follow(m, in->cursor, in->count, now);
	g = geom_of(m);
	if (gg_moving(m, now)) wait = 0;
	top = eased(m->top_from, m->top_to, now - m->moved, DS_MS_MOVE);
	fx = eased(m->fx_from, (float)(in->cursor % g.cols), now - m->ring_moved, DS_MS_QUICK);
	fy = eased(m->fy_from, (float)(in->cursor / g.cols), now - m->ring_moved, DS_MS_QUICK);
	games = in->count && in->folder_of ? in->folder_of(ctx, in->cursor) : 0;

	/* The grid, in a window it scrolls through, room round it for
	 * Highlight's marks and the lift (src/ds/focus.h) */
	clip = (SDL_Rect){ 0, g.y - 24, ZYPEROS_SCREEN_W, GG_ROWS * g.ch + (GG_ROWS - 1) * g.gap + 48 };
	SDL_RenderSetClipRect(r, &clip);
	if (in->count > 0) {
		SDL_FRect f = cell_rect(&g, fx, fy, top);
		int w = 0, h = 0;
		float cb = 1;
		SDL_Texture *t = texture(ctx, in->cursor, &w, &h, &cb);

		/* A folder is its picture: the marks go round the folder */
		if (t && w > 0 && h > 0 && in->folder_of && in->folder_of(ctx, in->cursor) > 0)
			f = fit_in(f, (float)w, h * (cb > 0 && cb <= 1 ? cb : 1), 0);
		focus = f;
	}
	first = (int)floorf(top) * g.cols;
	m->top_drawn = top;
	m->holes = 0;
	/* Twice: the covers out of focus, then over them the ones with a weight
	 * in the focus's glide, each grown with it on its lift; every cover
	 * with its hairline, and stepped back by what it is out of focus */
	for (int pass = 0; pass < 2; pass++)
	for (int i = first < 0 ? 0 : first; i < first + (GG_ROWS + 1) * g.cols && i < in->count; i++) {
		SDL_FRect c = cell_rect(&g, (float)(i % g.cols), (float)(i / g.cols), top), gc;
		const float wt = ds_focus_weight(&m->fw, i, in->cursor, now - m->ring_moved);
		int w = 0, h = 0;
		float cb = 1;
		bool folder;
		SDL_Texture *t;

		if ((wt > 0) != (pass == 1)) continue;
		if (c.y > clip.y + clip.h || c.y + c.h < clip.y) continue;
		t = texture(ctx, i, &w, &h, &cb);
		if (!t || w <= 0 || h <= 0) {
			/* still coming: the cell's place, quietly */
			SDL_Rect q = whole(c);

			m->holes++;
			ui_round_rect(r, &q, DS_RADIUS_CELL, ui_col(theme()->raised, 255));
			continue;
		}
		folder = in->folder_of && in->folder_of(ctx, i) > 0;
		gc = ds_focus_tile(c, &mk, wt);
		{
			SDL_FRect shape = folder ? fit_in(gc, (float)w, h * (cb > 0 && cb <= 1 ? cb : 1), 0) : gc;
			SDL_Rect lq = whole(shape);

			if (mk.lift && wt > .02f) ui_lift_focus(r, &lq, mk.lift == DS_FOCUS_LIFT_HIGH, wt);
			cover(r, in, ctx, i, t, w, h, cb, gc, c);
			/* What it is, at its art's corner as on the row (MP4's All),
			 * grown with the cell's lift */
			if (in->badge_of) {
				int k = in->badge_of(ctx, i);

				if (k != AB_NONE) ab_draw_at(r, (ab_kind)k, gc.x, gc.y, GG_BADGE_K * gc.w / c.w, 255);
			}
			if (!folder) ds_focus_hairline(r, gc, DS_RADIUS_CELL);
			ds_focus_dim(r, shape, DS_RADIUS_CELL, wt);
		}
		if (i != in->cursor) continue;
		if (mk.sheen) {
			const float st = (float)(now - m->ring_moved) / DS_MS_SHEEN;

			if (st < 1) {
				ds_focus_sheen(r, gc, st);
				wait = 0;
			}
		}
		/* The focused cover's video snap mark, and the wait before the
		 * snap: a bar along its foot, filling (DESIGN.md 11.19) */
		if (in->has_snap && !(in->snap && in->snap_k > 0))
			ds_play_disc(r, (int)lroundf(gc.x + gc.w) - DS_BADGE_INSET_SMALL - DS_BADGE_SMALL,
			             (int)lroundf(gc.y + gc.h) - DS_BADGE_INSET_SMALL - DS_BADGE_SMALL,
			             DS_BADGE_SMALL, 255);
		if (in->snap_wait > 0 && !(in->snap && in->snap_k > 0)) {
			int bw = (int)lroundf(gc.w) - 2 * DS_BADGE_INSET_SMALL;
			ds_bar(r, (int)lroundf(gc.x) + DS_BADGE_INSET_SMALL,
			       (int)lroundf(gc.y + gc.h) - DS_BADGE_INSET_SMALL - DS_BAR, bw,
			       (int)lroundf(bw * fminf(in->snap_wait, 1)), 255);
		}
	}
	/* The marks, over the covers, where the focus is now (gliding); under
	 * a video snap, the snap's own */
	if (in->count > 0 && !(in->snap && in->snap_k > 0)) marks_round(r, &mk, focus, 255);
	SDL_RenderSetClipRect(r, NULL);
	if (!gg_moving(m, now)) named_ahead(r, &g, in, ctx, (int)floorf(top));
	/* cards still to draw: another frame for them */
	if (g_cards_wanted) wait = 0;
	if (in->count > 0 && in->snap && in->snap_k > 0 && in->snap_w > 0 && in->snap_h > 0)
		snap(r, in, &g, cell_rect(&g, (float)(in->cursor % g.cols), (float)(in->cursor / g.cols), top));
	rail(r, &g, top, in->count);
	band(r, in, ctx, games, sh, phase, &wait);
	{
		key_hint foot[4];
		int nf = 0;

		foot[nf++] = (key_hint){ "B", T(in->inside ? S_GAMES_BACK : S_GAMES_SYSTEMS) };
		if (in->count > 0 && in->start_word) foot[nf++] = (key_hint){ "START", in->start_word };
		if (in->count > 0 && in->resumable && !in->install && !in->howto && !in->opens && !games)
			foot[nf++] = (key_hint){ "Y", T(S_GAMES_FRESH) };
		if (in->count > 0 && !games) foot[nf++] = (key_hint){ "X", T(S_GAMES_DETAILS) };
		keyhints_draw(r, in->foot ? in->foot : foot, in->foot ? in->nfoot : nf, GG_MARGIN,
		              ZYPEROS_SCREEN_W - GG_MARGIN, GG_FOOT(sh), KEYHINTS_SPREAD, true, 255);
	}
	SDL_SetRenderDrawColor(r, red, green, blue, alpha);
	SDL_SetRenderDrawBlendMode(r, blend);
	return wait;
}
