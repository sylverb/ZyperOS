/* SPDX-License-Identifier: GPL-3.0-only */
/* See screenfx_draw.h */
#include "screenfx_draw.h"

#include "platform.h"
#include "screenfx.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* What plat_screen_shader asked for */
static screenfx_kind g_kind;
static int g_strength = 100;

/* The masks made, kept: a list's cursor going back and forth over the
 * shaders pays for each one once, not at every step (CRT Curved's takes
 * about 65 ms on the Pixel 2). MASKS_KEPT of them, the one used longest
 * ago let go for a new one: 1.2 MB each at the Pixel 2's 640x480, so 7 MB
 * at most. A renderer is known by its pointer, as the bar's texture is
 * (plat_osd_forget): sfx_forget is told before one goes. */
#define MASKS_KEPT 6
static struct {
	SDL_Texture *tex;
	SDL_Renderer *for_r;
	screenfx_kind kind;
	int strength, w, h;
	unsigned used;          /* when it was last asked for, in asks */
} g_masks[MASKS_KEPT];
static unsigned g_asks, g_made;

unsigned sfx_masks_made(void) { return g_made; }

/* sfx_bend_target's copy of the frame */
static SDL_Texture *g_copy;
static int g_copy_w, g_copy_h;

void plat_screen_shader(const char *name, int strength)
{
	screenfx_kind k = screenfx_kind_of(name);

	/* The old screens only where the device draws them (as its Shader
	 * rows step over them elsewhere) */
	g_kind = plat_gpu_shaders() ? k : SFX_OFF;
	g_strength = strength < 10 ? 10 : strength > 100 ? 100 : strength;
}

bool sfx_on(void) { return g_kind != SFX_OFF; }
bool sfx_curved(void) { return screenfx_curved(g_kind); }

void sfx_forget(void)
{
	int i;

	for (i = 0; i < MASKS_KEPT; i++)
		if (g_masks[i].tex) SDL_DestroyTexture(g_masks[i].tex);
	memset(g_masks, 0, sizeof g_masks);
	if (g_copy) SDL_DestroyTexture(g_copy);
	g_copy = NULL;
}

/* The mask for what is asked: a kept one, or made now into a texture of its
 * own (never into one a frame may still be drawing with, which waits for
 * the GPU: 25 ms more a step of the list on the Pixel 2, 2026-11-18) */
static SDL_Texture *mask_for(SDL_Renderer *r, int w, int h)
{
	SDL_Texture *t;
	unsigned char *px;
	Uint64 t0, t1;
	int i, slot = 0;

	g_asks++;
	for (i = 0; i < MASKS_KEPT; i++)
		if (g_masks[i].tex && g_masks[i].for_r == r && g_masks[i].kind == g_kind &&
		    g_masks[i].strength == g_strength && g_masks[i].w == w && g_masks[i].h == h) {
			g_masks[i].used = g_asks;
			return g_masks[i].tex;
		}
	/* An empty place, else the one used longest ago */
	for (i = 0; i < MASKS_KEPT; i++) {
		if (!g_masks[i].tex) { slot = i; break; }
		if (g_masks[i].used < g_masks[slot].used) slot = i;
	}
	if (!(px = malloc((size_t)w * (size_t)h * 4))) return NULL;
	t0 = SDL_GetPerformanceCounter();
	screenfx_mask(g_kind, g_strength, w, h, px);
	t1 = SDL_GetPerformanceCounter();
	t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
	if (t) {
		/* A multiply: the frame times the mask, its alpha left alone */
		SDL_SetTextureBlendMode(t, SDL_BLENDMODE_MOD);
		/* Pixel for pixel: it is the panel's size where it is drawn */
		SDL_SetTextureScaleMode(t, SDL_ScaleModeNearest);
		SDL_UpdateTexture(t, NULL, px, w * 4);
	}
	free(px);
	if (!t) return NULL;
	if (g_masks[slot].tex) SDL_DestroyTexture(g_masks[slot].tex);
	g_masks[slot].tex = t;
	g_masks[slot].for_r = r;
	g_masks[slot].kind = g_kind;
	g_masks[slot].strength = g_strength;
	g_masks[slot].w = w;
	g_masks[slot].h = h;
	g_masks[slot].used = g_asks;
	g_made++;
	/* Once a mask is made: what the first step onto it costs */
	fprintf(stderr, "screens' shader: kind %d at %d%%, %dx%d, made in %.1f ms, sent in %.1f ms\n",
	        (int)g_kind, g_strength, w, h,
	        (double)(t1 - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency(),
	        (double)(SDL_GetPerformanceCounter() - t1) * 1000.0 / (double)SDL_GetPerformanceFrequency());
	return t;
}

void sfx_draw_mask(SDL_Renderer *r, int w, int h, const SDL_Rect *dst, double angle)
{
	SDL_Texture *t;

	if (!sfx_on() || !(t = mask_for(r, w, h))) return;
	SDL_RenderCopyEx(r, t, NULL, dst, angle, NULL, SDL_FLIP_NONE);
}

/* The bend's mesh: GX x GY cells, each a straight-sided quad of the curve.
 * The curve moves a point by at most about 2% of the picture, smoothly: at
 * 16 x 12 a cell is 40 pixels of the Pixel 2's panel, and the straight
 * sides are within a tenth of a pixel of it. */
#define GX 16
#define GY 12

void sfx_draw_curved(SDL_Renderer *r, SDL_Texture *src, const SDL_Rect *dst, double angle)
{
	static SDL_Vertex v[(GX + 1) * (GY + 1)];
	static int idx[GX * GY * 6];
	const double a = angle * M_PI / 180.0, ca = cos(a), sa = sin(a);
	const float cx = dst->x + dst->w / 2.0f, cy = dst->y + dst->h / 2.0f;
	int i, j, n = 0;

	for (j = 0; j <= GY; j++)
		for (i = 0; i <= GX; i++) {
			float x = (float)i / GX, y = (float)j / GY, u, w, dx, dy;
			SDL_Vertex *p = &v[j * (GX + 1) + i];

			screenfx_warp(x, y, &u, &w);
			/* A point of the edge the curve shows past the picture: moved
			 * in toward the middle onto the picture's edge. SDL 2's
			 * SDL_RenderGeometry refuses a texture point outside 0..1
			 * ("Values of 'uv' out of bounds") and draws nothing; past
			 * it is the mask's black, and the clear's. */
			if (u < 0.0f || u > 1.0f || w < 0.0f || w > 1.0f) {
				float in = 0.0f, out = 1.0f, t;
				int k;

				for (k = 0; k < 24; k++) {
					t = (in + out) / 2.0f;
					screenfx_warp(0.5f + (x - 0.5f) * t, 0.5f + (y - 0.5f) * t, &u, &w);
					if (u < 0.0f || u > 1.0f || w < 0.0f || w > 1.0f) out = t;
					else in = t;
				}
				x = 0.5f + (x - 0.5f) * in;
				y = 0.5f + (y - 0.5f) * in;
				screenfx_warp(x, y, &u, &w);
				u = u < 0.0f ? 0.0f : u > 1.0f ? 1.0f : u;
				w = w < 0.0f ? 0.0f : w > 1.0f ? 1.0f : w;
			}
			dx = x * dst->w - dst->w / 2.0f;
			dy = y * dst->h - dst->h / 2.0f;
			/* Turned as SDL_RenderCopyEx turns: clockwise on the screen */
			p->position.x = cx + (float)(dx * ca - dy * sa);
			p->position.y = cy + (float)(dx * sa + dy * ca);
			p->color = (SDL_Color){ 255, 255, 255, 255 };
			p->tex_coord.x = u;
			p->tex_coord.y = w;
		}
	for (j = 0; j < GY; j++)
		for (i = 0; i < GX; i++) {
			int k = j * (GX + 1) + i;

			idx[n++] = k;
			idx[n++] = k + 1;
			idx[n++] = k + GX + 1;
			idx[n++] = k + 1;
			idx[n++] = k + GX + 2;
			idx[n++] = k + GX + 1;
		}
	if (SDL_RenderGeometry(r, src, v, (GX + 1) * (GY + 1), idx, n) != 0) {
		static bool said;

		if (!said) fprintf(stderr, "screens' shader: the curve's mesh: %s\n", SDL_GetError());
		said = true;
	}
}

bool sfx_bend_target(SDL_Renderer *r, int w, int h)
{
	void *px;
	bool ok = false;

	if (g_copy && (g_copy_w != w || g_copy_h != h)) {
		SDL_DestroyTexture(g_copy);
		g_copy = NULL;
	}
	if (!g_copy) {
		g_copy = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
		if (!g_copy) return false;
		SDL_SetTextureBlendMode(g_copy, SDL_BLENDMODE_NONE);
		g_copy_w = w;
		g_copy_h = h;
	}
	if ((px = malloc((size_t)w * (size_t)h * 4)) &&
	    SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_RGBA32, px, w * 4) == 0 &&
	    SDL_UpdateTexture(g_copy, NULL, px, w * 4) == 0) {
		SDL_Rect all = { 0, 0, w, h };
		Uint8 cr, cg, cb, ca;

		SDL_GetRenderDrawColor(r, &cr, &cg, &cb, &ca);
		SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
		SDL_RenderClear(r);
		SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
		sfx_draw_curved(r, g_copy, &all, 0.0);
		ok = true;
	}
	free(px);
	return ok;
}
