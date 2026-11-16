/* The card of a game with no art (src/cardart.h), without a screen.
 *
 * What it holds:
 * - each system's era texture, from its tag;
 * - a name's ground is its own and always the same (the hash), and a grey
 *   system still gets a color;
 * - the name reads: near-white on the band over the brightest ground any
 *   system color and any name can make, at DESIGN.md's 4.5:1 (WCAG 2), for
 *   every system in config/systems.cfg;
 * - how a name breaks: one size on every card, smaller only for a word wider
 *   than the box or a name past four lines, balanced lines, never a short
 *   word alone at the end, " - " splitting the series from the rest, and an
 *   ellipsis only when something was cut.
 *
 * Widths are faked (a byte is 0.55 of the size, a space 0.3), so it holds
 * the rules, not Lexend's metrics. Links src/cardart.c and NOT SDL.
 */
#include "../src/cardart.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;

static void ck(int cond, const char *what)
{
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

static int fake_width(void *ctx, int size, const char *s)
{
	float w = 0;

	(void)ctx;
	for (; *s; s++) w += *s == ' ' ? size * .30f : ((unsigned char)*s & 0xC0) == 0x80 ? 0 : size * .55f;
	return (int)lroundf(w);
}

static double lin(double c)
{
	c /= 255.0;
	return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

static double lum(unsigned rgb)
{
	return 0.2126 * lin((rgb >> 16) & 255) + 0.7152 * lin((rgb >> 8) & 255) + 0.0722 * lin(rgb & 255);
}

static unsigned mixc(unsigned a, unsigned b, double t)
{
	unsigned out = 0;

	for (int sh = 16; sh >= 0; sh -= 8) {
		double x = ((a >> sh) & 255) + (((double)((b >> sh) & 255)) - ((a >> sh) & 255)) * t;
		out |= (unsigned)lround(x) << sh;
	}
	return out;
}

static unsigned scalec(unsigned a, double k)
{
	unsigned out = 0;

	for (int sh = 16; sh >= 0; sh -= 8) {
		double x = ((a >> sh) & 255) * k;
		out |= (unsigned)lround(x > 255 ? 255 : x) << sh;
	}
	return out;
}

/* The brightest the ground under the band can be (src/cardart_draw.c): an
 * end of the gradient, lit by the glow, under the shape, under the system
 * art's 13% of white, then the band's 0.48 (its middle; the feather is
 * outside the name's rows). Shards lift a facet by up to 14%. */
static unsigned brightest(unsigned sys, const ca_ground *g)
{
	unsigned glow = mixc(sys, 0xFFFFFF, .3), worst = 0;
	unsigned ends[2] = { g->c0, g->c1 };

	for (int i = 0; i < 2; i++) {
		unsigned c = scalec(mixc(ends[i], glow, .22), 1.14);
		c = mixc(c, g->shape_rgb, .34);
		c = mixc(c, 0xFFFFFF, .13);
		c = scalec(c, .48);
		if (lum(c) > lum(worst)) worst = c;
	}
	return worst;
}

#define NAME_INK 0xF6F5F1u   /* src/ui.c's name color */

static void systems_contrast(void)
{
	FILE *f = fopen("config/systems.cfg", "r");
	char line[1024];
	int systems = 0;
	double least = 99;

	ck(f != NULL, "config/systems.cfg opens (run from the repository root)");
	if (!f) return;
	while (fgets(line, sizeof line, f)) {
		char *field[8] = { 0 }, *p = line;
		unsigned sys;

		if (strncmp(line, "sys|", 4)) continue;
		for (int i = 0; i < 8 && p; i++) {
			field[i] = p;
			p = strchr(p, '|');
			if (p) *p++ = '\0';
		}
		if (!field[6] || !*field[6]) continue;
		sys = (unsigned)strtoul(field[6], NULL, 16);
		systems++;
		for (int n = 0; n < 400; n++) {
			char name[32];
			ca_ground g;
			double hi, lo, ratio;

			snprintf(name, sizeof name, "Game %d", n);
			cardart_ground(sys, cardart_hash(name), &g);
			hi = lum(NAME_INK) + 0.05;
			lo = lum(brightest(sys, &g)) + 0.05;
			ratio = hi / lo;
			if (ratio < least) least = ratio;
			if (ratio < 4.5) {
				printf("  FAIL %s (%06X): \"%s\" reads at %.2f:1\n", field[1], sys, name, ratio);
				fails++;
				break;
			}
		}
	}
	fclose(f);
	ck(systems > 30, "systems.cfg has its systems");
	printf("  name on the band, least contrast over %d systems x 400 names: %.2f:1\n", systems, least);
}

static double saturation(unsigned c)
{
	double r = ((c >> 16) & 255) / 255.0, g = ((c >> 8) & 255) / 255.0, b = (c & 255) / 255.0;
	double mx = fmax(r, fmax(g, b)), mn = fmin(r, fmin(g, b));

	return mx > 0 ? (mx - mn) / mx : 0;
}

static ca_name brk(const char *s)
{
	ca_name n;

	ck(cardart_name(s, 424, CA_LINES, fake_width, NULL, &n), s);
	return n;
}

static void names(void)
{
	ca_name n;
	char longword[64];

	/* the original two: one line at the one size */
	n = brk("Zeta Run");
	ck(n.size == CA_SIZE && n.n == 1 && !strcmp(n.line[0], "Zeta Run") && n.sub_n == 0, "Zeta Run: one line at 60");
	n = brk("DuckTales");
	ck(n.size == CA_SIZE && n.n == 1, "DuckTales: one line at 60");
	n = brk("Mega Man 2");
	ck(n.size == CA_SIZE && n.n == 1, "Mega Man 2: one line at 60");

	/* never a short word alone at the end, and still the one size */
	n = brk("Sonic The Hedgehog 2");
	ck(n.size == CA_SIZE, "Sonic The Hedgehog 2: at 60");
	ck(n.n >= 2 && strlen(n.line[n.n - 1]) > 3, "Sonic The Hedgehog 2: no lone \"2\"");
	/* a narrow box: "Hedgehog 2" fits only at the next size, which it takes */
	ck(cardart_name("Sonic The Hedgehog 2", 300, CA_LINES, fake_width, NULL, &n) && n.size == 50 &&
	   !strcmp(n.line[n.n - 1], "Hedgehog 2"), "a narrow box: the next size, not a lone \"2\"");
	n = brk("Final Fantasy Legend II");
	ck(n.size == CA_SIZE && strlen(n.line[n.n - 1]) > 3, "Final Fantasy Legend II: no lone \"II\"");

	/* the series, and the rest under it */
	n = brk("Castlevania - Symphony of the Night");
	ck(n.n == 1 && !strcmp(n.line[0], "Castlevania"), "Castlevania: the series alone on top");
	ck(n.sub_n >= 1 && n.sub_size == CA_SIZE * 6 / 10, "Castlevania: the rest under it at 36");
	ck(!strstr(n.sub[0], "-"), "Castlevania: no dash carried down");
	n = brk("- Lead Dash");
	ck(n.sub_n == 0, "a dash at the start splits nothing");
	n = brk("Trailing - ");
	ck(n.sub_n == 0, "a dash at the end splits nothing");

	/* balanced: the narrowest box that keeps the count */
	/* greedy: "Phantasy Star" / "Online"; balanced: "Phantasy" / "Star Online" */
	n = brk("Phantasy Star Online");
	ck(n.n == 2 && !strcmp(n.line[0], "Phantasy"), "Phantasy Star Online: balanced lines");

	/* smaller only for a word wider than the box */
	n = brk("Supercalifragil Racer");   /* 15 letters: 495 at 60, 413 at 50 */
	ck(n.size == 50, "a word too wide at 60 steps to 50");
	n = brk("Superextracalifragi");   /* 19 letters: 523 at 50, 439 at 42: cut */
	ck(n.size == 42 && n.n == 1 && strstr(n.line[0], "\xE2\x80\xA6"), "a word too wide at 42 is cut with an ellipsis");

	/* more than four lines at 60: a smaller size, never a fifth line */
	n = brk("The Quite Long Adventure Of A Very Long Name That Goes On And On");
	ck(n.n <= CA_LINES && n.size < CA_SIZE, "a five-line name steps down");
	for (int i = 0; i < n.n; i++) ck(!strstr(n.line[i], "\xE2\x80\xA6"), "a name that fits has no ellipsis");

	memset(longword, 'W', 40);
	longword[40] = '\0';
	n = brk(longword);
	ck(n.n == 1 && strstr(n.line[0], "\xE2\x80\xA6") && fake_width(NULL, n.size, n.line[0]) <= 424,
	   "a hopeless word is cut to the box");

	/* a Grid cell holding two lines: a smaller size before a third line */
	ck(cardart_name("The Quite Long Adventure Of A Name", 424, 2, fake_width, NULL, &n) && n.n <= 2,
	   "two lines at most where the cell holds two");
	ck(!cardart_name("", 424, CA_LINES, fake_width, NULL, &n), "an empty name is no card's");
	ck(!cardart_name("   ", 424, CA_LINES, fake_width, NULL, &n), "a blank name is no card's");
}

int main(void)
{
	ca_ground a, b;
	static const struct { const char *tag; ca_texture t; } tex[] = {
		{ "NES", CA_DITHER }, { "SMS", CA_DITHER }, { "PCE", CA_DITHER }, { "PICO8", CA_DITHER },
		{ "MD", CA_SCAN }, { "SFC", CA_SCAN }, { "NEOGEO", CA_SCAN }, { "ARCADE", CA_SCAN }, { "md", CA_SCAN },
		{ "PS1", CA_SHARDS }, { "N64", CA_SHARDS }, { "DC", CA_SHARDS }, { "PSP", CA_SHARDS },
		{ "GB", CA_LCD }, { "GBA", CA_LCD }, { "GG", CA_LCD }, { "NDS", CA_LCD },
		{ "PRT", CA_SHARDS }, { "", CA_SHARDS },
	};

	printf("cardart:\n");
	for (size_t i = 0; i < sizeof tex / sizeof *tex; i++) {
		char what[64];
		snprintf(what, sizeof what, "%s's texture", tex[i].tag);
		ck(cardart_texture(tex[i].tag) == tex[i].t, what);
	}
	ck(cardart_texture(NULL) == CA_SHARDS, "no tag: shards");

	/* the hash, FNV-1a: these values are on every card already made */
	ck(cardart_hash("Zeta Run") == 0xfb3d7212u, "the hash of Zeta Run");
	ck(cardart_hash("DuckTales") == 0x6a9a09d5u, "the hash of DuckTales");
	ck(cardart_hash("") != 0 && cardart_hash(NULL) != 0, "the hash is never 0");

	cardart_ground(0x3E82D6, cardart_hash("Zeta Run"), &a);
	cardart_ground(0x3E82D6, cardart_hash("Zeta Run"), &b);
	ck(!memcmp(&a, &b, sizeof a), "a name's ground is always the same");
	{
		int differ = 0;
		for (int n = 0; n < 12; n++) {
			char name[16];
			snprintf(name, sizeof name, "Game %d", n);
			cardart_ground(0x3E82D6, cardart_hash(name), &b);
			differ += a.c1 != b.c1 || a.shape != b.shape || a.angle != b.angle;
		}
		ck(differ >= 10, "names differ in their grounds");
	}
	/* PlayStation's grey still gets a color */
	for (int n = 0; n < 50; n++) {
		char name[16];
		snprintf(name, sizeof name, "Game %d", n);
		cardart_ground(0xB4B1AA, cardart_hash(name), &b);
		if (saturation(b.c1) < .2) { ck(0, "a grey system's ground has a color"); break; }
		ck(b.art_corner >= 0 && b.art_corner <= 2 && b.shape >= 0 && b.shape <= 2, "the ground's picks are in range");
	}

	systems_contrast();
	names();
	printf(fails ? "cardart: %d FAILED\n" : "cardart: ok\n", fails);
	return fails ? 1 : 0;
}
