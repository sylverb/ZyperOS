/* SPDX-License-Identifier: GPL-3.0-only */
/* MP4's video picture of any shape, from the daemon's frames to the texture
 * the player draws it from:
 *
 *   make check-mp4size
 *
 * A 4:3 video is narrower than the panel more often than not: what a video
 * site calls 360p is 480x360 at 4:3, and 240p is 320x240. The Pixel 2's GPU
 * faulted, and took the launcher with it, on snap textures narrower than
 * 640x480 (src/mp4frame.h, mpf_tex_size), and MP4's texture was its
 * picture's own size. For each size here - 4:3, odd, tiny, the largest, the
 * size changing from one picture to the next as a joined compilation's does -
 * pictures are published into a frames file as src/muse/vid.c publishes them
 * and read back by src/mp4video.c itself, built into this check against a
 * frames file of its own. The texture must never be smaller than 640x480 nor
 * than the picture; mp4v_texture must say where in it the picture is; drawn
 * from there, every pixel is the picture's and none the black around it;
 * past a smaller picture that follows a larger one, black, never the old
 * one; mp4v_fit keeps the picture's shape, its pixels' own counted; and
 * nothing may read or write out of bounds: built with UBSan where the
 * compiler has it, run under Guard Malloc on a Mac. Needs SDL2. */
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "../src/mp4frame.h"
#undef MPF_PATH
#define MPF_PATH MP4SIZE_FRAMES

#include "../src/mp4video.c"

static int failures;

#define CHECK(c, ...) do { if (!(c)) { printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); putchar('\n'); failures++; } } while (0)

/* mp4video.c's one need from the platform */
unsigned plat_now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (unsigned)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

static mpf_head *g_shm;
static uint32_t  g_serial;

/* One picture of w x h, white (studio range), packed as vid.c's publish()
 * packs it: Y w x h, then U and V (w+1)/2 x (h+1)/2 */
static void publish(uint32_t w, uint32_t h, uint32_t sar_num, uint32_t sar_den)
{
	int s = mpf_begin(g_shm);
	uint8_t *y = mpf_slot_data(g_shm, s);

	memset(y, 235, (size_t)w * h);
	memset(y + (size_t)w * h, 128, 2 * (size_t)mpf_chroma(w) * mpf_chroma(h));
	g_shm->slot[s].w = w;
	g_shm->slot[s].h = h;
	g_shm->slot[s].color = MPF_BT601;
	g_shm->slot[s].serial = ++g_serial;
	g_shm->slot[s].file = g_shm->file;
	g_shm->slot[s].pts = g_serial / 30.0;
	g_shm->sar_num = sar_num;
	g_shm->sar_den = sar_den;
	mpf_publish(g_shm, s);
}

/* The pixels of `r` on `surf` that are not white (want_white) or not black */
static long off_pixels(SDL_Surface *surf, SDL_Rect r, bool want_white)
{
	long bad = 0;
	int x, y;

	for (y = r.y; y < r.y + r.h; y++)
		for (x = r.x; x < r.x + r.w; x++) {
			Uint32 p = ((Uint32 *)((Uint8 *)surf->pixels + (size_t)y * surf->pitch))[x];
			Uint8 cr, cg, cb;

			SDL_GetRGB(p, surf->format, &cr, &cg, &cb);
			if (want_white ? (cr < 245 || cg < 245 || cb < 245) : (cr > 10 || cg > 10 || cb > 10))
				bad++;
		}
	return bad;
}

static void one_size(SDL_Renderer *r, SDL_Surface *surf, uint32_t w, uint32_t h,
                     uint32_t sar_num, uint32_t sar_den, bool new_file)
{
	SDL_Texture *t;
	SDL_Rect src = { -1, -1, -1, -1 }, box = { 0, 96, 1024, 576 }, fit;
	uint32_t pw, ph;
	int xw = 0, xh = 0, k;
	double aspect;

	printf("  %ux%u%s\n", w, h, new_file ? "" : ", the same file");
	if (new_file) __atomic_store_n(&g_shm->file, g_shm->file + 1, __ATOMIC_RELEASE);
	__atomic_store_n(&g_shm->active, 1u, __ATOMIC_RELEASE);
	for (k = 0; k < 3; k++) {
		publish(w, h, sar_num, sar_den);
		CHECK(mp4v_update(r), "%ux%u: picture %d taken", w, h, k);
	}
	t = mp4v_texture(&src);
	CHECK(t != NULL, "%ux%u: a texture", w, h);
	if (!t) return;

	/* Never a texture under 640x480 on the GPU, nor under the picture */
	SDL_QueryTexture(t, NULL, NULL, &xw, &xh);
	mpf_tex_size(w, h, &pw, &ph);
	CHECK(xw >= 640 && xh >= 480 && xw >= (int)w && xh >= (int)h,
	      "%ux%u: a texture of %dx%d, never under 640x480", w, h, xw, xh);
	CHECK(xw == (int)pw && xh == (int)ph, "%ux%u: the texture mpf_tex_size says, %ux%u, not %dx%d",
	      w, h, pw, ph, xw, xh);
	CHECK(src.x == 0 && src.y == 0 && src.w == (int)w && src.h == (int)h,
	      "%ux%u: drawn from %d,%d %dx%d", w, h, src.x, src.y, src.w, src.h);

	/* Drawn from that rectangle at its own size: all picture */
	SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
	SDL_RenderClear(r);
	SDL_RenderCopy(r, t, &src, &(SDL_Rect){ 0, 0, (int)w, (int)h });
	SDL_RenderFlush(r);
	{
		long bad = off_pixels(surf, (SDL_Rect){ 0, 0, (int)w, (int)h }, true);

		CHECK(bad == 0, "%ux%u: %ld pixels drawn from its rectangle are not the picture", w, h, bad);
	}
	/* The whole texture: black past the picture, never an older picture */
	if (xw > (int)w || xh > (int)h) {
		long bad;

		SDL_RenderClear(r);
		SDL_RenderCopy(r, t, NULL, &(SDL_Rect){ 0, 0, xw, xh });
		SDL_RenderFlush(r);
		bad = off_pixels(surf, (SDL_Rect){ (int)w, 0, xw - (int)w, xh }, false) +
		      off_pixels(surf, (SDL_Rect){ 0, (int)h, (int)w, xh - (int)h }, false);
		CHECK(bad == 0, "%ux%u: %ld pixels past it in its %dx%d texture are not black",
		      w, h, bad, xw, xh);
	}

	/* Fitted to the player's box, in its own shape */
	fit = mp4v_fit(box);
	aspect = (double)w / h * sar_num / sar_den;
	CHECK(fit.w <= box.w && fit.h <= box.h && (fit.w == box.w || fit.h == box.h),
	      "%ux%u: fitted to %dx%d, inside %dx%d and touching it", w, h, fit.w, fit.h, box.w, box.h);
	CHECK(fit.w == box.w ? fabs(fit.h - box.w / aspect) <= 1 : fabs(fit.w - box.h * aspect) <= 1,
	      "%ux%u at %u:%u: fitted to %dx%d, not its shape", w, h, sar_num, sar_den, fit.w, fit.h);
}

int main(void)
{
	/* w, h, its pixels' shape, and whether it is a new file or the size
	 * changing inside one (a compilation joined without re-encoding) */
	static const struct { uint32_t w, h, sn, sd; bool file; } SIZES[] = {
		{ 480, 360, 1, 1, true },   { 320, 240, 1, 1, true },   { 640, 480, 1, 1, true },
		{ 720, 540, 1, 1, true },   { 960, 720, 1, 1, true },   { 1440, 1080, 1, 1, true },
		{ 720, 480, 8, 9, true },   { 704, 576, 12, 11, true }, { 720, 576, 16, 15, true },
		{ 640, 360, 1, 1, true },   { 854, 480, 1, 1, true },   { 426, 240, 1, 1, true },
		{ 321, 241, 1, 1, true },   { 718, 539, 1, 1, true },   { 1, 1, 1, 1, true },
		{ 2, 2, 1, 1, true },       { 1, 480, 1, 1, true },     { 480, 1, 1, 1, true },
		{ MPF_MAX_W, MPF_MAX_H, 1, 1, true }, { 1450, 1088, 1, 1, true },
		/* one file, its pictures changing size: down inside one texture, up
		 * out of it, and back */
		{ 720, 540, 1, 1, true },   { 480, 360, 1, 1, false },  { 320, 240, 1, 1, false },
		{ 321, 241, 1, 1, false },  { 1440, 1080, 1, 1, false }, { 480, 360, 1, 1, false },
		{ 640, 480, 1, 1, false },  { 2, 2, 1, 1, false },      { 480, 360, 1, 1, false },
	};
	SDL_Surface *surf;
	SDL_Renderer *r;
	size_t i;
	int fd;

	setvbuf(stdout, NULL, _IONBF, 0);
	unlink(MP4SIZE_FRAMES);
	fd = open(MP4SIZE_FRAMES, O_RDWR | O_CREAT, 0644);
	if (fd < 0 || ftruncate(fd, MPF_SIZE) != 0) { printf("FAIL: no frames file %s\n", MP4SIZE_FRAMES); return 1; }
	g_shm = mmap(NULL, MPF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (g_shm == MAP_FAILED) { printf("FAIL: mmap\n"); return 1; }
	g_shm->version = MPF_VERSION;
	g_shm->magic = MPF_MAGIC;

	if (SDL_Init(SDL_INIT_VIDEO) != 0) { printf("SDL: %s\n", SDL_GetError()); return 1; }
	surf = SDL_CreateRGBSurfaceWithFormat(0, MPF_MAX_W, MPF_MAX_H, 32, SDL_PIXELFORMAT_ARGB8888);
	r = surf ? SDL_CreateSoftwareRenderer(surf) : NULL;
	if (!r) { printf("FAIL: no renderer: %s\n", SDL_GetError()); return 1; }

	printf("MP4 pictures of every shape, read into their texture and drawn:\n");
	for (i = 0; i < sizeof SIZES / sizeof SIZES[0]; i++)
		one_size(r, surf, SIZES[i].w, SIZES[i].h, SIZES[i].sn, SIZES[i].sd, SIZES[i].file);
	mp4v_release();
	SDL_DestroyRenderer(r);
	SDL_FreeSurface(surf);
	munmap(g_shm, MPF_SIZE);
	unlink(MP4SIZE_FRAMES);
	if (failures) { printf("mp4size: %d FAILED\n", failures); return 1; }
	printf("ok: an MP4 picture of any shape goes to the GPU in a texture never under 640x480, "
	       "and is drawn whole from its own rectangle\n");
	return 0;
}
