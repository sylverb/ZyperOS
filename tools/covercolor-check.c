/* SPDX-License-Identifier: GPL-3.0-only */
/* An album cover's main color (src/covercolor.h) on covers made here: one
 * color is that color; a white sleeve with a blue title is blue, not white;
 * the larger of two vivid colors wins; a grey photograph is its grey; a
 * transparent picture is mid grey. And darkened for the vinyl: its hue kept,
 * no lighter than asked, its saturation held.
 *
 *   make check-covercolor
 */
#include "../src/covercolor.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define W 300
#define H 300

static uint32_t px[W * H];
static int fails;

static void fill(uint32_t c, int x0, int y0, int x1, int y1)
{
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++) px[y * W + x] = c;
}

static int dist(unsigned a, unsigned b)
{
	int dr = (int)((a >> 16) & 255) - (int)((b >> 16) & 255);
	int dg = (int)((a >> 8) & 255) - (int)((b >> 8) & 255);
	int db = (int)(a & 255) - (int)(b & 255);

	return abs(dr) + abs(dg) + abs(db);
}

static void expect(const char *what, unsigned want, int tol)
{
	unsigned got = cover_main_rgb(px, W, H, W * 4);

	if (dist(got, want) > tol) {
		printf("FAIL: %s: #%06x, not near #%06x\n", what, got, want);
		fails++;
	}
}

static float lightness(unsigned c)
{
	float r = ((c >> 16) & 255) / 255.0f, g = ((c >> 8) & 255) / 255.0f, b = (c & 255) / 255.0f;

	return (fmaxf(r, fmaxf(g, b)) + fminf(r, fminf(g, b))) / 2;
}

int main(void)
{
	unsigned d;

	fill(0xFFD03020, 0, 0, W, H);
	expect("one red", 0xD03020, 6);

	fill(0xFFF4F2EE, 0, 0, W, H);
	fill(0xFF2050C8, 40, 60, 260, 120);          /* a blue title, 15% of it */
	expect("a white sleeve with a blue title", 0x2050C8, 12);

	fill(0xFF7A3FB0, 0, 0, W, H);                /* purple 60%, orange 40% */
	fill(0xFFF08A20, 0, 180, W, H);
	expect("the larger of two colors", 0x7A3FB0, 12);

	for (int y = 0; y < H; y++)
		fill(0xFF000000u | (unsigned)(y * 255 / H) * 0x010101u, 0, y, W, y + 1);
	expect("a grey photograph", 0x7F7F7F, 12);

	fill(0x00FF0000, 0, 0, W, H);
	expect("a transparent picture", 0x808080, 0);

	d = cover_rgb_darken(0xD03020, 0.22f, 0.55f);
	if (lightness(d) > 0.225f || ((d >> 16) & 255) <= (d & 255)) {
		printf("FAIL: red darkened: #%06x (lightness %.3f)\n", d, lightness(d));
		fails++;
	}
	d = cover_rgb_darken(0x7F7F7F, 0.22f, 0.55f);
	if (dist(d, 0x383838) > 6) {
		printf("FAIL: grey darkened: #%06x, not a grey\n", d);
		fails++;
	}
	d = cover_rgb_darken(0x101010, 0.22f, 0.55f);
	if (d != 0x101010) {
		printf("FAIL: a black already darker is lightened: #%06x\n", d);
		fails++;
	}

	if (fails) {
		printf("covercolor-check: %d failed\n", fails);
		return 1;
	}
	printf("covercolor-check: one color, a title on white, the larger of two, grey, transparent; darkened for the vinyl\n");
	return 0;
}
