/* SPDX-License-Identifier: GPL-3.0-only */
/* The cassette's textures: see cassette.h. Drawn on the CPU once, as signed
 * distances (src/ds/sdf.h), each part over the last with a one-unit soft
 * edge, into a canvas of straight-alpha floats, then a texture. */
#include "cassette.h"

#include "ds/sdf.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The materials, the only colors the look has of its own besides the
 * cover's (DESIGN.md 11.64): the smoke-grey shell and its moulded edge, the
 * dark inside seen through the window, the screws, the hubs' white plastic,
 * the tape's brown */
typedef struct { float r, g, b, a; } rgba;
static const rgba SMOKE     = { 214 / 255.f, 212 / 255.f, 203 / 255.f, 0.55f };
static const rgba MOULD     = { 156 / 255.f, 154 / 255.f, 146 / 255.f, 1.0f };
static const rgba INSIDE    = { 30 / 255.f, 32 / 255.f, 30 / 255.f, 0.62f };
static const rgba SLOT      = { 0.0f, 0.0f, 0.0f, 0.45f };
static const rgba HOLE      = { 18 / 255.f, 20 / 255.f, 19 / 255.f, 1.0f };
static const rgba TAPE      = { 70 / 255.f, 51 / 255.f, 42 / 255.f, 1.0f };
static const rgba HUB_DARK  = { 217 / 255.f, 216 / 255.f, 208 / 255.f, 1.0f };
static const rgba HUB_LIGHT = { 244 / 255.f, 243 / 255.f, 238 / 255.f, 1.0f };
static const rgba TOOTH     = { 154 / 255.f, 154 / 255.f, 146 / 255.f, 1.0f };
static const rgba SPINDLE   = { 30 / 255.f, 32 / 255.f, 31 / 255.f, 1.0f };
/* The pack, lighter where the tape is wound tight near the hub and darker
 * at its edge: 0.45, 0.8 and 1 of its radius */
static const rgba PACK_IN   = { 90 / 255.f, 66 / 255.f, 50 / 255.f, 1.0f };
static const rgba PACK_MID  = { 70 / 255.f, 51 / 255.f, 42 / 255.f, 1.0f };
static const rgba PACK_EDGE = { 46 / 255.f, 34 / 255.f, 28 / 255.f, 1.0f };

typedef struct { int w, h; float *p; } canvas;     /* w x h of r, g, b, a */

static bool cv_new(canvas *c, int w, int h)
{
	c->w = w;
	c->h = h;
	c->p = calloc((size_t)w * h * 4, sizeof *c->p);
	return c->p != NULL;
}

/* `col` over pixel (x, y) at coverage k */
static void put(canvas *c, int x, int y, rgba col, float k)
{
	float *d = c->p + ((size_t)y * c->w + x) * 4;
	float a = col.a * k, out = a + d[3] * (1 - a);

	if (a <= 0 || out <= 0) return;
	d[0] = (col.r * a + d[0] * d[3] * (1 - a)) / out;
	d[1] = (col.g * a + d[1] * d[3] * (1 - a)) / out;
	d[2] = (col.b * a + d[2] * d[3] * (1 - a)) / out;
	d[3] = out;
}

/* A shape, its signed distance `sd` at a pixel's middle, over the box
 * (x0, y0)-(x1, y1); the color `fn`'s at that pixel when it is given */
typedef float (*sd_fn)(const float *q, float x, float y);
typedef rgba (*col_fn)(const float *q, float x, float y);

static void paint(canvas *c, sd_fn sd, col_fn fn, const float *q, rgba col, float x0, float y0,
                  float x1, float y1)
{
	int ix0 = (int)floorf(x0) - 1, iy0 = (int)floorf(y0) - 1;
	int ix1 = (int)ceilf(x1) + 1, iy1 = (int)ceilf(y1) + 1, x, y;

	if (ix0 < 0) ix0 = 0;
	if (iy0 < 0) iy0 = 0;
	if (ix1 > c->w) ix1 = c->w;
	if (iy1 > c->h) iy1 = c->h;
	for (y = iy0; y < iy1; y++)
		for (x = ix0; x < ix1; x++) {
			float px = (float)x + 0.5f, py = (float)y + 0.5f;
			float k = ds_sd_clamp(0.5f - sd(q, px, py), 0, 1);

			if (k > 0) put(c, x, y, fn ? fn(q, px, py) : col, k);
		}
}

/* q: x0 y0 x1 y1 r */
static float sd_rbox(const float *q, float x, float y)
{
	return ds_sd_rbox(x, y, q[0], q[1], q[2], q[3], q[4]);
}

/* q: x0 y0 x1 y1 r w: the box's outline, w wide, centred on its edge */
static float sd_rbox_line(const float *q, float x, float y)
{
	return fabsf(ds_sd_rbox(x, y, q[0], q[1], q[2], q[3], q[4])) - q[5] / 2;
}

/* q: cx cy r */
static float sd_disc(const float *q, float x, float y)
{
	return hypotf(x - q[0], y - q[1]) - q[2];
}

/* q: cx cy r w */
static float sd_ring(const float *q, float x, float y)
{
	return fabsf(hypotf(x - q[0], y - q[1]) - q[2]) - q[3] / 2;
}

/* q: x0 y0 x1 y1 w: a stroke with round ends */
static float sd_seg(const float *q, float x, float y)
{
	return ds_sd_segment(x, y, q[0], q[1], q[2], q[3]) - q[4] / 2;
}

/* q: the label's box and corner, then the window's pill: the paper with the
 * window cut from it */
static float sd_paper(const float *q, float x, float y)
{
	float label = ds_sd_rbox(x, y, q[0], q[1], q[2], q[3], q[4]);
	float pill = ds_sd_rbox(x, y, q[5], q[6], q[7], q[8], (q[8] - q[6]) / 2);

	return fmaxf(label, -pill);
}

/* q: the trapezoid's four corners */
static float sd_poly4(const float *q, float x, float y)
{
	return ds_sd_poly(q, 4, x, y);
}

static rgba mix(rgba a, rgba b, float t)
{
	t = ds_sd_clamp(t, 0, 1);
	return (rgba){ a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
	               a.a + (b.a - a.a) * t };
}

/* The pack's brown from its middle out (q: cx cy r) */
static rgba pack_col(const float *q, float x, float y)
{
	float t = hypotf(x - q[0], y - q[1]) / q[2];

	return t < 0.45f ? PACK_IN : t < 0.8f ? mix(PACK_IN, PACK_MID, (t - 0.45f) / 0.35f)
	                                      : mix(PACK_MID, PACK_EDGE, (t - 0.8f) / 0.2f);
}

/* The glass's light: white, 16% at the window's top to none at its middle
 * (q: the pill's box) */
static rgba glass_col(const float *q, float x, float y)
{
	float t = (y - q[1]) / ((q[3] - q[1]) / 2);

	(void)x;
	return (rgba){ 1, 1, 1, 0.16f * (1 - ds_sd_clamp(t, 0, 1)) };
}

static rgba role(ds_rgb c)
{
	return (rgba){ c.r / 255.f, c.g / 255.f, c.b / 255.f, 1 };
}

static SDL_Texture *texture(SDL_Renderer *r, canvas *c)
{
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, c->w, c->h, 32, SDL_PIXELFORMAT_ARGB8888);
	SDL_Texture *t = NULL;
	int x, y;

	if (s && SDL_LockSurface(s) == 0) {
		for (y = 0; y < c->h; y++) {
			Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + (size_t)y * s->pitch);

			for (x = 0; x < c->w; x++) {
				const float *d = c->p + ((size_t)y * c->w + x) * 4;

				row[x] = (Uint32)(d[3] * 255 + 0.5f) << 24 | (Uint32)(d[0] * 255 + 0.5f) << 16 |
				         (Uint32)(d[1] * 255 + 0.5f) << 8 | (Uint32)(d[2] * 255 + 0.5f);
			}
		}
		SDL_UnlockSurface(s);
		t = SDL_CreateTextureFromSurface(r, s);
		if (t) {
			SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
			SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
		}
	}
	if (s) SDL_FreeSurface(s);
	free(c->p);
	c->p = NULL;
	return t;
}

/* A screw's head, 3.4 mm, sunk in its hole, its cross slot */
static void screw(canvas *c, float cx, float cy)
{
	const float d[3] = { cx, cy, 10 }, ring[4] = { cx, cy, 10, 1.5f };
	const float h[5] = { cx - 6, cy, cx + 6, cy, 2 }, v[5] = { cx, cy - 6, cx, cy + 6, 2 };

	paint(c, sd_disc, NULL, d, MOULD, cx - 10, cy - 10, cx + 10, cy + 10);
	paint(c, sd_ring, NULL, ring, SLOT, cx - 12, cy - 12, cx + 12, cy + 12);
	paint(c, sd_seg, NULL, h, SLOT, cx - 7, cy - 2, cx + 7, cy + 2);
	paint(c, sd_seg, NULL, v, SLOT, cx - 2, cy - 7, cx + 2, cy + 7);
}

/* The shell, its screws and its head end, the label with the window cut
 * from it, and the dark inside the window: everything that does not move */
static SDL_Texture *shell(SDL_Renderer *r, const cas_tex *t)
{
	canvas c;
	const float body[5] = { 1.5f, 1.5f, CAS_W - 1.5f, CAS_H - 1.5f, 16 };
	const float rim[6] = { 1.5f, 1.5f, CAS_W - 1.5f, CAS_H - 1.5f, 16, 3 };
	/* The head end, a trapezoid 54.3 mm along the foot and 40.7 along
	 * its top, 16.3 high */
	const float head[8] = { CAS_WIN_X0, CAS_H, CAS_WIN_X1, CAS_H, 416, 280, 176, 280 };
	/* Its openings: the pinch rollers' either side, the head's between */
	const float hole_l[5] = { 192, 320, 232, 352, 4 }, hole_r[5] = { 360, 320, 400, 352, 4 },
	            hole_m[5] = { 272, 328, 320, 352, 4 };
	const float tape[5] = { 180, 354, 412, 354, 4 };
	const float inside[5] = { CAS_WIN_X0, CAS_WIN_Y0, CAS_WIN_X1, CAS_WIN_Y1, (CAS_WIN_Y1 - CAS_WIN_Y0) / 2.0f };
	const float paper[9] = { CAS_LABEL_X0, CAS_LABEL_Y0, CAS_LABEL_X1, CAS_LABEL_Y1, 12,
	                         CAS_WIN_X0, CAS_WIN_Y0, CAS_WIN_X1, CAS_WIN_Y1 };

	if (!cv_new(&c, CAS_W, CAS_H)) return NULL;
	paint(&c, sd_rbox, NULL, body, SMOKE, 0, 0, CAS_W, CAS_H);
	paint(&c, sd_rbox_line, NULL, rim, MOULD, 0, 0, CAS_W, CAS_H);
	screw(&c, 24, 24);
	screw(&c, CAS_W - 24, 24);
	screw(&c, 24, CAS_H - 24);
	screw(&c, CAS_W - 24, CAS_H - 24);
	paint(&c, sd_poly4, NULL, head, MOULD, CAS_WIN_X0, 280, CAS_WIN_X1, CAS_H);
	screw(&c, CAS_W / 2, 300);
	paint(&c, sd_rbox, NULL, hole_l, HOLE, 192, 320, 232, 352);
	paint(&c, sd_rbox, NULL, hole_r, HOLE, 360, 320, 400, 352);
	paint(&c, sd_rbox, NULL, hole_m, HOLE, 272, 328, 320, 352);
	paint(&c, sd_seg, NULL, tape, TAPE, 176, 350, 416, 358);
	paint(&c, sd_rbox, NULL, inside, INSIDE, CAS_WIN_X0, CAS_WIN_Y0, CAS_WIN_X1, CAS_WIN_Y1);
	paint(&c, sd_paper, NULL, paper, role(t->paper), CAS_LABEL_X0, CAS_LABEL_Y0, CAS_LABEL_X1,
	      CAS_LABEL_Y1);
	return texture(r, &c);
}

/* The window's box over the reels: the label's paper in its corners round
 * the pill, the pill's moulded edge, and the light on its glass */
static SDL_Texture *glass(SDL_Renderer *r, const cas_tex *t)
{
	const float w = CAS_WIN_X1 - CAS_WIN_X0, h = CAS_WIN_Y1 - CAS_WIN_Y0;
	/* The paper: the box less the pill (the label's corner is far away) */
	const float paper[9] = { -40, -40, w + 40, h + 40, 0, 0, 0, w, h };
	const float pill[5] = { 0, 0, w, h, h / 2 }, edge[6] = { 0, 0, w, h, h / 2, 2 };
	canvas c;

	if (!cv_new(&c, (int)w, (int)h)) return NULL;
	paint(&c, sd_rbox, glass_col, pill, (rgba){ 0 }, 0, 0, w, h);
	paint(&c, sd_paper, NULL, paper, role(t->paper), 0, 0, w, h);
	paint(&c, sd_rbox_line, NULL, edge, MOULD, 0, 0, w, h);
	return texture(r, &c);
}

/* A reel's hub: the white plastic the tape winds on, the clamp that holds
 * the tape's end in its rim, and the six teeth round the spindle's hole */
static SDL_Texture *hub(SDL_Renderer *r, const cas_tex *t)
{
	const int side = (int)ceilf(CAS_HUB_R) * 2;
	const float m = (float)side / 2;
	const float disc[3] = { m, m, CAS_HUB_R - 0.5f }, rim[4] = { m, m, CAS_HUB_R - 2, 2 };
	const float spindle[3] = { m, m, 24 }, clamp[5] = { m + 46, m, m + 58, m, 6 };
	canvas c;
	int k;

	if (!cv_new(&c, side, side)) return NULL;
	paint(&c, sd_disc, NULL, disc, t->dark ? HUB_DARK : HUB_LIGHT, 0, 0, side, side);
	paint(&c, sd_ring, NULL, rim, MOULD, 0, 0, side, side);
	paint(&c, sd_seg, NULL, clamp, TOOTH, m + 40, m - 4, m + 62, m + 4);
	paint(&c, sd_disc, NULL, spindle, SPINDLE, m - 24, m - 24, m + 24, m + 24);
	for (k = 0; k < 6; k++) {
		/* The hub's own plastic, from the hole's wall in toward the
		 * spindle, between the clamp's way round */
		float a = (float)k * (float)M_PI / 3 + (float)M_PI / 6, ca = cosf(a), sa = sinf(a);
		const float tooth[5] = { m + ca * 26, m + sa * 26, m + ca * 16, m + sa * 16, 7 };

		paint(&c, sd_seg, NULL, tooth, t->dark ? HUB_DARK : HUB_LIGHT, m - 30, m - 30, m + 30, m + 30);
	}
	return texture(r, &c);
}

/* A full pack of tape, its brown darker toward its edge */
static SDL_Texture *pack(SDL_Renderer *r)
{
	const int side = (int)ceilf(CAS_PACK_R) * 2;
	const float q[3] = { (float)side / 2, (float)side / 2, CAS_PACK_R - 0.5f };
	canvas c;

	if (!cv_new(&c, side, side)) return NULL;
	paint(&c, sd_disc, pack_col, q, (rgba){ 0 }, 0, 0, side, side);
	return texture(r, &c);
}

void cas_free(cas_tex *t)
{
	if (t->shell) SDL_DestroyTexture(t->shell);
	if (t->glass) SDL_DestroyTexture(t->glass);
	if (t->hub) SDL_DestroyTexture(t->hub);
	if (t->pack) SDL_DestroyTexture(t->pack);
	t->shell = t->glass = t->hub = t->pack = NULL;
}

bool cas_ready(SDL_Renderer *r, cas_tex *t, const ds_theme *th, bool dark)
{
	if (t->shell && t->glass && t->hub && t->pack && !memcmp(&t->paper, &th->surface, sizeof t->paper) &&
	    t->dark == dark)
		return true;
	{
		Uint32 t0 = SDL_GetTicks();

		cas_free(t);
		t->paper = th->surface;
		t->dark = dark;
		t->shell = shell(r, t);
		t->glass = glass(r, t);
		t->hub = hub(r, t);
		t->pack = pack(r);
		/* Once a theme: what it costs, for the device's log */
		fprintf(stderr, "mp4: the cassette drawn in %u ms\n", (unsigned)(SDL_GetTicks() - t0));
	}
	return t->shell && t->glass && t->hub && t->pack;
}
