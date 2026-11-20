/* SPDX-License-Identifier: GPL-3.0-only */
/* check-shaderpic: the Shader and Strength lists' pictures (src/shaderdemo.h)
 * as textures. Each one, every filter and Strength step on each panel, loads
 * as a texture WITH AN ALPHA CHANNEL, opaque. The pictures are RGB PNGs, and
 * an RGB888 texture drawn into the screen's target by the Pixel 2's SDL (2.32,
 * GLES2) leaves alpha 0 there, which the panel's copy of the screen blends
 * over black: the list's picture was black on the device and right on every
 * computer, whose SDL writes 255 (2026-11-19, on the 1.1.0 test build; found
 * with the device's own SDL in Docker, as tools/yuvtarget-check.sh runs it).
 * The host cannot see that alpha, so this holds the format the device needs.
 * A picture that is not there gives NULL, and is not read again each frame.
 * On the launcher's own objects; run from the repository's root. */
#include "../src/app.h"
#include "../src/platform.h"
#include "../src/shaderdemo.h"
#include "../src/strength.h"

#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>

static int failures;

#define CK(cond, ...)                                                          \
	do {                                                                       \
		if (!(cond)) {                                                         \
			printf("  FAIL: ");                                                \
			printf(__VA_ARGS__);                                               \
			printf("\n");                                                      \
			failures++;                                                        \
		}                                                                      \
	} while (0)

int main(void)
{
	static const int panel[2][2] = { { 640, 480 }, { 1024, 768 } };
	SDL_Surface *win;
	SDL_Renderer *r;
	int n = 0;

	printf("shaderpic: the Shader lists' pictures as textures\n");
	SDL_Init(SDL_INIT_VIDEO);
	IMG_Init(IMG_INIT_PNG);
	win = SDL_CreateRGBSurfaceWithFormat(0, 1024, 768, 32, SDL_PIXELFORMAT_ARGB8888);
	r = win ? SDL_CreateSoftwareRenderer(win) : NULL;
	if (!r) { printf("  FAIL: no renderer: %s\n", SDL_GetError()); return 1; }
	P_ROOT = ".";
	for (int p = 0; p < 2; p++)
		for (int k = 0; k < SHADER_COUNT; k++)
			for (int st = STRENGTH_MIN; st <= 100; st += STRENGTH_STEP) {
				SDL_Texture *t = shader_demo_texture(r, k, st, panel[p][0], panel[p][1]);
				Uint32 f = 0;

				if (!shader_has_strength(k) && st < 100) continue;
				CK(t != NULL, "%dx%d %s %d%%: no picture", panel[p][0], panel[p][1], SHADERS[k].name, st);
				if (!t) continue;
				SDL_QueryTexture(t, &f, NULL, NULL, NULL);
				CK(SDL_ISPIXELFORMAT_ALPHA(f), "%dx%d %s %d%%: a %s texture, with no alpha: black on the Pixel 2",
				   panel[p][0], panel[p][1], SHADERS[k].name, st, SDL_GetPixelFormatName(f));
				n++;
			}
	CK(n >= 2 * (SHADER_COUNT - 2), "only %d pictures looked at", n);
	/* Not on the card: NULL, said once, and not read again */
	P_ROOT = "/nonexistent-zyperos-root";
	CK(shader_demo_texture(r, 2, 100, 640, 480) == NULL, "a picture that is not there gave a texture");
	CK(shader_demo_texture(r, 2, 100, 640, 480) == NULL, "a missing picture gave a texture the second time");
	SDL_DestroyRenderer(r);
	SDL_FreeSurface(win);
	printf(failures ? "\n%d check(s) failed\n" : "ok: %d pictures, every one a texture with alpha\n",
	       failures ? failures : n);
	return failures ? 1 : 0;
}
