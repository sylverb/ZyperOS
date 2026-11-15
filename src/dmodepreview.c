/* SPDX-License-Identifier: GPL-3.0-only */
/* See dmodepreview.h */
#include "dmodepreview.h"

#include <stdio.h>
#include <stdlib.h>

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
