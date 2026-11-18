/* SPDX-License-Identifier: GPL-3.0-only */
/* make check-screenfx: the launcher's screens' shader (src/screenfx.h).
 *
 * - Held to Diatom: at a game's size (a 256x224 picture on the Pixel 2's
 *   640x480, phase 0), each old screen's mask is, within one level of 255,
 *   what tools/shaderdemo.c - Diatom's GLSL in C, line for line - makes of a
 *   white picture, at 100%, 50% and 10%, every pixel.
 * - What a person sees on the Pixel 2 (320x240 over 640x480): Scanlines dim
 *   every other row and no column; the LCD's grid dims a row and a column of
 *   every two; the CRT's lines show (its bright rows lighter than its dark
 *   ones) with its grille in thirds; CRT Curved is black in its corners and
 *   the CRT in its middle; 10% is close to the plain picture; the curve
 *   holds the middle still and sends the corners past the picture.
 * - The time a mask takes on this machine, for the notes.
 *
 * tools/shaderdemo.c comes in whole, its main() renamed, for its shade() and
 * its scene. */
#define main shaderdemo_main
#include "shaderdemo.c"
#undef main

#include "../src/screenfx.h"
#include "../src/screenfx_draw.h"

/* What src/screenfx_draw.c asks of the device: the old screens are drawn */
bool plat_gpu_shaders(void) { return true; }
void plat_screen_shader(const char *name, int strength);

#include <time.h>

static int fails;

static void ok(const char *what, int cond)
{
	printf("  %s %s\n", cond ? "ok  " : "FAIL", what);
	fails += !cond;
}

static double now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static unsigned char *mask(screenfx_kind k, int strength, int w, int h)
{
	unsigned char *m = calloc((size_t)w * h, 4);

	if (m) screenfx_mask(k, strength, w, h, m);
	return m;
}

#define AT(m, w, x, y, c) ((m)[((size_t)(y) * (w) + (x)) * 4 + (c)])

/* Every pixel of kind k's mask at 256x224 over 640x480 against shaderdemo's
 * shade() of a white scene: the most any channel is apart, in levels */
static int against_diatom(screenfx_kind k, filt f, int strength)
{
	const int W = 640, H = 480;
	const v2 scale = { (float)W / SW, (float)H / SH };
	unsigned char *m = calloc((size_t)W * H, 4);
	int x, y, worst = 0;

	if (!m) return 255;
	screenfx_mask_grid(k, strength, W, H, (float)SW, (float)SH, 0.0f, m);
	for (y = 0; y < H; y++)
		for (x = 0; x < W; x++) {
			v3 c = shade(f, x, y, scale, (float)strength / 100.0f);
			float want[3] = { c.r, c.g, c.b };

			for (int ch = 0; ch < 3; ch++) {
				int d = abs((int)AT(m, W, x, y, ch) - (int)(clampf(want[ch], 0, 1) * 255.0f + 0.5f));

				if (d > worst) worst = d;
			}
		}
	free(m);
	return worst;
}

int main(void)
{
	static const struct { screenfx_kind k; filt f; const char *name; } KINDS[] = {
		{ SFX_CRT, F_CRT, "CRT" },
		{ SFX_CRT_CURVED, F_CRT_CURVED, "CRT Curved" },
		{ SFX_SCANLINES, F_SCANLINES, "Scanlines" },
		{ SFX_LCD, F_LCD, "LCD Grid" },
	};
	static const int STRENGTHS[] = { 100, 50, 10 };
	const int W = 640, H = 480;
	char what[160];
	unsigned char *m;
	size_t i, j;

	/* A white scene: shade() reads `scene` */
	memset(scene, 255, sizeof scene);

	printf("Diatom's arithmetic, a white 256x224 picture on 640x480\n");
	for (i = 0; i < sizeof KINDS / sizeof KINDS[0]; i++)
		for (j = 0; j < sizeof STRENGTHS / sizeof STRENGTHS[0]; j++) {
			int worst = against_diatom(KINDS[i].k, KINDS[i].f, STRENGTHS[j]);

			snprintf(what, sizeof what, "%s at %d%%: within one level of shaderdemo's everywhere (worst %d)",
			         KINDS[i].name, STRENGTHS[j], worst);
			ok(what, worst <= 1);
		}

	printf("names\n");
	ok("crt, crt-curved, scanlines and lcd are the four old screens",
	   screenfx_kind_of("crt") == SFX_CRT && screenfx_kind_of("crt-curved") == SFX_CRT_CURVED &&
	   screenfx_kind_of("scanlines") == SFX_SCANLINES && screenfx_kind_of("lcd") == SFX_LCD);
	ok("nearest, shimmerless, nothing and NULL are Off",
	   screenfx_kind_of("nearest") == SFX_OFF && screenfx_kind_of("shimmerless") == SFX_OFF &&
	   screenfx_kind_of("") == SFX_OFF && screenfx_kind_of(NULL) == SFX_OFF);
	ok("only CRT Curved bends the picture",
	   screenfx_curved(SFX_CRT_CURVED) && !screenfx_curved(SFX_CRT) && !screenfx_curved(SFX_SCANLINES) &&
	   !screenfx_curved(SFX_LCD) && !screenfx_curved(SFX_OFF));

	printf("on the Pixel 2: 320x240 over 640x480\n");
	if ((m = mask(SFX_SCANLINES, 100, W, H))) {
		int even = 1, cols = 1;

		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++) {
				if (AT(m, W, x, y, 0) != AT(m, W, 0, y, 0) || AT(m, W, x, y, 1) != AT(m, W, x, y, 0)) cols = 0;
				if (AT(m, W, x, y, 0) != (y % 2 ? AT(m, W, 0, 1, 0) : 255)) even = 0;
			}
		snprintf(what, sizeof what, "Scanlines: even rows as they are, odd rows dimmed (%d), every column alike",
		         AT(m, W, 0, 1, 0));
		ok(what, even && cols && AT(m, W, 0, 1, 0) < 230 && AT(m, W, 0, 1, 0) > 180);
		free(m);
	}
	if ((m = mask(SFX_LCD, 100, W, H))) {
		snprintf(what, sizeof what, "LCD Grid: a lit pixel, a dimmed row and column, a darker corner (%d %d %d %d)",
		         AT(m, W, 0, 0, 0), AT(m, W, 1, 0, 0), AT(m, W, 0, 1, 0), AT(m, W, 1, 1, 0));
		ok(what, AT(m, W, 0, 0, 0) == 255 && AT(m, W, 1, 0, 0) == AT(m, W, 0, 1, 0) &&
		         AT(m, W, 1, 0, 0) < 230 && AT(m, W, 1, 1, 0) < AT(m, W, 1, 0, 0) &&
		         AT(m, W, 2, 2, 0) == 255);
		free(m);
	}
	if ((m = mask(SFX_CRT, 100, W, H))) {
		int bright = AT(m, W, 1, 100, 1), dark = AT(m, W, 1, 101, 1), triad = 1;

		for (int x = 0; x < W && triad; x++)
			for (int c = 0; c < 3; c++)
				if ((AT(m, W, x, 100, c) < AT(m, W, x, 100, (c + 1) % 3)) != (x % 3 == c)) triad = 0;
		snprintf(what, sizeof what, "CRT: a line's middle row brighter than the row between lines (%d, %d)",
		         bright, dark);
		ok(what, bright > dark + 40);
		ok("CRT: one color of each three columns dimmed, red, green, blue in turn", triad);
		free(m);
	}
	if ((m = mask(SFX_CRT_CURVED, 100, W, H))) {
		unsigned char *flat = mask(SFX_CRT, 100, W, H);

		ok("CRT Curved: black in each corner",
		   !AT(m, W, 0, 0, 1) && !AT(m, W, W - 1, 0, 1) && !AT(m, W, 0, H - 1, 1) && !AT(m, W, W - 1, H - 1, 1));
		ok("CRT Curved: the CRT in its middle", flat && !memcmp(&AT(m, W, W / 2 - 3, H / 2, 0),
		                                                        &AT(flat, W, W / 2 - 3, H / 2, 0), 4 * 6));
		free(flat);
		free(m);
	}
	if ((m = mask(SFX_SCANLINES, 10, W, H))) {
		ok("10%: the band a faint hint", AT(m, W, 0, 1, 0) > 245);
		free(m);
	}
	if ((m = mask(SFX_CRT, 10, W, H))) {
		ok("10%: the CRT close to the plain picture", AT(m, W, 1, 101, 1) > 225 && AT(m, W, 1, 100, 1) > 225);
		free(m);
	}
	{
		float u, v, u2, v2;

		screenfx_warp(0.5f, 0.5f, &u, &v);
		screenfx_warp(0.0f, 0.0f, &u2, &v2);
		ok("the curve holds the middle still", fabsf(u - 0.5f) < 1e-6f && fabsf(v - 0.5f) < 1e-6f);
		ok("the curve sends a corner past the picture", u2 < 0.0f && v2 < 0.0f);
	}

	printf("masks kept (src/screenfx_draw.c), SDL's software renderer\n");
	{
		SDL_Surface *sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGBA32);
		SDL_Renderer *r = sf ? SDL_CreateSoftwareRenderer(sf) : NULL;
		SDL_Rect all = { 0, 0, W, H };
		static const char *const NAMES[] = { "crt", "crt-curved", "scanlines", "lcd" };
		unsigned before;
		int pass, n;

		if (!r) {
			ok("a software renderer to draw with", 0);
		} else {
			before = sfx_masks_made();
			for (pass = 0; pass < 3; pass++)
				for (n = 0; n < 4; n++) {
					plat_screen_shader(NAMES[n], 100);
					sfx_draw_mask(r, W, H, &all, 0.0);
				}
			snprintf(what, sizeof what, "the four old screens, three times round: each made once (%u made)",
			         sfx_masks_made() - before);
			ok(what, sfx_masks_made() - before == 4);
			before = sfx_masks_made();
			for (n = 10; n <= 40; n += 10) {
				plat_screen_shader("crt-curved", n);
				sfx_draw_mask(r, W, H, &all, 0.0);
			}
			/* Eight asked: the two used longest ago (CRT and CRT Curved
			 * at 100%) gave way; Scanlines and the LCD are still kept */
			plat_screen_shader("lcd", 100);
			sfx_draw_mask(r, W, H, &all, 0.0);
			plat_screen_shader("scanlines", 100);
			sfx_draw_mask(r, W, H, &all, 0.0);
			ok("six kept: four strengths more made, the two newest of the first still kept",
			   sfx_masks_made() - before == 4);
			plat_screen_shader("crt", 100);
			sfx_draw_mask(r, W, H, &all, 0.0);
			ok("the one used longest ago was let go, and is made again", sfx_masks_made() - before == 5);
			plat_screen_shader("nearest", 100);
			sfx_draw_mask(r, W, H, &all, 0.0);
			ok("Off makes nothing", sfx_masks_made() - before == 5);
			sfx_forget();
			SDL_DestroyRenderer(r);
		}
		if (sf) SDL_FreeSurface(sf);
	}

	printf("time to make a mask on this machine\n");
	for (i = 0; i < sizeof KINDS / sizeof KINDS[0]; i++) {
		double t0 = now_ms();

		if ((m = mask(KINDS[i].k, 100, W, H))) free(m);
		printf("  %-10s 640x480 %.1f ms\n", KINDS[i].name, now_ms() - t0);
	}
	printf(fails ? "screenfx: %d FAILED\n" : "screenfx: all passed\n", fails);
	return fails != 0;
}
