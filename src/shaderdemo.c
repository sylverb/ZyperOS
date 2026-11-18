/* SPDX-License-Identifier: GPL-3.0-only */
/* See shaderdemo.h */
#include "shaderdemo.h"

#include <SDL_image.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "dmodepreview.h"
#include "ds/preview.h"
#include "i18n.h"
#include "platform.h"
#include "strength.h"

int shader_at(int k, const char *tag)
{
	int i, n = 0;

	for (i = 0; i < SHADER_COUNT; i++) {
		if (SHADERS[i].gpu && !plat_gpu_shaders()) continue;
		if (!shader_for(i, tag)) continue;
		if (n++ == k) return i;
	}
	return 0;
}

/* Each shader's note, in SHADERS' order */
static const str_id NOTE[SHADER_COUNT] = {
	S_GAMEMENU_SHADER_OFF_NOTE, S_GAMEMENU_SHADER_SHIMMERLESS_NOTE, S_GAMEMENU_SHADER_CRT_NOTE,
	S_GAMEMENU_SHADER_CRT_CURVED_NOTE, S_GAMEMENU_SHADER_SCANLINES_NOTE, S_GAMEMENU_SHADER_LCD_NOTE,
	S_GAMEMENU_SHADER_LCD_UNLIT_NOTE,
};

void shader_demo_notes(const char **out, int n, const char *tag)
{
	for (int k = 0; k < n; k++) out[k] = T(NOTE[shader_at(k, tag)]);
}

void shader_demo_strength_notes(const char **out, int steps)
{
	for (int k = 0; k < steps; k++) out[k] = T(S_GAMEMENU_STRENGTH_NOTE);
}

/* The last few pictures shown, so moving the cursor back and forth reads
 * each PNG once */
#define DEMO_KEEP 4
static struct { char path[256]; SDL_Texture *t; SDL_Renderer *r; unsigned used; } g_keep[DEMO_KEEP];
static unsigned g_clock;

static SDL_Texture *demo_tex(SDL_Renderer *r, const char *path)
{
	int i, oldest = 0;
	SDL_Surface *s;

	for (i = 0; i < DEMO_KEEP; i++) {
		if (g_keep[i].t && g_keep[i].r == r && !strcmp(g_keep[i].path, path)) {
			g_keep[i].used = ++g_clock;
			return g_keep[i].t;
		}
		if (g_keep[i].used < g_keep[oldest].used) oldest = i;
	}
	if (g_keep[oldest].t) SDL_DestroyTexture(g_keep[oldest].t);
	memset(&g_keep[oldest], 0, sizeof g_keep[oldest]);
	if (!(s = IMG_Load(path))) return NULL;
	g_keep[oldest].t = SDL_CreateTextureFromSurface(r, s);
	SDL_FreeSurface(s);
	if (!g_keep[oldest].t) return NULL;
	/* Its pixels as they are where the screen draws it larger (a host's
	 * shot), as the panel shows them one for one */
	SDL_SetTextureScaleMode(g_keep[oldest].t, SDL_ScaleModeNearest);
	snprintf(g_keep[oldest].path, sizeof g_keep[oldest].path, "%s", path);
	g_keep[oldest].r = r;
	g_keep[oldest].used = ++g_clock;
	return g_keep[oldest].t;
}

void shader_demo_draw(app *a, int shader, int strength, int x, int y, int w, int h)
{
	const SDL_Rect screen = { x, y, w, h };
	SDL_Rect in = ds_preview_screen_begin(a->r, &screen);
	char path[256];
	int sw, sh, tw = 0, th = 0;
	SDL_Texture *t;

	if (shader < 0 || shader >= SHADER_COUNT) shader = 0;
	strength = strength_clamp(strength);
	/* The panel's pictures: the handheld's own pixels, one a filter and an
	 * old screen's Strength step (100% is the filter's name alone) */
	dmode_surface(&sw, &sh);
	if (shader_has_strength(shader) && strength < 100)
		snprintf(path, sizeof path, "%s/res/shaderdemo/%dx%d/%s-%d.png", P_ROOT, sw, sh, SHADERS[shader].name,
		         strength);
	else
		snprintf(path, sizeof path, "%s/res/shaderdemo/%dx%d/%s.png", P_ROOT, sw, sh, SHADERS[shader].name);
	if ((t = demo_tex(a->r, path)) && SDL_QueryTexture(t, NULL, NULL, &tw, &th) == 0) {
		/* One picture pixel to one of the panel's: the close-up's top left,
		 * as much of it as the screen's inside holds */
		const float k = (float)sh / ZYPEROS_SCREEN_H;
		SDL_Rect src = { 0, 0, (int)lroundf((float)in.w * k), (int)lroundf((float)in.h * k) };
		SDL_Rect dst;

		if (src.w > tw) src.w = tw;
		if (src.h > th) src.h = th;
		dst = (SDL_Rect){ in.x, in.y, (int)lroundf((float)src.w / k), (int)lroundf((float)src.h / k) };
		SDL_RenderCopy(a->r, t, &src, &dst);
	}
	ds_preview_screen_end(a->r, &screen);
}
