/* SPDX-License-Identifier: GPL-3.0-only */
#include "cardart.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

const int CA_SIZE_STEPS[CA_SIZES] = { 60, 50, 42 };

static bool in(const char *tag, const char *const *l)
{
	for (; *l; l++)
		if (!strcasecmp(tag, *l)) return true;
	return false;
}

ca_texture cardart_texture(const char *tag)
{
	static const char *const dither[] = { "NES", "SMS", "SG1000", "PCE", "A2600", "A5200", "A7800",
	                                      "COLECO", "MSX2", "PICO8", "TIC80", NULL };
	static const char *const scan[] = { "MD", "SFC", "SEGACD", "32X", "NEOGEO", "NEOCD", "ARCADE",
	                                    "MAME", "VECTREX", "DOOM", "SCUMMVM", "OPENBOR", NULL };
	static const char *const lcd[] = { "GB", "GBC", "GBA", "GG", "GW", "LYNX", "NGP", "NGPC", "WS",
	                                   "WSC", "VB", "PKMINI", "NDS", "J2ME", NULL };

	if (!tag) return CA_SHARDS;
	if (in(tag, dither)) return CA_DITHER;
	if (in(tag, scan)) return CA_SCAN;
	if (in(tag, lcd)) return CA_LCD;
	return CA_SHARDS;
}

uint32_t cardart_hash(const char *name)
{
	uint32_t h = 2166136261u;

	for (; name && *name; name++) {
		h ^= (uint8_t)*name;
		h *= 16777619u;
	}
	return h ? h : 1;
}

static uint32_t next(uint32_t *st)
{
	*st ^= *st << 13;
	*st ^= *st >> 17;
	*st ^= *st << 5;
	return *st;
}

static float unit(uint32_t *st) { return (float)(next(st) & 0xffffff) / 16777216.f; }

typedef struct { float r, g, b; } rgbf;

static rgbf of(unsigned c)
{
	return (rgbf){ ((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f, (c & 255) / 255.f };
}

static float clamp1(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static unsigned to(rgbf c)
{
	return ((unsigned)lroundf(clamp1(c.r) * 255) << 16) | ((unsigned)lroundf(clamp1(c.g) * 255) << 8) |
	       (unsigned)lroundf(clamp1(c.b) * 255);
}

static rgbf mix(rgbf a, rgbf b, float t)
{
	return (rgbf){ a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

static rgbf scale(rgbf a, float k) { return (rgbf){ a.r * k, a.g * k, a.b * k }; }

static rgbf hue(rgbf c, float deg)
{
	float mx = fmaxf(c.r, fmaxf(c.g, c.b)), mn = fminf(c.r, fminf(c.g, c.b)), d = mx - mn;
	float h = 0, s = mx > 0 ? d / mx : 0, v = mx, f, p, q, t;
	int i;

	if (d > 0) {
		if (mx == c.r) h = fmodf((c.g - c.b) / d, 6);
		else if (mx == c.g) h = (c.b - c.r) / d + 2;
		else h = (c.r - c.g) / d + 4;
	}
	h = fmodf(h * 60 + deg + 720, 360) / 60;
	i = (int)h;
	f = h - (float)i;
	p = v * (1 - s);
	q = v * (1 - s * f);
	t = v * (1 - s * (1 - f));
	switch (i % 6) {
	case 0: return (rgbf){ v, t, p };
	case 1: return (rgbf){ q, v, p };
	case 2: return (rgbf){ p, v, t };
	case 3: return (rgbf){ p, q, v };
	case 4: return (rgbf){ t, p, v };
	default: return (rgbf){ v, p, q };
	}
}

void cardart_ground(unsigned sys_rgb, uint32_t hash, ca_ground *g)
{
	static const float turns[] = { -64, -34, 34, 64, 180 };
	static const float angles[] = { 0, 28, -28, 55, -55 };
	/* for a grey: a blue, a teal, a plum and a bronze, all deep */
	static const unsigned tints[] = { 0x2F4C8F, 0x1F7A75, 0x7A3E8F, 0x8F5A2F };
	rgbf sys = of(sys_rgb);
	uint32_t st = hash;
	float mx = fmaxf(sys.r, fmaxf(sys.g, sys.b)), mn = fminf(sys.r, fminf(sys.g, sys.b));

	g->c0 = to(scale(hue(sys, (unit(&st) - .5f) * 30), .70f));
	g->c1 = to(scale(hue(sys, turns[next(&st) % 5]), .46f));
	if (mx <= 0 || (mx - mn) / mx < .15f)
		g->c1 = to(scale(mix(sys, of(tints[hash % 4]), .65f), .55f));
	g->angle = angles[next(&st) % 5] * 3.14159265f / 180;
	g->glow_x = .15f + .7f * unit(&st);
	g->glow_y = .12f + .4f * unit(&st);

	st = hash * 747796405u + 2891336453u;
	if (!st) st = 1;
	g->shape = (int)(next(&st) % 3);
	g->shape_angle = (unit(&st) - .5f) * 1.6f;
	g->shape_x = .2f + .6f * unit(&st);
	g->shape_y = .15f + .7f * unit(&st);
	g->shape_r = .30f + .22f * unit(&st);
	g->shape_rgb = to(mix(hue(sys, (unit(&st) - .5f) * 90), (rgbf){ 1, 1, 1 }, .35f));
	g->art_corner = (int)((hash >> 7) % 3);
	g->seed = hash * 2654435761u;
	if (!g->seed) g->seed = 1;
}

/* ---- the name ---- */

/* Greedy lines of `text` that fit `box_w`: their number, -1 when one word
 * alone is wider, or max + 1 when they run past `max` */
static int wrap(const char *text, int size, int box_w, int max, ca_width_fn width, void *ctx,
                char lines[][CA_LINE])
{
	char word[96], trial[CA_LINE * 2];
	const char *p = text;
	int n = 0;

	lines[0][0] = '\0';
	while (*p) {
		size_t w = 0;

		while (*p == ' ') p++;
		while (*p && *p != ' ' && w + 1 < sizeof word) word[w++] = *p++;
		while (*p && *p != ' ') p++;
		word[w] = '\0';
		if (!w) break;
		if (width(ctx, size, word) > box_w) return -1;
		if (!lines[n][0]) {
			snprintf(lines[n], CA_LINE, "%s", word);
			continue;
		}
		snprintf(trial, sizeof trial, "%s %s", lines[n], word);
		if (strlen(trial) < CA_LINE && width(ctx, size, trial) <= box_w) {
			snprintf(lines[n], CA_LINE, "%s", trial);
			continue;
		}
		if (++n >= max) return max + 1;
		snprintf(lines[n], CA_LINE, "%s", word);
	}
	return lines[0][0] ? n + 1 : 0;
}

/* `text` in at most `max` lines at `size`, balanced, with no short word
 * alone at the end: their number, or -1 when it does not fit */
static int lines_at(const char *text, int size, int box_w, int max, ca_width_fn width, void *ctx,
                    char out[][CA_LINE])
{
	char t[CA_LINES][CA_LINE];
	int n = wrap(text, size, box_w, max, width, ctx, out);

	if (n <= 0 || n > max) return -1;
	for (int bw = box_w - 8; n > 1 && bw > 40; bw -= 8) {
		if (wrap(text, size, bw, max, width, ctx, t) != n) break;
		memcpy(out, t, sizeof(char[CA_LINE]) * (size_t)n);
	}
	if (n > 1 && strlen(out[n - 1]) <= 3) {
		for (int bw = box_w - 8; bw > 40; bw -= 8) {
			int m = wrap(text, size, bw, max, width, ctx, t);

			if (m <= 0 || m > max) break;
			if (strlen(t[m - 1]) > 3) {
				memcpy(out, t, sizeof(char[CA_LINE]) * (size_t)m);
				return m;
			}
		}
		/* no word could go down to it: the next size */
		return -1;
	}
	return n;
}

/* The smallest size's lines, cut to `max` with an ellipsis that fits */
static int cut(const char *text, int size, int box_w, int max, ca_width_fn width, void *ctx,
               char out[][CA_LINE])
{
	char t[CA_LINES + 8][CA_LINE];
	int n = wrap(text, size, box_w, CA_LINES + 7, width, ctx, t);
	bool short_of = n < 0 || n > max;

	if (n < 0) {
		/* a word wider than the box even here: the name as one line, cut */
		snprintf(t[0], CA_LINE, "%s", text);
		n = 1;
	}
	if (n > max) n = max;
	for (int i = 0; i < n; i++) snprintf(out[i], CA_LINE, "%s", t[i]);
	if (short_of) {
		char *last = out[n - 1];
		size_t len = strlen(last);
		char trial[CA_LINE + 4];

		for (;;) {
			snprintf(trial, sizeof trial, "%.*s\xE2\x80\xA6", (int)len, last);
			if (width(ctx, size, trial) <= box_w || len == 0) break;
			/* back off a whole UTF-8 character */
			do len--; while (len > 0 && ((unsigned char)last[len] & 0xC0) == 0x80);
		}
		snprintf(out[n - 1], CA_LINE, "%s", trial);
	}
	return n;
}

bool cardart_name(const char *name, int box_w, int max_lines, ca_width_fn width, void *ctx,
                  ca_name *out)
{
	char main_t[CA_LINE * 2], sub_t[CA_LINE * 2];
	const char *dash;
	int k;

	memset(out, 0, sizeof *out);
	if (max_lines < 1) max_lines = 1;
	if (max_lines > CA_LINES) max_lines = CA_LINES;
	if (!name) return false;
	while (*name == ' ') name++;
	if (!*name) return false;
	snprintf(main_t, sizeof main_t, "%s", name);
	sub_t[0] = '\0';
	/* "Castlevania - Symphony of the Night": the series, and the rest under it */
	if ((dash = strstr(main_t, " - ")) && dash > main_t && dash[3]) {
		snprintf(sub_t, sizeof sub_t, "%s", dash + 3);
		main_t[dash - main_t] = '\0';
	}
	for (k = 0; k < CA_SIZES; k++) {
		int size = CA_SIZE_STEPS[k], sub = size * 6 / 10;
		int n = lines_at(main_t, size, box_w, max_lines, width, ctx, out->line);
		int sn = sub_t[0] ? lines_at(sub_t, sub, box_w, CA_SUB_LINES, width, ctx, out->sub) : 0;

		if (n > 0 && sn >= 0) {
			out->size = size;
			out->sub_size = sub;
			out->n = n;
			out->sub_n = sn;
			return true;
		}
	}
	/* Nothing fit even at the smallest: its lines, cut */
	k = CA_SIZES - 1;
	out->size = CA_SIZE_STEPS[k];
	out->sub_size = out->size * 6 / 10;
	out->n = cut(main_t, out->size, box_w, max_lines, width, ctx, out->line);
	out->sub_n = sub_t[0] ? cut(sub_t, out->sub_size, box_w, CA_SUB_LINES, width, ctx, out->sub) : 0;
	return true;
}
