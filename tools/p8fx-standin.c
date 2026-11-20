/* SPDX-License-Identifier: GPL-3.0-only */
/* A stand-in for pico8_64's drawing, for make check-p8fx-gl
 * (tools/p8fx-gl-check.py): the same SDL calls in the same order, with a
 * picture of its own (PICO-8's sixteen colours, no cart's art), so the shim
 * (src/p8fx/shim.c) can be run over it on a computer and what it draws
 * read back.
 *
 * pico8_64 0.2.7's codo_set_screen_plat and codo_blit_to_video_plat, as its
 * imports and their calls show: SDL_CreateWindow, SDL_CreateRenderer (-1,
 * PRESENTVSYNC), SDL_RENDER_SCALE_QUALITY "nearest" at -pixel_perfect 1
 * (else linear), SDL_SetRenderDrawColor black, a clear and a present, the
 * screen's texture (ARGB8888, streaming, 128 times its preblit scale) and
 * the FPS line's (128x8). Each frame: SDL_UpdateTexture of the whole
 * picture, SDL_RenderClear, SDL_RenderCopy to the display mode's rect, the
 * FPS line's copy when it is on, SDL_RenderPresent.
 *
 *   p8fx-standin [--sharp] [--preblit K] [--rect X,Y,W,H] [--fps]
 *                [--frames N] [--scene SCENE.ppm] [--then LOOK] OUT.ppm
 *
 * --sharp is Sharp Pixels (pixel_perfect 1: 3x, 384x384 in the middle of
 * 640x480, preblit 1); otherwise 480x480 in the middle, preblit 3, linear.
 * OUT.ppm is the last frame as the renderer has it before the present
 * (SDL_RenderReadPixels), SCENE.ppm the 128x128 picture itself. --then
 * moves the file LOOK over the shim's look file ($ZYPEROS_P8FX) after the
 * first frame, as the in-game menu writes it while PICO-8 runs, and waits
 * long enough for the shim to look again. */
#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define W 640
#define H 480
#define S 128

/* PICO-8's sixteen colours */
static const uint32_t PAL[16] = {
	0x000000, 0x1d2b53, 0x7e2553, 0x008751, 0xab5236, 0x5f574f, 0xc2c3c7, 0xfff1e8,
	0xff004d, 0xffa300, 0xffec27, 0x00e436, 0x29adff, 0x83769c, 0xff77a8, 0xffccaa,
};

static uint32_t screen[S * S];

/* Bands of each colour, a checker of single pixels, a box with a one-pixel
 * outline and a block that moves with the frame, so a shader's lines and a
 * blur's colours have edges to show at */
static void draw(int frame)
{
	int x, y;

	for (y = 0; y < S; y++)
		for (x = 0; x < S; x++) {
			int c = y < 32 ? (x / 8) % 16 : y < 64 ? ((x + y) & 1 ? 7 : 1) : y < 96 ? 12 : 3;

			screen[y * S + x] = 0xff000000u | PAL[c];
		}
	for (y = 70; y < 90; y++)
		for (x = 20; x < 60; x++)
			screen[y * S + x] = 0xff000000u | PAL[(x == 20 || x == 59 || y == 70 || y == 89) ? 0 : 10];
	for (y = 100; y < 116; y++)
		for (x = 0; x < 16; x++)
			screen[y * S + ((x + frame * 4) % S)] = 0xff000000u | PAL[8];
}

static bool write_ppm(const char *path, const uint8_t *rgb, int w, int h)
{
	FILE *f = fopen(path, "wb");
	bool ok;

	if (!f) return false;
	ok = fprintf(f, "P6\n%d %d\n255\n", w, h) > 0 && fwrite(rgb, 3, (size_t)w * h, f) == (size_t)w * h;
	return fclose(f) == 0 && ok;
}

int main(int argc, char **argv)
{
	bool sharp = false, fps = false;
	int preblit = 0, frames = 3, i;
	const char *out = NULL, *scene = NULL, *then = NULL;
	SDL_Rect dst = { -1, 0, 0, 0 };
	SDL_Window *win;
	SDL_Renderer *r;
	SDL_Texture *tex, *fps_tex;
	uint32_t *big, fps_px[S * 8];
	uint8_t *rgb;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--sharp")) sharp = true;
		else if (!strcmp(argv[i], "--fps")) fps = true;
		else if (!strcmp(argv[i], "--preblit") && i + 1 < argc) preblit = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--scene") && i + 1 < argc) scene = argv[++i];
		else if (!strcmp(argv[i], "--then") && i + 1 < argc) then = argv[++i];
		else if (!strcmp(argv[i], "--rect") && i + 1 < argc &&
		         sscanf(argv[i + 1], "%d,%d,%d,%d", &dst.x, &dst.y, &dst.w, &dst.h) == 4) i++;
		else if (argv[i][0] != '-' && !out) out = argv[i];
		else {
			fprintf(stderr, "usage: p8fx-standin [--sharp] [--preblit K] [--rect X,Y,W,H] [--fps] "
			                "[--frames N] [--scene SCENE.ppm] [--then LOOK] OUT.ppm\n");
			return 2;
		}
	}
	if (!out || frames < 1) return 2;
	if (preblit <= 0) preblit = sharp ? 1 : 3;
	if (dst.x == -1 && !dst.w) dst = sharp ? (SDL_Rect){ 128, 48, 384, 384 } : (SDL_Rect){ 80, 0, 480, 480 };

	if (SDL_Init(SDL_INIT_VIDEO)) {
		fprintf(stderr, "p8fx-standin: %s\n", SDL_GetError());
		return 1;
	}
	win = SDL_CreateWindow("PICO-8", 0, 0, W, H, SDL_WINDOW_FULLSCREEN);
	r = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC) : NULL;
	if (!r) {
		fprintf(stderr, "p8fx-standin: %s\n", SDL_GetError());
		return 1;
	}
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, sharp ? "nearest" : "linear");
	SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
	SDL_RenderClear(r);
	SDL_RenderPresent(r);
	tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, S * preblit, S * preblit);
	fps_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, S, 8);
	big = malloc((size_t)S * preblit * S * preblit * 4);
	rgb = malloc((size_t)W * H * 3 > (size_t)S * S * 3 ? (size_t)W * H * 3 : (size_t)S * S * 3);
	if (!tex || !fps_tex || !big || !rgb) {
		fprintf(stderr, "p8fx-standin: %s\n", SDL_GetError());
		return 1;
	}
	/* The FPS line: a light bar on black, as PICO-8's text sits */
	for (i = 0; i < S * 8; i++) fps_px[i] = (i % S) < 40 && i / S > 1 && i / S < 7 ? 0xfffff1e8u : 0xff000000u;

	for (int f = 0; f < frames; f++) {
		int x, y;

		draw(f);
		/* PICO-8's preblit: each pixel repeated K times each way */
		for (y = 0; y < S * preblit; y++)
			for (x = 0; x < S * preblit; x++)
				big[(size_t)y * S * preblit + x] = screen[(y / preblit) * S + x / preblit];
		SDL_UpdateTexture(tex, NULL, big, S * preblit * 4);
		SDL_RenderClear(r);
		SDL_RenderCopy(r, tex, NULL, &dst);
		if (fps) {
			SDL_Rect at = { 0, 0, S, 8 };

			SDL_UpdateTexture(fps_tex, NULL, fps_px, S * 4);
			SDL_RenderCopy(r, fps_tex, NULL, &at);
		}
		if (f == frames - 1) {
			if (SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_RGB24, rgb, W * 3) ||
			    !write_ppm(out, rgb, W, H)) {
				fprintf(stderr, "p8fx-standin: %s: %s\n", out, SDL_GetError());
				return 1;
			}
			if (scene) {
				for (i = 0; i < S * S; i++) {
					rgb[i * 3] = (uint8_t)(screen[i] >> 16);
					rgb[i * 3 + 1] = (uint8_t)(screen[i] >> 8);
					rgb[i * 3 + 2] = (uint8_t)screen[i];
				}
				if (!write_ppm(scene, rgb, S, S)) return 1;
			}
		}
		SDL_RenderPresent(r);
		if (f == 0 && then) {
			const char *look = getenv("ZYPEROS_P8FX");

			if (!look || rename(then, look)) {
				fprintf(stderr, "p8fx-standin: --then %s: no look file to move it over\n", then);
				return 1;
			}
			/* The shim looks at the file four times a second */
			usleep(400 * 1000);
		}
	}
	SDL_DestroyTexture(fps_tex);
	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(r);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
