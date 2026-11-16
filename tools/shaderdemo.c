/* SPDX-License-Identifier: GPL-3.0-only */
/* The Shader list's demonstration (DESIGN.md 11.3, a choice with a picture):
 * a game-like scene of its own (Zyper on a ledge, a bone, a score, stars;
 * no game's art), drawn here at a console's 256x224, through
 * each of Diatom's filters as the handheld draws it, and the top left of it
 * kept at the handheld's own pixels, one image a filter and, for an old
 * screen, a Strength step. Each filter is Diatom's GLSL (diatom/port/pixel2.c)
 * in C, line for line: the same numbers in the same order, in float as the
 * GPU's highp is.
 *
 *     build-native/shaderdemo OUT_DIR
 *
 * writes OUT_DIR/<panel>/<filter>.png (at 100%) and <filter>-<strength>.png
 * (10 to 90): a panel's pictures, which src/shaderdemo.c reads by the
 * panel's size (dmode_surface). */
#include <SDL.h>
#include <SDL_image.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* A 256x224 console's picture, as NES, Super NES and Mega Drive games draw,
 * shown 4:3 as Original Shape shows it: a scale no panel divides evenly,
 * where Off's pixels come out of two sizes and Sharp-Shimmerless's do not */
#define SW 256
#define SH 224

static unsigned char scene[SH][SW][3];

/* ---- the scene: a sky, a score, a hero on a ledge, a coin, a slope ---------- */

static void put(int x, int y, unsigned rgb)
{
	if (x < 0 || y < 0 || x >= SW || y >= SH) return;
	scene[y][x][0] = (unsigned char)(rgb >> 16);
	scene[y][x][1] = (unsigned char)(rgb >> 8);
	scene[y][x][2] = (unsigned char)rgb;
}

/* A 3x5 font for the score's letters and digits */
static const char *GLYPH(char c)
{
	switch (c) {
	case '0': return "111101101101111";
	case '2': return "111001111100111";
	case '4': return "101101111001001";
	case 'S': return "111100111001111";
	case 'C': return "111100100100111";
	case 'O': return "111101101101111";
	case 'R': return "110101110101101";
	case 'E': return "111100110100111";
	default:  return "000000000000000";
	}
}

static void text(int x, int y, const char *s, unsigned rgb, unsigned shadow)
{
	for (; *s; s++, x += 4) {
		const char *g = GLYPH(*s);

		for (int j = 0; j < 5; j++)
			for (int i = 0; i < 3; i++)
				if (g[j * 3 + i] == '1') {
					put(x + i + 1, y + j + 1, shadow);
					put(x + i, y + j, rgb);
				}
	}
}

/* Zyper, ZyperOS's own schnauzer, 16 x 16: his ears, white brows and
 * beard, his nose and tongue, sitting */
static const char *ZYPER[16] = {
	"....KK....KK....",
	"...KGGK..KGGK...",
	"...KGGGKKGGGK...",
	"..KGGGGGGGGGGK..",
	"..KGWWGGGGWWGK..",
	"..KGGEGGGGEGGK..",
	"..KGGGGGGGGGGK..",
	"...KWWWNNWWWK...",
	"...KWWWWWWWWK...",
	"....KWWPPWWK....",
	".....KWPPWK.....",
	"....KKKKKKKK....",
	"...KGGGGGGGGK...",
	"..KGGGGGGGGGGK..",
	"..KGGK....KGGK..",
	"..KKK......KKK..",
};

static void zyper(int x, int y)
{
	for (int j = 0; j < 16; j++)
		for (int i = 0; i < 16; i++) {
			unsigned c;

			switch (ZYPER[j][i]) {
			case 'K': c = 0x2b2b33; break;
			case 'G': c = 0x8a8f9a; break;
			case 'W': c = 0xf2f2f2; break;
			case 'E': case 'N': c = 0x111111; break;
			case 'P': c = 0xff7aa8; break;
			default:  continue;
			}
			put(x + i, y + j, c);
		}
}

static void draw_scene(void)
{
	/* The sky in bands, deep blue to pale, as a console's palette steps */
	static const unsigned SKY[8] = { 0x16205a, 0x1d2c74, 0x25398c, 0x2e49a3, 0x3a5cb8,
	                                 0x4f75cb, 0x6a92db, 0x8cb2ea };
	for (int y = 0; y < SH; y++)
		for (int x = 0; x < SW; x++) put(x, y, SKY[y * 8 / 150 < 8 ? y * 8 / 150 : 7]);
	/* Stars */
	static const int STAR[][2] = { { 12, 22 }, { 47, 15 }, { 70, 28 }, { 88, 10 }, { 130, 18 },
	                               { 170, 30 }, { 210, 12 }, { 260, 24 }, { 300, 8 } };
	for (size_t k = 0; k < sizeof STAR / sizeof STAR[0]; k++) {
		put(STAR[k][0], STAR[k][1], 0xffffff);
		put(STAR[k][0] + 1, STAR[k][1], 0x9fb4ff);
		put(STAR[k][0], STAR[k][1] + 1, 0x9fb4ff);
	}
	/* A cloud */
	for (int y = 0; y < 14; y++)
		for (int x = 0; x < 40; x++) {
			int dx = x - 20, dy = y - 9;

			if (dx * dx / 4 + dy * dy * 2 < 100 || (x > 6 && x < 22 && y > 2 && (x - 14) * (x - 14) + (y - 7) * (y - 7) < 40))
				put(150 + x, 36 + y, y > 10 ? 0xc6d4f2 : 0xf4f7ff);
		}
	/* The score, top left */
	text(4, 4, "SCORE", 0x9ff0c8, 0x101830);
	text(28, 4, "4200", 0xffffff, 0x101830);
	/* A grassy ledge of stone blocks under him: a grass edge with tufts,
	 * then grey blocks with dark joints */
	for (int y = 48; y < 74; y++)
		for (int x = 0; x < 64; x++) {
			unsigned c;

			if (y < 51) c = (y == 48 && x % 5 == 2) ? 0x9ef06a : 0x4cc24a;
			else if (y == 51) c = 0x2e8a36;
			else {
				bool joint = (y - 52) % 7 == 6 || ((x + ((y - 52) / 7 % 2) * 6) % 12 == 11);

				c = joint ? 0x3b3f4a : ((x + y) % 9 == 0 ? 0x9aa0ad : 0x7d8391);
			}
			put(x, y, c);
		}
	/* Zyper on it, a bone floating past the edge */
	zyper(16, 32);
	for (int y = 0; y < 6; y++)
		for (int x = 0; x < 12; x++) {
			bool knob = (x < 3 || x > 8) && (y < 2 || y > 3), bar = x >= 2 && x <= 9 && y >= 2 && y <= 3;

			if (knob || bar) put(48 + x, 24 + y, (y == 0 || y == 5 || (bar && y == 3)) ? 0xd9d2c0 : 0xfffbea);
		}
	/* A green slope from the ledge's foot up to the right, outlined */
	for (int x = 64; x < SW; x++) {
		int top = 74 - (x - 64) / 2;

		if (top < 40) top = 40;
		for (int y = top; y < SH; y++) put(x, y, y == top ? 0x174a1a : (y - top < 3 ? 0x3fbf4a : 0x2f8f3a));
	}
	/* The ground under it all */
	for (int y = 74; y < SH; y++)
		for (int x = 0; x < 64; x++) put(x, y, ((x / 8 + y / 8) & 1) ? 0x7a3a18 : 0x8c4620);
}

/* ---- GLSL in C ---------------------------------------------------------------- */

typedef struct { float x, y; } v2;
typedef struct { float r, g, b; } v3;

static float stepf(float edge, float x) { return x < edge ? 0.0f : 1.0f; }
static float mixf(float a, float b, float t) { return a * (1.0f - t) + b * t; }
static float fractf(float x) { return x - floorf(x); }
static float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }

/* texture(): LINEAR, clamped to the edge, at uv in texels / size */
static v3 texture(float u, float v)
{
	float x = u * SW - 0.5f, y = v * SH - 0.5f;
	int x0 = (int)floorf(x), y0 = (int)floorf(y);
	float fx = x - (float)x0, fy = y - (float)y0;
	v3 c = { 0, 0, 0 };

	for (int j = 0; j < 2; j++)
		for (int i = 0; i < 2; i++) {
			int xi = x0 + i < 0 ? 0 : x0 + i >= SW ? SW - 1 : x0 + i;
			int yi = y0 + j < 0 ? 0 : y0 + j >= SH ? SH - 1 : y0 + j;
			float w = (i ? fx : 1.0f - fx) * (j ? fy : 1.0f - fy);

			c.r += w * scene[yi][xi][0] / 255.0f;
			c.g += w * scene[yi][xi][1] / 255.0f;
			c.b += w * scene[yi][xi][2] / 255.0f;
		}
	return c;
}

typedef enum { F_NEAREST, F_SHIMMERLESS, F_CRT, F_CRT_CURVED, F_SCANLINES, F_LCD, F_COUNT } filt;
static const char *NAME[F_COUNT] = { "nearest", "shimmerless", "crt", "crt-curved", "scanlines", "lcd" };

/* One destination pixel (dx, dy) of a SWxSH picture drawn scale times */
static v3 shade(filt f, int dx, int dy, v2 scale, float strength)
{
	const v2 inv = { 1.0f / scale.x, 1.0f / scale.y };
	const v2 px = { (float)dx + 0.5f, (float)dy + 0.5f };

	if (f == F_NEAREST) {
		int x = (int)floorf(px.x * inv.x), y = (int)floorf(px.y * inv.y);

		return (v3){ scene[y][x][0] / 255.0f, scene[y][x][1] / 255.0f, scene[y][x][2] / 255.0f };
	}
	if (f == F_SHIMMERLESS) {
		float t[2], p[2] = { px.x, px.y }, s[2] = { scale.x, scale.y }, iv[2] = { inv.x, inv.y };

		for (int k = 0; k < 2; k++) {
			float tl = floorf(p[k]), br = ceilf(p[k]);
			float t_tl = floorf(iv[k] * tl), t_br = floorf(iv[k] * br);

			t[k] = t_br + 0.5f - (1.0f - stepf(t_br, t_tl)) * (s[k] * t_br - tl);
		}
		return texture(t[0] / SW, t[1] / SH);
	}
	if (f == F_SCANLINES || f == F_LCD) {
		const float ratio[2] = { f == F_LCD ? 0.3f : 0.0f, 0.3f }, opacity[2] = { f == F_LCD ? 0.3f : 0.0f, 0.3f };
		float t[2], fv[2], p[2] = { px.x, px.y }, s[2] = { scale.x, scale.y }, iv[2] = { inv.x, inv.y };
		v3 c;

		for (int k = 0; k < 2; k++) {
			const float RATIO = 1.0f - ratio[k], ALPHA = 1.0f - opacity[k] * strength;
			float tl = iv[k] * floorf(p[k]), br = iv[k] * ceilf(p[k]);
			float t_tl = floorf(tl), t_br = floorf(br), ftl = fractf(tl), fbr = fractf(br);
			float a_tl = mixf(RATIO - ftl + ALPHA * (1.0f - RATIO), ALPHA * (1.0f - RATIO), stepf(RATIO, ftl));
			float a_br = mixf(fbr, RATIO + ALPHA * (fbr - RATIO), stepf(RATIO, fbr));
			float w = mixf(a_br / (a_tl + a_br), 1.0f, stepf(t_br, t_tl));
			float ff = (fbr + clampf(RATIO - ftl, 0.0f, 1.0f)) * s[k];

			ff = mixf(ff, RATIO * s[k], stepf(RATIO, fbr));
			ff = mixf(ff, clampf((RATIO - ftl) * s[k], 0.0f, 1.0f), stepf(t_br, t_tl));
			fv[k] = ALPHA + (1.0f - ALPHA) * ff;
			t[k] = t_br - 0.5f + w;
		}
		c = texture(t[0] / SW, t[1] / SH);
		return (v3){ fv[0] * fv[1] * c.r, fv[0] * fv[1] * c.g, fv[0] * fv[1] * c.b };
	}
	{
		/* The CRT, and the CRT curved (WARP) */
		const float THIN = 0.75f, DARK = 0.5f, TX = 0.419207f, TY = 0.580793f;
		const v2 quad = { (float)SW, (float)SH };
		v2 pos = { px.x * inv.x, px.y * inv.y };
		float vin = 1.0f;

		if (f == F_CRT_CURVED) {
			v2 q = { px.x / (quad.x * scale.x) * 2.0f - 1.0f, px.y / (quad.y * scale.y) * 2.0f - 1.0f };
			v2 m = { 1.0f + q.y * q.y * 0.02f, 1.0f + q.x * q.x * 0.015f };

			q.x *= m.x;
			q.y *= m.y;
			vin = (1.0f - (1.0f - clampf(q.x * q.x, 0, 1)) * (1.0f - clampf(q.y * q.y, 0, 1))) * 1.001f;
			vin = clampf(quad.y - vin * quad.y, 0.0f, 1.0f);
			pos.x = (q.x + 1.0f) * 0.5f * quad.x;
			pos.y = (q.y + 1.0f) * 0.5f * quad.y;
		}
		float y0 = floorf(pos.y - 0.5f) + 0.5f;
		float x = pos.x + 0.5f, xi = floorf(x), xf = x - xi;

		xf = xf * xf * xf * (xf * (xf * 6.0f - 15.0f) + 10.0f);
		v2 p = { xi + xf - 0.5f, y0 };
		v3 a, b, c;
		for (int k = 0; k < 2; k++) {
			float u = clampf(p.x, 0.5f, quad.x - 0.5f), v = clampf(p.y + (float)k, 0.5f, quad.y - 0.5f);
			v3 t = texture(u / SW, v / SH);

			t.r *= t.r; t.g *= t.g; t.b *= t.b;
			if (k) b = t; else a = t;
		}
		float off = pos.y - y0;
		float sa = cosf(fminf(0.5f, off * THIN) * 6.2831853f) * 0.5f + 0.5f;
		float sb = cosf(fminf(0.5f, (1.0f - off) * THIN) * 6.2831853f) * 0.5f + 0.5f;

		c.r = (a.r * sa + b.r * sb) * vin;
		c.g = (a.g * sa + b.g * sb) * vin;
		c.b = (a.b * sa + b.b * sb) * vin;
		float mm = fractf(px.x * (1.0f / 3.0f));
		if (mm < 1.0f / 3.0f) c.r *= DARK; else if (mm < 2.0f / 3.0f) c.g *= DARK; else c.b *= DARK;
		float peak = fmaxf(1.0f / (256.0f * 65536.0f), fmaxf(c.r, fmaxf(c.g, c.b)));
		float k = 1.0f / peak * (peak / (peak * TX + TY));

		c.r *= k; c.g *= k; c.b *= k;
		if (strength < 1.0f) {
			float qy = off * off * off * (off * (off * 6.0f - 15.0f) + 10.0f);

			c.r = mixf(mixf(a.r, b.r, qy) * vin, c.r, strength);
			c.g = mixf(mixf(a.g, b.g, qy) * vin, c.g, strength);
			c.b = mixf(mixf(a.b, b.b, qy) * vin, c.b, strength);
		}
		return (v3){ sqrtf(fmaxf(c.r, 0)), sqrtf(fmaxf(c.g, 0)), sqrtf(fmaxf(c.b, 0)) };
	}
}

/* The Brick's Sharp-Shimmerless, its CPU path (diatom/port/brick.c, its
 * column and row maps and BLIT_ROW_BLEND): on each axis a source pixel and
 * a weight in 256ths, mixed across, then down, in whole numbers */
typedef struct { int idx, w; } tap;

static void cpu_map(tap *m, int dst, int src)
{
	const double scale = (double)dst / (double)src;
	int i;

	for (i = 0; i < dst; i++) {
		int l = (int)floor((double)i / scale), r = (int)floor((double)(i + 1) / scale), p = r > l ? r - 1 : l;
		double fr = r > l ? 1.0 - (scale * (double)r - (double)i) : 0.0;

		if (fr < 0.0) fr = 0.0;
		if (fr > 1.0) fr = 1.0;
		if (p >= src - 1) { p = src - 1; fr = 0.0; }
		if (p < 0) p = 0;
		m[i].idx = p;
		m[i].w = (int)(fr * 256.0 + 0.5);
	}
	/* A map whose every weight is 0 or 256 is the plain copy */
	for (i = 0; i < dst; i++)
		if (m[i].w != 0 && m[i].w != 256) return;
	for (i = 0; i < dst; i++)
		if (m[i].w == 256) { m[i].idx++; m[i].w = 0; }
}

static int cpu_mix(int a, int b, int w)
{
	return a + (((b - a) * w) >> 8);
}

static v3 shimmerless_cpu(const tap *cx, const tap *cy, int dx, int dy)
{
	const tap tx = cx[dx], ty = cy[dy];
	int out[3];

	for (int k = 0; k < 3; k++) {
		int a = scene[ty.idx][tx.idx][k], b;

		if (tx.w) a = cpu_mix(a, scene[ty.idx][tx.idx + 1][k], tx.w);
		if (ty.w) {
			b = scene[ty.idx + 1][tx.idx][k];
			if (tx.w) b = cpu_mix(b, scene[ty.idx + 1][tx.idx + 1][k], tx.w);
			a = cpu_mix(a, b, ty.w);
		}
		out[k] = a;
	}
	return (v3){ out[0] / 255.0f, out[1] / 255.0f, out[2] / 255.0f };
}

static tap g_cx[2048], g_cy[2048];
static bool g_cpu;   /* the device draws Sharp-Shimmerless on its CPU (the Brick) */

static int save(const char *dir, const char *dev, filt f, int strength, v2 scale, int cw, int ch)
{
	char path[512];
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, cw, ch, 24, SDL_PIXELFORMAT_RGB24);

	if (!s) return 1;
	for (int y = 0; y < ch; y++) {
		unsigned char *row = (unsigned char *)s->pixels + (size_t)y * s->pitch;

		for (int x = 0; x < cw; x++) {
			v3 c = f == F_SHIMMERLESS && g_cpu ? shimmerless_cpu(g_cx, g_cy, x, y)
			                                    : shade(f, x, y, scale, (float)strength / 100.0f);

			row[3 * x + 0] = (unsigned char)lroundf(clampf(c.r, 0, 1) * 255.0f);
			row[3 * x + 1] = (unsigned char)lroundf(clampf(c.g, 0, 1) * 255.0f);
			row[3 * x + 2] = (unsigned char)lroundf(clampf(c.b, 0, 1) * 255.0f);
		}
	}
	if (strength >= 100) snprintf(path, sizeof path, "%s/%s/%s.png", dir, dev, NAME[f]);
	else snprintf(path, sizeof path, "%s/%s/%s-%d.png", dir, dev, NAME[f], strength);
	int bad = IMG_SavePNG(s, path) != 0;
	SDL_FreeSurface(s);
	if (bad) fprintf(stderr, "shaderdemo: %s: %s\n", path, IMG_GetError());
	return bad;
}

int main(int argc, char **argv)
{
	/* Each handheld: its panel, which the 4:3 picture fills (640x480 on
	 * the Pixel 2, 1024x768 on the Brick), the close-up its preview's screen
	 * holds at its own pixels (304 x 228 units), and the filters it offers
	 * (the old screens where the GPU draws them) */
	static const struct { const char *id; int pw, ph, cw, ch; bool gpu, cpu; } DEV[] = {
		{ "640x480",   640, 480, 190, 142, true, false },  /* the Pixel 2: every filter a GLSL pass */
		{ "1024x768", 1024, 768, 304, 228, true, true },   /* the Brick (Sharp-Shimmerless on its CPU),
		                                                    * and the host's shots (the old screens) */
	};
	int bad = 0;

	if (argc != 2) {
		fprintf(stderr, "usage: shaderdemo OUT_DIR\n");
		return 2;
	}
	draw_scene();
	for (size_t d = 0; d < sizeof DEV / sizeof DEV[0]; d++) {
		char dir[512];
		const v2 scale = { (float)DEV[d].pw / SW, (float)DEV[d].ph / SH };

		g_cpu = DEV[d].cpu;
		cpu_map(g_cx, DEV[d].pw, SW);
		cpu_map(g_cy, DEV[d].ph, SH);

		snprintf(dir, sizeof dir, "%s/%s", argv[1], DEV[d].id);
		mkdir(argv[1], 0755);
		mkdir(dir, 0755);
		for (int f = 0; f < F_COUNT; f++) {
			if (f >= F_CRT && !DEV[d].gpu) continue;
			bad |= save(argv[1], DEV[d].id, (filt)f, 100, scale, DEV[d].cw, DEV[d].ch);
			if (f >= F_CRT)
				for (int s = 10; s < 100; s += 10) bad |= save(argv[1], DEV[d].id, (filt)f, s, scale, DEV[d].cw, DEV[d].ch);
		}
	}
	return bad;
}
