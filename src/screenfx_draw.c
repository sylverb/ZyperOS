/* SPDX-License-Identifier: GPL-3.0-only */
/* See screenfx_draw.h */
#include "screenfx_draw.h"

#include "platform.h"
#include "screenfx.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* What plat_screen_shader asked for */
static screenfx_kind g_kind;
static int g_strength = 100;

/* The mask's texture, and what it was made for. A renderer is known by its
 * pointer, as the bar's texture is (plat_osd_forget): sfx_forget is told
 * before one goes. */
static SDL_Texture *g_mask;
static SDL_Renderer *g_mask_for;
static screenfx_kind g_mask_kind;
static int g_mask_strength, g_mask_w, g_mask_h;

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
	if (g_mask) SDL_DestroyTexture(g_mask);
	if (g_copy) SDL_DestroyTexture(g_copy);
	g_mask = g_copy = NULL;
	g_mask_for = NULL;
}

/* The mask for what is asked, made when that or the size changed. Into a
 * texture of its own each time, the last one let go: the last frame may
 * still be drawing with it, and writing into it then waits for the GPU
 * (25 ms more a step of the list on the Pixel 2, 2026-11-18). */
static SDL_Texture *mask_for(SDL_Renderer *r, int w, int h)
{
	SDL_Texture *t;
	unsigned char *px;
	Uint64 t0, t1;

	if (g_mask && g_mask_for == r && g_mask_kind == g_kind && g_mask_strength == g_strength &&
	    g_mask_w == w && g_mask_h == h)
		return g_mask;
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
	if (g_mask && g_mask_for == r) SDL_DestroyTexture(g_mask);
	g_mask = t;
	g_mask_for = r;
	g_mask_w = w;
	g_mask_h = h;
	g_mask_kind = g_kind;
	g_mask_strength = g_strength;
	/* Once a shader, strength and size: what a list's step costs */
	fprintf(stderr, "screens' shader: kind %d at %d%%, %dx%d, made in %.1f ms, sent in %.1f ms\n",
	        (int)g_kind, g_strength, w, h,
	        (double)(t1 - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency(),
	        (double)(SDL_GetPerformanceCounter() - t1) * 1000.0 / (double)SDL_GetPerformanceFrequency());
	return g_mask;
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
