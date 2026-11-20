/* SPDX-License-Identifier: GPL-3.0-only */
/* See dmodepreview.h */
#include "dmodepreview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "dmodefit.h"
#include "ds/preview.h"
#include "platform.h"
#include "theme.h"
#include "ui.h"

void dmode_surface(int *w, int *h)
{
	const device_profile *p = plat_profile();
	int sw, sh;

	if (plat_resident_source_surface(&sw, &sh)) {
		*w = sw;
		*h = sh;
		return;
	}
	*w = p ? p->screen.panel_w : ZYPEROS_SCREEN_W;
	*h = p ? p->screen.panel_h : ZYPEROS_SCREEN_H;
	if (p && p->screen.turn % 180) {
		int t = *w;
		*w = *h;
		*h = t;
	}
	if (*w <= 0 || *h <= 0) {
		*w = ZYPEROS_SCREEN_W;
		*h = ZYPEROS_SCREEN_H;
	}
}

static void shape_key(const char *tag, char *out, size_t n)
{
	snprintf(out, n, "display_shape.%s", tag ? tag : "");
}

void dmode_note_shape(const char *tag)
{
	int w, h;
	double aspect;
	char key[64], v[32];

	if (!tag || !*tag || !plat_resident_source(&w, &h, &aspect)) return;
	shape_key(tag, key, sizeof key);
	snprintf(v, sizeof v, "%.5f", aspect);
	db_set_str(db_dev(), key, v);
	/* And its size, for the system menu's Border row (src/border.h): room
	 * around it or none, at each mode */
	snprintf(key, sizeof key, "display_source.%s", tag);
	snprintf(v, sizeof v, "%dx%d", w, h);
	db_set_str(db_dev(), key, v);
}

bool dmode_source_of(const char *tag, int *w, int *h)
{
	char key[64], v[32];

	*w = *h = 0;
	snprintf(key, sizeof key, "display_source.%s", tag ? tag : "");
	if (!db_get_str(db_dev(), key, v, sizeof v, "") || sscanf(v, "%dx%d", w, h) != 2 || *w <= 0 || *h <= 0) {
		*w = *h = 0;
		return false;
	}
	return true;
}

double dmode_shape_of(const char *tag, bool *known)
{
	char key[64], v[32];

	shape_key(tag, key, sizeof key);
	*known = db_get_str(db_dev(), key, v, sizeof v, "") && v[0];
	return *known ? atof(v) : 0.0;
}

/* The stand-in: a scene of flat bands and a sun, at a console's 320x240,
 * drawn once - something the eye reads as a picture, so the bars and crops
 * around it read as what they are */
static SDL_Texture *stand_in(SDL_Renderer *r)
{
	static SDL_Texture *t;
	static SDL_Renderer *made;
	SDL_Surface *s;
	int x, y;

	if (t && made == r) return t;
	s = SDL_CreateRGBSurfaceWithFormat(0, 320, 240, 32, SDL_PIXELFORMAT_ARGB8888);
	if (!s) return NULL;
	for (y = 0; y < 240; y++) {
		Uint32 *px = (Uint32 *)((Uint8 *)s->pixels + (size_t)y * s->pitch);

		for (x = 0; x < 320; x++) {
			const int dx = x - 230, dy = y - 70;
			Uint8 R, G, B;

			if (dx * dx + dy * dy < 26 * 26)      { R = 255; G = 216; B = 77;  }  /* sun */
			else if (y < 150)                     { R = (Uint8)(116 + y / 3); G = (Uint8)(176 + y / 4); B = 255; }
			else if (y < 160 + (x / 24 % 2) * 6)  { R = 94;  G = 224; B = 138; }  /* hills */
			else if (((x / 16) + (y / 16)) & 1)   { R = 154; G = 98;  B = 40;  }  /* ground, in blocks */
			else                                  { R = 176; G = 116; B = 54;  }
			px[x] = SDL_MapRGBA(s->format, R, G, B, 255);
		}
	}
	if (t) SDL_DestroyTexture(t);
	t = SDL_CreateTextureFromSurface(r, s);
	made = r;
	SDL_FreeSurface(s);
	return t;
}

/* A Game Boy game's stand-in at its 160x144, in four greens, and a border's
 * at the Super Game Boy's 256x224 around it: drawn once each. The border is
 * ours, a game's own kind of border (a pattern round a window), with the
 * 160x144 window at (48, 40) where the Super Game Boy puts the game. */
#define GB_COLS 160
#define GB_LINES 144
#define SGB_COLS 256
#define SGB_LINES 224
static const Uint8 GB_SHADE[4][3] = {
	{ 202, 220, 159 }, { 139, 172, 15 }, { 48, 98, 48 }, { 15, 56, 15 },
};

static SDL_Texture *gb_stand_in(SDL_Renderer *r, bool border)
{
	static SDL_Texture *t[2];
	static SDL_Renderer *made[2];
	const int sw = border ? SGB_COLS : GB_COLS, sh = border ? SGB_LINES : GB_LINES;
	SDL_Surface *s;
	int x, y;

	if (t[border] && made[border] == r) return t[border];
	s = SDL_CreateRGBSurfaceWithFormat(0, sw, sh, 32, SDL_PIXELFORMAT_ARGB8888);
	if (!s) return NULL;
	for (y = 0; y < sh; y++) {
		Uint32 *px = (Uint32 *)((Uint8 *)s->pixels + (size_t)y * s->pitch);

		for (x = 0; x < sw; x++) {
			Uint8 R, G, B;

			if (border) {
				/* Diamonds of two blues and a cream band round the
				 * window, as a game's own border is a picture of its own */
				const int dx = abs((x % 32) - 16), dy = abs((y % 32) - 16);
				const bool band = x >= 40 && x < 216 && y >= 32 && y < 192;

				if (band)               { R = 246; G = 226; B = 150; }
				else if (dx + dy < 12)  { R = 84;  G = 132; B = 204; }
				else                    { R = 52;  G = 92;  B = 166; }
			} else {
				const int dx = x - 116, dy = y - 40, sh_i =
					dx * dx + dy * dy < 16 * 16 ? 1      /* sun */
					: y < 96 ? 0                          /* sky */
					: y < 102 + (x / 20 % 2) * 4 ? 2      /* hills */
					: (((x / 16) + (y / 16)) & 1) ? 3 : 2;  /* ground */

				R = GB_SHADE[sh_i][0]; G = GB_SHADE[sh_i][1]; B = GB_SHADE[sh_i][2];
			}
			px[x] = SDL_MapRGBA(s->format, R, G, B, 255);
		}
	}
	if (t[border]) SDL_DestroyTexture(t[border]);
	t[border] = SDL_CreateTextureFromSurface(r, s);
	made[border] = r;
	SDL_FreeSurface(s);
	return t[border];
}

/* A rect of the surface in the box's units, edge to edge */
static SDL_Rect in_box(const SDL_Rect *box, int surf_w, int surf_h, int fx, int fy, int fw, int fh)
{
	SDL_Rect d;

	d.x = box->x + fx * box->w / surf_w;
	d.y = box->y + fy * box->h / surf_h;
	d.w = box->x + (fx + fw) * box->w / surf_w - d.x;
	d.h = box->y + (fy + fh) * box->h / surf_h - d.y;
	return d;
}

void dmode_sgb_preview(app *a, int dmode, int k, int x, int y, int w, int h)
{
	const SDL_Rect screen = { x, y, w, h };
	const bool sgb = k != 0;
	SDL_Texture *game = gb_stand_in(a->r, false);
	SDL_Rect box;
	dmode_rect fit;
	int surf_w, surf_h;

	dmode_surface(&surf_w, &surf_h);
	dmode = dmode >= 0 && dmode < DMODE_COUNT ? dmode : 0;
	box = ds_preview_screen_begin(a->r, &screen);
	fit = dmode_fit(DMODES[dmode].name, sgb ? SGB_COLS : GB_COLS, sgb ? SGB_LINES : GB_LINES, 0, surf_w, surf_h);
	if (fit.w > 0 && fit.h > 0) {
		const SDL_Rect pic = in_box(&box, surf_w, surf_h, fit.x, fit.y, fit.w, fit.h);

		SDL_RenderSetClipRect(a->r, &box);
		if (sgb) {
			/* The game's window, at the same scale as the frame */
			const SDL_Rect win = in_box(&box, surf_w, surf_h, fit.x + 48 * fit.w / SGB_COLS,
			                            fit.y + 40 * fit.h / SGB_LINES, GB_COLS * fit.w / SGB_COLS,
			                            GB_LINES * fit.h / SGB_LINES);

			SDL_Texture *bt = gb_stand_in(a->r, true);

			if (bt) SDL_RenderCopy(a->r, bt, NULL, &pic);
			if (game) SDL_RenderCopy(a->r, game, NULL, &win);
		} else if (game) {
			SDL_RenderCopy(a->r, game, NULL, &pic);
		}
		SDL_RenderSetClipRect(a->r, NULL);
	}
	ds_preview_screen_end(a->r, &screen);
}

/* The picture made small (BORDER_BLUR_W x BORDER_BLUR_H, as Diatom makes it),
 * in two steps so each takes an average of a few texels, into a target kept
 * for it: what is stretched back over the box, smooth and darkened */
static SDL_Texture *blur_of(SDL_Renderer *r, SDL_Texture *pic)
{
	static SDL_Texture *mid, *small;
	static SDL_Renderer *made;
	SDL_Texture *was = SDL_GetRenderTarget(r);

	if (made != r) {
		if (mid) SDL_DestroyTexture(mid);
		if (small) SDL_DestroyTexture(small);
		mid = SDL_CreateTexture(r, PLAT_TARGET_FORMAT, SDL_TEXTUREACCESS_TARGET, 128, 96);
		small = SDL_CreateTexture(r, PLAT_TARGET_FORMAT, SDL_TEXTUREACCESS_TARGET, 32, 24);
		if (mid) SDL_SetTextureScaleMode(mid, SDL_ScaleModeLinear);
		if (small) SDL_SetTextureScaleMode(small, SDL_ScaleModeLinear);
		made = r;
	}
	if (!mid || !small || !pic) return NULL;
	SDL_SetTextureScaleMode(pic, SDL_ScaleModeLinear);
	SDL_SetRenderTarget(r, mid);
	SDL_RenderCopy(r, pic, NULL, NULL);
	SDL_SetRenderTarget(r, small);
	SDL_RenderCopy(r, mid, NULL, NULL);
	SDL_SetRenderTarget(r, was);
	SDL_SetTextureScaleMode(pic, SDL_ScaleModeNearest);
	return small;
}

void dmode_border_preview(app *a, SDL_Texture *frame, int src_w, int src_h, double aspect,
                          int dmode, int kind, SDL_Surface *own, const char *own_key,
                          const SDL_Rect *game_at, int x, int y, int w, int h)
{
	const SDL_Rect screen = { x, y, w, h };
	SDL_Texture *pic = frame;
	SDL_Rect box;
	dmode_rect fit;
	int surf_w, surf_h;

	if (!pic) {
		/* The stand-in, at the size the system's games last said when it
		 * is known (a Game Boy's 160x144), else a console's */
		pic = stand_in(a->r);
		if (src_w <= 0 || src_h <= 0) {
			src_w = 320;
			src_h = 240;
			aspect = 4.0 / 3.0;
		}
	}
	if (src_w <= 0 || src_h <= 0) SDL_QueryTexture(pic, NULL, NULL, &src_w, &src_h);
	dmode_surface(&surf_w, &surf_h);
	dmode = dmode >= 0 && dmode < DMODE_COUNT ? dmode : 0;
	box = ds_preview_screen_begin(a->r, &screen);
	SDL_RenderSetClipRect(a->r, &box);
	if (kind == 1 && pic) {
		/* Blur: Diatom's, darkened to 0.55 */
		SDL_Texture *b = blur_of(a->r, pic);

		if (b) {
			SDL_SetTextureColorMod(b, 140, 140, 140);
			SDL_RenderCopy(a->r, b, NULL, &box);
			SDL_SetTextureColorMod(b, 255, 255, 255);
		}
	} else if (kind >= 2 && own) {
		/* Its texture made once for the picture it is from */
		static SDL_Texture *t;
		static char of[512];
		static SDL_Renderer *in;

		if (!t || strcmp(of, own_key ? own_key : "") || in != a->r) {
			if (t) SDL_DestroyTexture(t);
			t = SDL_CreateTextureFromSurface(a->r, own);
			if (t) SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
			snprintf(of, sizeof of, "%s", own_key ? own_key : "");
			in = a->r;
		}
		if (t) SDL_RenderCopy(a->r, t, NULL, &box);
	}
	fit = dmode_fit(DMODES[dmode].name, src_w, src_h, aspect, surf_w, surf_h);
	if (game_at) fit = (dmode_rect){ game_at->x, game_at->y, game_at->w, game_at->h };
	if (pic && fit.w > 0 && fit.h > 0) {
		SDL_Rect dst;

		dst.x = box.x + fit.x * box.w / surf_w;
		dst.y = box.y + fit.y * box.h / surf_h;
		dst.w = box.x + (fit.x + fit.w) * box.w / surf_w - dst.x;
		dst.h = box.y + (fit.y + fit.h) * box.h / surf_h - dst.y;
		SDL_RenderCopy(a->r, pic, NULL, &dst);
	}
	SDL_RenderSetClipRect(a->r, NULL);
	ds_preview_screen_end(a->r, &screen);
}

void dmode_preview_draw(app *a, SDL_Texture *frame, int src_w, int src_h, double aspect,
                        int surf_w, int surf_h, int k, int x, int y, int w, int h)
{
	const SDL_Rect screen = { x, y, w, h };
	SDL_Texture *pic = frame;
	SDL_Rect box;
	dmode_rect fit;

	if (!pic) {
		pic = stand_in(a->r);
		src_w = 320;
		src_h = 240;
		aspect = 4.0 / 3.0;
	}
	if (src_w <= 0 || src_h <= 0) SDL_QueryTexture(pic, NULL, NULL, &src_w, &src_h);
	if (surf_w <= 0 || surf_h <= 0) dmode_surface(&surf_w, &surf_h);
	k = k >= 0 && k < DMODE_COUNT ? k : 0;

	/* The screen, framed (src/ds/preview.h): ink, as Diatom leaves what the
	 * picture does not cover */
	box = ds_preview_screen_begin(a->r, &screen);
	x = box.x; y = box.y; w = box.w; h = box.h;
	fit = dmode_fit(DMODES[k].name, src_w, src_h, aspect, surf_w, surf_h);
	if (pic && fit.w > 0 && fit.h > 0) {
		/* From the surface's pixels to the box's units, edge to edge, so a
		 * picture that fills the surface fills the box exactly */
		SDL_Rect dst;

		dst.x = x + fit.x * w / surf_w;
		dst.y = y + fit.y * h / surf_h;
		dst.w = x + (fit.x + fit.w) * w / surf_w - dst.x;
		dst.h = y + (fit.y + fit.h) * h / surf_h - dst.y;
		SDL_RenderSetClipRect(a->r, &box);
		SDL_RenderCopy(a->r, pic, NULL, &dst);
		SDL_RenderSetClipRect(a->r, NULL);
	}
	ds_preview_screen_end(a->r, &screen);
}
