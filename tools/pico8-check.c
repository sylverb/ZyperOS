/* Your PICO-8 (src/pico8.c): the zip's rules (its version, the AArch64
 * program, which files it gives), a zip copied into Bios/PICO-8 by hand
 * taken out (zips this check writes itself, with made-up bytes: never
 * Lexaloffle's files), Splore's entry on the shelf, and the page's words.
 * And its picture (src/p8draw.h): every Display mode's rect is the one
 * Diatom gives a 128 x 128 picture (diatom/src/scale.c), and
 * sd/pixel2/PICO-8.sh hands that rect and filter to pico8_64 - the script
 * run as it is, on a card in a scratch folder, with a stand-in pico8_64
 * that writes down its arguments - on the Pixel 2's turned panel and on a
 * landscape one, with NEG_OK as it ships and turned to 0. No SDL, no device.
 *
 *     make check-pico8 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include <zlib.h>

#include "../src/i18n.h"
#include "../src/pico8.h"
#include "../src/p8draw.h"
#include "diatom.h"

static int fails;

#define CHECK(cond, what) do { if (!(cond)) { printf("  FAIL %s\n", what); fails++; } } while (0)

/* ---------- a zip, written here ------------------------------------------- */

typedef struct { const char *name; const unsigned char *data; size_t n; bool deflate; } zfile;

static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); }
static void put32(FILE *f, uint32_t v) { put16(f, v & 0xffff); put16(f, v >> 16); }

/* A zip of `n` files at `path`: stored, or raw-deflated as a zip has it */
static void zip_write(const char *path, const zfile *z, int n)
{
	FILE *f = fopen(path, "wb");
	uint32_t off[8], csz[8], crc[8];
	unsigned char *cd[8];
	long cd_at, cd_end;
	int i;

	for (i = 0; i < n; i++) {
		size_t cap = compressBound(z[i].n) + 64, out = z[i].n;
		unsigned char *buf = malloc(cap);

		crc[i] = (uint32_t)crc32(0, z[i].data, (uInt)z[i].n);
		if (z[i].deflate) {
			z_stream s;

			memset(&s, 0, sizeof s);
			deflateInit2(&s, 9, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
			s.next_in = (unsigned char *)z[i].data;
			s.avail_in = (uInt)z[i].n;
			s.next_out = buf;
			s.avail_out = (uInt)cap;
			deflate(&s, Z_FINISH);
			out = s.total_out;
			deflateEnd(&s);
		} else {
			memcpy(buf, z[i].data, z[i].n);
		}
		cd[i] = buf;
		csz[i] = (uint32_t)out;
		off[i] = (uint32_t)ftell(f);
		put32(f, 0x04034b50); put16(f, 20); put16(f, 0); put16(f, z[i].deflate ? 8 : 0);
		put16(f, 0); put16(f, 0); put32(f, crc[i]); put32(f, csz[i]); put32(f, (uint32_t)z[i].n);
		put16(f, (unsigned)strlen(z[i].name)); put16(f, 0);
		fwrite(z[i].name, 1, strlen(z[i].name), f);
		fwrite(buf, 1, out, f);
	}
	cd_at = ftell(f);
	for (i = 0; i < n; i++) {
		put32(f, 0x02014b50); put16(f, 20); put16(f, 20); put16(f, 0); put16(f, z[i].deflate ? 8 : 0);
		put16(f, 0); put16(f, 0); put32(f, crc[i]); put32(f, csz[i]); put32(f, (uint32_t)z[i].n);
		put16(f, (unsigned)strlen(z[i].name)); put16(f, 0); put16(f, 0); put16(f, 0); put16(f, 0);
		put32(f, 0); put32(f, off[i]);
		fwrite(z[i].name, 1, strlen(z[i].name), f);
		free(cd[i]);
	}
	cd_end = ftell(f);
	put32(f, 0x06054b50); put16(f, 0); put16(f, 0); put16(f, (unsigned)n); put16(f, (unsigned)n);
	put32(f, (uint32_t)(cd_end - cd_at)); put32(f, (uint32_t)cd_at); put16(f, 0);
	fclose(f);
}

/* A made-up ELF header of `machine`, then filler: not a program anyone runs */
static unsigned char *fake_elf(unsigned machine, size_t n)
{
	unsigned char *b = calloc(1, n);
	size_t i;

	memcpy(b, "\x7f" "ELF", 4);
	b[4] = 2; b[5] = 1; b[6] = 1;
	b[18] = machine & 255; b[19] = machine >> 8;
	for (i = 64; i < n; i++) b[i] = (unsigned char)(i * 7);
	return b;
}

static bool file_is(const char *path, const unsigned char *data, size_t n)
{
	FILE *f = fopen(path, "rb");
	unsigned char *b;
	bool ok;

	if (!f) return false;
	b = malloc(n + 1);
	ok = fread(b, 1, n + 1, f) == n && !memcmp(b, data, n);
	free(b);
	fclose(f);
	return ok;
}

static bool exists(const char *path) { return access(path, F_OK) == 0; }

static void rmtree(const char *dir)
{
	char cmd[512];

	snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
	if (system(cmd) != 0) printf("  (could not remove %s)\n", dir);
}


/* ---------- the picture: PICO-8.sh's flags, mode by mode ------------------ */

/* The script at `script`, a copy of PICO-8.sh with the card and the DRM
 * folder in `root`, run for one Display word: its -draw_rect and
 * -pixel_perfect, as the stand-in pico8_64 wrote them down */
static bool script_flags(const char *root, const char *script, const char *display,
                         dmode_rect *r, int *pp)
{
	char cmd[1024], line[256], args[512];
	FILE *f;
	bool got_r = false, got_p = false, next_r = false, next_p = false;

	snprintf(args, sizeof args, "%s/args", root);
	unlink(args);
	snprintf(cmd, sizeof cmd, "cd '%s' && PATH='%s/bin':\"$PATH\" ZYPEROS_DISPLAY='%s' P8_ARGS='%s' "
	         "sh '%s' '%s/card/Roms/PICO-8/x.p8' > '%s/out' 2>&1",
	         root, root, display, args, script, root, root);
	if (system(cmd) != 0 || !(f = fopen(args, "r"))) return false;
	while (fgets(line, sizeof line, f)) {
		line[strcspn(line, "\n")] = '\0';
		if (next_r) got_r = sscanf(line, "%d,%d,%d,%d", &r->x, &r->y, &r->w, &r->h) == 4;
		if (next_p) got_p = sscanf(line, "%d", pp) == 1;
		next_r = !strcmp(line, "-draw_rect");
		next_p = !strcmp(line, "-pixel_perfect");
	}
	fclose(f);
	return got_r && got_p;
}

static void put_file(const char *path, const char *text, bool run)
{
	FILE *f = fopen(path, "w");

	if (!f) return;
	fputs(text, f);
	fclose(f);
	if (run) chmod(path, 0755);
}

/* PICO-8.sh with the card's and the DRM folder's paths moved into `root`,
 * and with `neg` for NEG_OK; false when the script no longer says either */
static bool script_copy(const char *root, const char *out, const char *neg)
{
	FILE *in = fopen("sd/pixel2/PICO-8.sh", "r"), *o = fopen(out, "w");
	char line[1024];
	bool card = false, drm = false, negs = false;

	while (in && o && fgets(line, sizeof line, in)) {
		char *at;

		if (!strcmp(line, "CARD=/mnt/SDCARD\n")) {
			fprintf(o, "CARD='%s/card'\n", root);
			card = true;
		} else if ((at = strstr(line, "/sys/class/drm/"))) {
			fprintf(o, "%.*s%s/drm/%s", (int)(at - line), line, root, at + strlen("/sys/class/drm/"));
			drm = true;
		} else if (!strncmp(line, "NEG_OK=", 7)) {
			if (neg) fprintf(o, "NEG_OK=%s\n", neg);
			else fputs(line, o);
			negs = true;
		} else {
			fputs(line, o);
		}
	}
	if (in) fclose(in);
	if (o) fclose(o);
	return card && drm && negs;
}

static void display_flags(void)
{
	char root[] = "/tmp/zyperos-p8d-XXXXXX", p[512], text[512], script[512];
	/* The panel as the DRM folder says, and the landscape window PICO-8
	 * gets of it (SDL turns a portrait panel) */
	static const struct { const char *mode; int w, h; } PANELS[] = {
		{ "480x640", 640, 480 },      /* the Pixel 2's, turned */
		{ "1024x768", 1024, 768 },    /* a landscape one */
	};
	static const char *const EXTRA[] = { "", "bogus" };   /* none, and a word no mode has */
	int pn, m, n = 0;

	printf("the picture, mode by mode (src/p8draw.h, sd/pixel2/PICO-8.sh):\n");
	/* p8_draw_rect is Diatom's own rect for a 128 x 128 picture */
	for (pn = 0; pn < 2; pn++)
		for (m = 0; m < diatom_mode_count; m++) {
			dmode_rect r;
			diatom_rect d = diatom_scale_rect(diatom_modes[m].mode, P8_SCREEN, P8_SCREEN, 0.0,
			                                  PANELS[pn].w, PANELS[pn].h);

			p8_draw_rect(diatom_modes[m].name, PANELS[pn].w, PANELS[pn].h, true, &r);
			snprintf(text, sizeof text, "%s on %dx%d is Diatom's rect for 128x128 (%d,%d %dx%d)",
			         diatom_modes[m].name, PANELS[pn].w, PANELS[pn].h, d.x, d.y, d.w, d.h);
			CHECK(r.x == d.x && r.y == d.y && r.w == d.w && r.h == d.h, text);
		}
	/* The table in src/p8draw.h, on the Pixel 2 */
	{
		static const struct { const char *mode; int x, y, w, h, pp; } T[] = {
			{ "stretch", 0, 0, 640, 480, 0 },     { "aspect", 80, 0, 480, 480, 0 },
			{ "square", 80, 0, 480, 480, 0 },     { "integer", 128, 48, 384, 384, 1 },
			{ "integer-vertical", 128, 48, 384, 384, 1 },
			{ "overscale", 0, -80, 640, 640, 1 }, { "fill", 0, -80, 640, 640, 1 },
			{ "native", 256, 176, 128, 128, 1 },  { "bogus", 0, 0, 640, 480, 0 },
		};
		int i;

		for (i = 0; i < (int)(sizeof T / sizeof T[0]); i++) {
			dmode_rect r;
			bool pp = p8_draw_rect(T[i].mode, 640, 480, true, &r);

			snprintf(text, sizeof text, "the Pixel 2's %s: %d,%d %dx%d, %s", T[i].mode, T[i].x, T[i].y,
			         T[i].w, T[i].h, T[i].pp ? "unfiltered" : "filtered");
			CHECK(r.x == T[i].x && r.y == T[i].y && r.w == T[i].w && r.h == T[i].h && pp == T[i].pp, text);
		}
	}
	/* With no negative offset: Zoomed is Sharp Pixels, Edges Cut is Original Shape */
	{
		dmode_rect a, b;

		p8_draw_rect("overscale", 640, 480, false, &a);
		p8_draw_rect("integer", 640, 480, true, &b);
		CHECK(!memcmp(&a, &b, sizeof a) && !strcmp(p8_draw_mode("overscale", false), "integer"),
		      "P8_NEG_OK 0: Zoomed draws as Sharp Pixels");
		p8_draw_rect("fill", 640, 480, false, &a);
		p8_draw_rect("aspect", 640, 480, true, &b);
		CHECK(!memcmp(&a, &b, sizeof a) && !strcmp(p8_draw_mode("fill", false), "aspect"),
		      "P8_NEG_OK 0: Edges Cut draws as Original Shape");
		CHECK(!strcmp(p8_draw_mode("native", false), "native") && !strcmp(p8_draw_mode(NULL, true), "stretch"),
		      "and every other mode is itself, none being Full Screen");
	}

	if (!mkdtemp(root)) {
		CHECK(false, "a scratch folder");
		return;
	}
	/* The card: your PICO-8's two files (pico8_64 a stand-in that writes
	 * its arguments one a line), a cart, and a mount that binds nothing */
	snprintf(p, sizeof p, "mkdir -p '%s/card/Bios/PICO-8' '%s/card/Roms/PICO-8' '%s/bin' '%s/drm/card0-DSI-1'",
	         root, root, root, root);
	CHECK(system(p) == 0, "the scratch card");
	snprintf(p, sizeof p, "%s/card/Bios/PICO-8/pico8_64", root);
	put_file(p, "#!/bin/sh\n: > \"$P8_ARGS\"\nfor a in \"$@\"; do printf '%s\\n' \"$a\" >> \"$P8_ARGS\"; done\n", true);
	snprintf(p, sizeof p, "%s/card/Bios/PICO-8/pico8.dat", root);
	put_file(p, "stand-in", false);
	snprintf(p, sizeof p, "%s/card/Roms/PICO-8/x.p8", root);
	put_file(p, "pico-8 cartridge\n", false);
	snprintf(p, sizeof p, "%s/bin/mount", root);
	put_file(p, "#!/bin/sh\nexit 0\n", true);

	/* NEG_OK as it ships is P8_NEG_OK */
	{
		FILE *f = fopen("sd/pixel2/PICO-8.sh", "r");
		char line[256];
		int ships = -1;

		while (f && fgets(line, sizeof line, f))
			if (!strncmp(line, "NEG_OK=", 7)) ships = atoi(line + 7);
		if (f) fclose(f);
		snprintf(text, sizeof text, "PICO-8.sh's NEG_OK is src/p8draw.h's P8_NEG_OK (%d, %d)", ships, P8_NEG_OK);
		CHECK(ships == P8_NEG_OK, text);
	}
	for (int neg = 0; neg < 2; neg++) {
		snprintf(script, sizeof script, "%s/PICO-8.sh", root);
		CHECK(script_copy(root, script, neg ? NULL : "0"),
		      "PICO-8.sh still has its CARD line, the DRM folder and NEG_OK");
		for (pn = 0; pn < 2; pn++) {
			snprintf(p, sizeof p, "%s/drm/card0-DSI-1/modes", root);
			snprintf(text, sizeof text, "%s\n", PANELS[pn].mode);
			put_file(p, text, false);
			for (m = 0; m < diatom_mode_count + 2; m++) {
				const char *word = m < diatom_mode_count ? diatom_modes[m].name : EXTRA[m - diatom_mode_count];
				bool ok = neg ? P8_NEG_OK : false;
				dmode_rect want, got = { 0, 0, 0, 0 };
				int pp = -1;
				bool wpp = p8_draw_rect(word, PANELS[pn].w, PANELS[pn].h, ok, &want);
				bool ran = script_flags(root, script, word, &got, &pp);

				snprintf(text, sizeof text,
				         "PICO-8.sh, NEG_OK=%d, Display \"%s\" on a %s panel: -draw_rect %d,%d,%d,%d "
				         "-pixel_perfect %d (it said %d,%d,%d,%d and %d)",
				         neg ? P8_NEG_OK : 0, word, PANELS[pn].mode, want.x, want.y, want.w, want.h, wpp,
				         got.x, got.y, got.w, got.h, pp);
				CHECK(ran && !memcmp(&want, &got, sizeof want) && pp == (int)wpp, text);
				n++;
			}
		}
	}
	/* And with no panel the system names: the Pixel 2's 640 x 480 */
	{
		dmode_rect want, got = { 0, 0, 0, 0 };
		int pp = -1;

		snprintf(p, sizeof p, "%s/drm/card0-DSI-1/modes", root);
		unlink(p);
		p8_draw_rect("native", 640, 480, true, &want);
		CHECK(script_flags(root, script, "native", &got, &pp) && !memcmp(&want, &got, sizeof want),
		      "PICO-8.sh with no panel mode to read: 640 x 480");
	}
	printf("  %d runs of PICO-8.sh\n", n);
	rmtree(root);
}

int main(void)
{
	char v[32], zip[128], said[256], card[] = "/tmp/zyperos-p8-XXXXXX", dir[256], p[512];
	const char *manual = "PICO-8 v0.2.7\nhttps://www.pico-8.com\n(c) Copyright 2014-2025 Lexaloffle Games\n";
	unsigned char *prog, *prog32, *x86, data[4096], lic[] = "license text";

	printf("the version:\n");
	CHECK(p8_version_from_name("pico-8_0.2.7_raspi.zip", v, sizeof v) && !strcmp(v, "0.2.7"),
	      "pico-8_0.2.7_raspi.zip is 0.2.7");
	CHECK(p8_version_from_name("/x/y/PICO-8_0.2.6b_raspi.zip", v, sizeof v) && !strcmp(v, "0.2.6b"),
	      "its path and the name's case do not matter, a letter after it does (0.2.6b)");
	CHECK(!p8_version_from_name("raspi.zip", v, sizeof v) && !p8_version_from_name("pico-8_raspi.zip", v, sizeof v),
	      "a name with no version says none");
	CHECK(p8_version_from_manual(manual, strlen(manual), v, sizeof v) && !strcmp(v, "0.2.7"),
	      "the manual's first line: PICO-8 v0.2.7");
	CHECK(p8_version_from_manual("PICO-8 0.2.5g manual", 20, v, sizeof v) && !strcmp(v, "0.2.5g"),
	      "with no v too");
	CHECK(!p8_version_from_manual("PICO-8 is a fantasy console", 27, v, sizeof v),
	      "the name with words after it is no version");

	printf("the program:\n");
	prog = fake_elf(183, 200000);
	prog32 = fake_elf(40, 200000);
	x86 = fake_elf(62, 200000);
	CHECK(p8_elf_aarch64(prog, 200000), "a 64-bit ARM ELF is pico8_64's kind");
	CHECK(!p8_elf_aarch64(prog32, 200000), "a 32-bit ARM one (pico8, pico8_dyn) is not");
	CHECK(!p8_elf_aarch64(x86, 200000), "nor an x86-64 one (the Linux PC zip's)");
	CHECK(!p8_elf_aarch64(prog, 19) && !p8_elf_aarch64((const unsigned char *)"MZ\x90", 3),
	      "nor a few bytes, nor a Windows program");

	printf("the zip's files:\n");
	CHECK(p8_zip_member("pico-8/pico8_64") && !strcmp(p8_zip_member("pico-8/pico8_64"), "pico8_64"),
	      "pico-8/pico8_64 is the program");
	CHECK(p8_zip_member("pico8.dat") && !strcmp(p8_zip_member("pico8.dat"), "pico8.dat"),
	      "pico8.dat at the zip's top too");
	CHECK(p8_zip_member("pico-8/license.txt") && !strcmp(p8_zip_member("pico-8/license.txt"), "license.txt"),
	      "its license goes with it");
	CHECK(p8_zip_member("pico-8/pico-8_manual.txt") && !*p8_zip_member("pico-8/pico-8_manual.txt") &&
	      p8_zip_member("pico-8/pico-8.txt") && !*p8_zip_member("pico-8/pico-8.txt"),
	      "the manual, old name or new, is read for the version only");
	CHECK(!p8_zip_member("pico-8/pico8") && !p8_zip_member("pico-8/pico8_dyn") &&
	      !p8_zip_member("pico-8/lexaloffle-pico8.png") && !p8_zip_member("__MACOSX/pico-8/._pico8_64") &&
	      !p8_zip_member("/pico8_64"),
	      "the 32-bit programs, the icon and a Mac's ._ copies are left");

	printf("Splore's entry:\n");
	{
		game_list l = { 0 };
		game_entry e[3];

		CHECK(splore_listing_add(&l) && l.count == 1 && splore_file(l.items[0].file) &&
		      !strcmp(l.items[0].name, "Splore"),
		      "added to an empty listing, named Splore");
		CHECK(splore_listing_add(&l) && l.count == 1, "never twice");
		free(l.items);
		memset(e, 0, sizeof e);
		snprintf(e[0].file, sizeof e[0].file, "%s", "Alpha.p8");
		snprintf(e[1].file, sizeof e[1].file, "%s", "Beta.p8.png");
		snprintf(e[2].file, sizeof e[2].file, "%s", SPLORE_FILE);
		splore_pin(e, 3);
		CHECK(splore_file(e[0].file) && !strcmp(e[1].file, "Alpha.p8") && !strcmp(e[2].file, "Beta.p8.png"),
		      "first after a sort, the carts in their order");
		CHECK(!splore_file("Splore.p8") && splore_file("splore.SPLORE"),
		      "a cart named Splore is a cart");
	}

	printf("the page:\n");
	{
		char notes[P8_NOTES][P8_NOTE_LEN];
		su_page pg;
		int nn;

		p8_page_build(&pg, notes, &nn);
		CHECK(!strcmp(pg.from, "PICO-8") && !strcmp(pg.sub, "By Lexaloffle · not on this card"),
		      "PICO-8, by Lexaloffle, not on this card");
		CHECK(nn == 3 && strstr(notes[0], "Splore") && strstr(notes[1], "Lexaloffle") &&
		      strstr(notes[2], "ZyperOS Installer") && strstr(notes[2], "Bios/PICO-8"),
		      "what it adds, whose it is, how to add it");
		CHECK(pg.nkeys == 1 && !strcmp(pg.keys[0].button, "B") && pg.keys[0].act == SUA_BACK && !pg.bar,
		      "Back its one key, no bar: nothing here brings PICO-8");
	}

	printf("a zip copied by hand:\n");
	if (!mkdtemp(card)) { perror("mkdtemp"); return 1; }
	snprintf(dir, sizeof dir, "%s/Bios/PICO-8", card);
	CHECK(p8_take_zip(card, v, sizeof v, zip, sizeof zip) == P8_TAKE_NONE, "no Bios/PICO-8: nothing");
	snprintf(p, sizeof p, "%s/Bios", card); mkdir(p, 0755); mkdir(dir, 0755);
	CHECK(p8_take_zip(card, v, sizeof v, zip, sizeof zip) == P8_TAKE_NONE, "no zip in it: nothing");
	memset(data, 'd', sizeof data);
	{
		zfile win[] = { { "pico-8/pico8.exe", x86, 1000, false }, { "pico-8/pico8.dat", data, sizeof data, true } };

		snprintf(p, sizeof p, "%s/pico-8_0.2.7_windows.zip", dir);
		zip_write(p, win, 2);
		CHECK(p8_take_zip(card, v, sizeof v, zip, sizeof zip) == P8_TAKE_NOT_RASPI &&
		      !strcmp(zip, "pico-8_0.2.7_windows.zip"), "the Windows zip is not the Raspberry Pi one");
		p8_take_message(P8_TAKE_NOT_RASPI, v, zip, said, sizeof said);
		CHECK(!strcmp(said, "pico-8_0.2.7_windows.zip is not the PICO-8 for Raspberry Pi zip."),
		      "and says so by name");
		unlink(p);
	}
	{
		zfile pc[] = { { "pico-8/pico8_64", x86, 200000, true }, { "pico-8/pico8.dat", data, sizeof data, false } };

		snprintf(p, sizeof p, "%s/odd.zip", dir);
		zip_write(p, pc, 2);
		CHECK(p8_take_zip(card, v, sizeof v, zip, sizeof zip) == P8_TAKE_NOT_RASPI,
		      "a pico8_64 that is no ARM program is refused");
		snprintf(p, sizeof p, "%s/%s", dir, P8_PROGRAM);
		CHECK(!exists(p), "and nothing is written");
		snprintf(p, sizeof p, "%s/odd.zip", dir);
		unlink(p);
	}
	snprintf(p, sizeof p, "%s/notes.zip", dir);
	{
		FILE *f = fopen(p, "wb");

		fputs("not a zip at all", f);
		fclose(f);
		CHECK(p8_take_zip(card, v, sizeof v, zip, sizeof zip) == P8_TAKE_NOT_RASPI, "nor a file that is no zip");
		unlink(p);
	}
	{
		zfile raspi[] = {
			{ "pico-8/pico8", prog32, 200000, true },
			{ "pico-8/pico8_64", prog, 200000, true },
			{ "pico-8/pico8.dat", data, sizeof data, true },
			{ "pico-8/license.txt", lic, sizeof lic - 1, false },
			{ "pico-8/pico-8_manual.txt", (const unsigned char *)manual, strlen(manual), true },
			{ "__MACOSX/pico-8/._pico8_64", data, 100, false },
		};
		struct stat st;

		snprintf(p, sizeof p, "%s/pico-8_0.2.7_raspi.zip", dir);
		zip_write(p, raspi, 6);
		CHECK(p8_take_zip(card, v, sizeof v, zip, sizeof zip) == P8_TAKE_DONE && !strcmp(v, "0.2.7"),
		      "the Raspberry Pi zip is taken, its version from the manual");
		snprintf(p, sizeof p, "%s/%s", dir, P8_PROGRAM);
		CHECK(file_is(p, prog, 200000), "pico8_64 out whole (deflated in the zip)");
		CHECK(stat(p, &st) == 0 && (st.st_mode & 0111), "and runnable");
		snprintf(p, sizeof p, "%s/%s", dir, P8_DATA);
		CHECK(file_is(p, data, sizeof data), "pico8.dat beside it");
		snprintf(p, sizeof p, "%s/%s", dir, P8_LICENSE);
		CHECK(file_is(p, lic, sizeof lic - 1), "its license (stored in the zip) with it");
		snprintf(p, sizeof p, "%s/%s", dir, P8_VERSION);
		CHECK(file_is(p, (const unsigned char *)"0.2.7\n", 6), "version.txt says 0.2.7");
		snprintf(p, sizeof p, "%s/pico8", dir);
		CHECK(!exists(p), "the 32-bit program is left in the zip");
		snprintf(p, sizeof p, "%s/pico-8_0.2.7_raspi.zip", dir);
		CHECK(exists(p), "the zip stays where the player put it");
		p8_take_message(P8_TAKE_DONE, v, zip, said, sizeof said);
		CHECK(!strcmp(said, "Your PICO-8 0.2.7 is ready."), "Your PICO-8 0.2.7 is ready.");
		CHECK(p8_take_zip(card, v, sizeof v, zip, sizeof zip) == P8_TAKE_NONE,
		      "taken once: the program is as new as the zip");
		/* A newer zip, copied over later: taken again */
		{
			struct utimbuf old = { time(NULL) - 3600, time(NULL) - 3600 };
			char q[512];

			snprintf(q, sizeof q, "%s/%s", dir, P8_PROGRAM);
			utime(q, &old);
			CHECK(p8_take_zip(card, v, sizeof v, zip, sizeof zip) == P8_TAKE_DONE,
			      "a zip newer than the program is taken again");
		}
		unlink(p);
	}
	{
		zfile bare[] = { { "pico8_64", prog, 200000, false }, { "pico8.dat", data, sizeof data, false } };
		struct utimbuf old = { time(NULL) - 3600, time(NULL) - 3600 };

		snprintf(p, sizeof p, "%s/%s", dir, P8_PROGRAM);
		utime(p, &old);
		snprintf(p, sizeof p, "%s/my-pico.zip", dir);
		zip_write(p, bare, 2);
		CHECK(p8_take_zip(card, v, sizeof v, zip, sizeof zip) == P8_TAKE_DONE && !v[0],
		      "a zip of the two files alone, no folder, no version");
		snprintf(p, sizeof p, "%s/%s", dir, P8_VERSION);
		CHECK(!exists(p), "and no version.txt from before stays to say an old one");
		p8_take_message(P8_TAKE_DONE, v, zip, said, sizeof said);
		CHECK(!strcmp(said, "Your PICO-8 is ready."), "Your PICO-8 is ready.");
	}
	p8_take_message(P8_TAKE_NONE, "", "", said, sizeof said);
	CHECK(!said[0], "nothing taken, nothing said");
	rmtree(card);
	free(prog);
	free(prog32);
	free(x86);

	display_flags();

	if (fails) { printf("pico8: %d FAILED\n", fails); return 1; }
	printf("pico8: ok\n");
	return 0;
}
