/* SPDX-License-Identifier: GPL-3.0-only */
/* A game's cheats with no device: the .cht reader (src/cht.h) and the list
 * the in-game menu shows (src/cheats.h), on a card in a folder of its own.
 *
 *     make check-cheats
 *
 * The reader: RetroArch's quoted keys; a player's file unquoted, with CRLF, a
 * byte-order mark, comments and keys out of order; a count that says fewer
 * or more than there are; RetroArch's own memory pokes (handler 1) and
 * cheats with no code left out; a long description cut at a character; a
 * text that is no .cht.
 *
 * The list: the player's own file first, under each name and folder it may
 * have (NextUI's too), each once; libretro's files after it, each a group
 * named for its code type; why a game has none (libretro has none, or not
 * checked yet); a game in a subfolder; which are on, kept as ids that
 * survive a reload, drop a cheat that went and turn off one whose code
 * changed; the codes Diatom is handed; the on-first order. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/cheats.h"
#include "../src/cht.h"

#ifndef CHEATS_TMP
#error "build with -DCHEATS_TMP, a folder of this check's own (the Makefile does)"
#endif

static int failures;

#define CHECK(cond, ...)                                                      \
	do {                                                                      \
		if (!(cond)) {                                                        \
			printf("  FAIL: ");                                               \
			printf(__VA_ARGS__);                                              \
			printf("\n");                                                     \
			failures++;                                                       \
		}                                                                     \
	} while (0)

typedef struct {
	int n;
	char desc[8][CHT_DESC_MAX + 1];
	char code[8][256];
} got;

static void take(const char *desc, const char *code, void *ctx)
{
	got *g = ctx;

	if (g->n >= 8) return;
	snprintf(g->desc[g->n], sizeof g->desc[0], "%s", desc);
	snprintf(g->code[g->n], sizeof g->code[0], "%s", code);
	g->n++;
}

static int parse(const char *text, got *g)
{
	memset(g, 0, sizeof *g);
	return cht_parse(text, strlen(text), take, g);
}

static void reader(void)
{
	got g;
	int r;
	char longd[400];

	r = parse("cheats = 2\n\n"
	          "cheat0_desc = \"Infinite Lives\"\n"
	          "cheat0_code = \"7E0DBE:63\"\n"
	          "cheat0_enable = false\n\n"
	          "cheat1_desc = \"Start on Any World\"\n"
	          "cheat1_code = \"DD62-6F0D+DD6B-6D6D\"\n"
	          "cheat1_enable = true\n", &g);
	CHECK(r == 2 && g.n == 2, "RetroArch's file: 2 cheats, got %d", r);
	CHECK(!strcmp(g.desc[0], "Infinite Lives") && !strcmp(g.code[0], "7E0DBE:63"),
	      "the first is as written: '%s' '%s'", g.desc[0], g.code[0]);
	CHECK(!strcmp(g.code[1], "DD62-6F0D+DD6B-6D6D"), "a code joined with + stays whole: '%s'",
	      g.code[1]);

	r = parse("\xef\xbb\xbf# my own\r\n"
	          "cheat1_code = 01FF-1234\r\n"
	          "  cheat0_code=ABCD\r\n"
	          "cheat0_desc = Max Money Always\r\n"
	          "cheat1_desc =   Moon Jump  \r\n"
	          "cheats = 2\r\n", &g);
	CHECK(r == 2, "a player's file by hand: 2 cheats, got %d", r);
	CHECK(!strcmp(g.desc[0], "Max Money Always") && !strcmp(g.code[0], "ABCD"),
	      "unquoted, CRLF, a byte-order mark and keys out of order: '%s' '%s'",
	      g.desc[0], g.code[0]);
	CHECK(!strcmp(g.desc[1], "Moon Jump") && !strcmp(g.code[1], "01FF-1234"),
	      "the spaces around an unquoted value are not its: '%s'", g.desc[1]);

	r = parse("cheats = 3\n"
	          "cheat0_desc = \"A\"\ncheat0_code = \"1\"\n"
	          "cheat1_desc = \"No code\"\n"
	          "cheat2_desc = \"C\"\ncheat2_code = \"3\"\n", &g);
	CHECK(r == 2 && !strcmp(g.desc[1], "C"), "a cheat with no code is left out: %d, '%s'",
	      r, g.desc[1]);

	r = parse("cheats = 2\n"
	          "cheat0_desc = \"Poke\"\ncheat0_handler = \"1\"\ncheat0_code = \"\"\n"
	          "cheat0_address = \"8000\"\ncheat0_value = \"99\"\n"
	          "cheat1_desc = \"Code\"\ncheat1_handler = \"0\"\ncheat1_code = \"7E0019:02\"\n"
	          "cheat1_address = \"0\"\n", &g);
	CHECK(r == 1 && !strcmp(g.desc[0], "Code"),
	      "RetroArch's own memory poke is left out, the core's code kept: %d '%s'", r, g.desc[0]);

	r = parse("cheat0_code = \"1\"\ncheat1_code = \"2\"\ncheat2_code = \"3\"\n", &g);
	CHECK(r == 3 && !g.desc[0][0], "no cheats line: as far as the last, no desc is \"\": %d", r);

	r = parse("cheats = 1\ncheat0_code = \"1\"\ncheat1_code = \"2\"\n", &g);
	CHECK(r == 1, "a count of fewer is the count: %d", r);

	r = parse("cheats = 9\ncheat0_code = \"1\"\n", &g);
	CHECK(r == 1, "a count of more is as far as there are: %d", r);

	r = parse("cheat5000_code = \"far\"\ncheat0_code = \"near\"\n", &g);
	CHECK(r == 1 && !strcmp(g.code[0], "near"), "a number past CHT_MAX is left out: %d", r);

	/* 127 ASCII bytes and then "é" (two bytes): the cut falls inside it */
	memset(longd, 'a', 127);
	strcpy(longd + 127, "\xc3\xa9 and more words after it");
	{
		char text[600];

		snprintf(text, sizeof text, "cheat0_desc = \"%s\"\ncheat0_code = \"1\"\n", longd);
		parse(text, &g);
	}
	CHECK(strlen(g.desc[0]) == 127, "a long description is cut before a character it would split: %zu",
	      strlen(g.desc[0]));

	CHECK(parse("Super Mario World\nthis is a readme\n", &g) == -1, "a text that is no .cht is -1");
	CHECK(parse("", &g) == -1, "an empty one is -1 too");
}

/* ---- the list, on a card ------------------------------------------------ */

static char card[512];

static void put(const char *rel, const char *text)
{
	char path[1024], dir[1024], *s;

	snprintf(path, sizeof path, "%s/%s", card, rel);
	snprintf(dir, sizeof dir, "%s", path);
	for (s = dir + strlen(card) + 1; (s = strchr(s, '/')); s++) {
		*s = '\0';
		mkdir(dir, 0755);
		*s = '/';
	}
	FILE *f = fopen(path, "w");
	if (!f) { printf("  FAIL: cannot write %s\n", path); failures++; return; }
	fputs(text, f);
	fclose(f);
}

static const char *ONE = "cheats = 1\ncheat0_desc = \"%s\"\ncheat0_code = \"%s\"\n";

static void put1(const char *rel, const char *desc, const char *code)
{
	char text[512];

	snprintf(text, sizeof text, ONE, desc, code);
	put(rel, text);
}

static void list(void)
{
	static cheat_list L, M;
	char state[256], codes[512];
	int order[CHEATS_MAX], n, i;
	FILE *f;

	snprintf(card, sizeof card, "%s/card-%d", CHEATS_TMP, (int)getpid());
	mkdir(CHEATS_TMP, 0755);
	mkdir(card, 0755);

	/* Nothing on the card for it */
	CHECK(!cheats_load(&L, card, "SNES", "SFC", "Kingdom Run.sfc") && L.where == CHEATS_NOT_CHECKED,
	      "no file: none, not checked yet (%d)", L.where);
	cheats_free(&L);

	/* libretro looked, and has none */
	put("Cheats/SNES/.libretro/Kingdom Run/.none", "");
	CHECK(!cheats_load(&L, card, "SNES", "SFC", "Kingdom Run.sfc") && L.where == CHEATS_NONE_LIBRETRO,
	      ".none: libretro has none (%d)", L.where);
	cheats_free(&L);

	/* The player's own, then libretro's two files */
	put1("Cheats/SNES/Kingdom Run.cht", "Mine", "AAAA");
	put("Cheats/SNES/.libretro/Kingdom Run/Kingdom Run (USA).cht",
	    "cheats = 2\ncheat0_desc = \"Infinite Lives\"\ncheat0_code = \"7E0DBE:63\"\n"
	    "cheat1_desc = \"Infinite Coins\"\ncheat1_code = \"7E0DBF:99\"\n");
	put1("Cheats/SNES/.libretro/Kingdom Run/Kingdom Run (USA) (Game Genie).cht",
	     "Start on Any World", "DD62-6F0D");
	put("Cheats/SNES/.libretro/Kingdom Run/notes.txt", "not a cheat file");
	CHECK(cheats_load(&L, card, "SNES", "SFC", "Kingdom Run.sfc") && L.n == 4 && L.ng == 3,
	      "the player's file and libretro's two: 4 cheats in 3 groups, got %d in %d", L.n, L.ng);
	CHECK(L.where == CHEATS_HAVE, "with a file it has some");
	CHECK(L.g[0].yours && !strcmp(L.c[0].desc, "Mine"), "the player's file is first: '%s'", L.c[0].desc);
	CHECK(!L.g[1].yours && !L.g[1].label[0], "libretro's plain file has no code type: '%s'", L.g[1].label);
	CHECK(!strcmp(L.g[2].label, "Game Genie"), "the other is named for its code type: '%s'", L.g[2].label);
	CHECK(L.c[3].group == 2 && !strcmp(L.c[3].code, "DD62-6F0D"), "its cheat is in it");
	for (i = 0; i < L.n; i++) CHECK(!L.c[i].on, "every cheat starts off (%d)", i);

	/* On, kept, and back after a reload */
	L.c[0].on = L.c[2].on = true;
	cheats_state_str(&L, state, sizeof state);
	CHECK(cheats_on_count(&L) == 2, "2 on");
	cheats_free(&L);
	cheats_load(&M, card, "SNES", "SFC", "Kingdom Run.sfc");
	cheats_state_apply(&M, state);
	CHECK(M.c[0].on && !M.c[1].on && M.c[2].on && !M.c[3].on,
	      "the same two are on after a reload (state '%s')", state);

	/* The codes Diatom is handed: the two on, a line each */
	snprintf(codes, sizeof codes, "%s/codes.txt", card);
	CHECK(cheats_write_codes(&M, codes) == 2, "two codes written");
	f = fopen(codes, "r");
	{
		char buf[256] = "";
		size_t got_n = f ? fread(buf, 1, sizeof buf - 1, f) : 0;

		buf[got_n] = '\0';
		CHECK(!strcmp(buf, "AAAA\n7E0DBF:99\n"), "the codes file: '%s'", buf);
	}
	if (f) fclose(f);

	n = cheats_order(&M, true, order);
	CHECK(n == 4 && order[0] == 0 && order[1] == 2 && order[2] == 1 && order[3] == 3,
	      "on first: 0 2 1 3, got %d %d %d %d", order[0], order[1], order[2], order[3]);
	n = cheats_order(&M, false, order);
	CHECK(n == 4 && order[1] == 1, "as they are otherwise");
	cheats_free(&M);

	/* An update: Infinite Coins' code changed, Infinite Lives went, one came */
	put("Cheats/SNES/.libretro/Kingdom Run/Kingdom Run (USA).cht",
	    "cheats = 2\ncheat0_desc = \"Infinite Coins\"\ncheat0_code = \"7E0DBF:98\"\n"
	    "cheat1_desc = \"Moon Jump\"\ncheat1_code = \"7E0100:01\"\n");
	cheats_load(&M, card, "SNES", "SFC", "Kingdom Run.sfc");
	cheats_state_apply(&M, state);
	CHECK(M.c[0].on && cheats_on_count(&M) == 1,
	      "after an update the player's cheat stays on, the changed one is off: %d on",
	      cheats_on_count(&M));
	cheats_state_str(&M, state, sizeof state);
	CHECK(strlen(state) == 8, "and the state keeps only what is there: '%s'", state);
	cheats_free(&M);

	/* NextUI's names, each once, in a subfolder game too */
	put1("Cheats/SNES/Kingdom Run.sfc.cht", "NextUI name", "BBBB");
	put1("Cheats/SFC/Kingdom Run.sfc.cht", "NextUI folder", "CCCC");
	put1("Cheats/SFC/Kingdom Run.cht", "NextUI folder, no extension", "DDDD");
	cheats_load(&L, card, "SNES", "SFC", "Hacks/Kingdom Run.sfc");
	CHECK(L.ng == 6 && L.g[0].yours && L.g[1].yours && L.g[2].yours && L.g[3].yours,
	      "a game in a subfolder: its four own files, then libretro's: %d groups", L.ng);
	CHECK(!strcmp(L.c[1].desc, "NextUI name") && !strcmp(L.c[2].desc, "NextUI folder, no extension"),
	      "in their order: '%s' '%s'", L.c[1].desc, L.c[2].desc);
	cheats_free(&L);

	/* A system whose tag is its folder looks there once */
	put1("Cheats/GB/Tetris.cht", "Once", "EEEE");
	cheats_load(&L, card, "GB", "GB", "Tetris.gb");
	CHECK(L.n == 1, "tag and folder the same: the file once, got %d", L.n);
	cheats_free(&L);

	{
		char key[128];

		cheats_key("SFC", "Kingdom Run.sfc", key, sizeof key);
		CHECK(!strcmp(key, "cheats.SFC.Kingdom Run.sfc"), "the key: '%s'", key);
	}
	CHECK(cheats_id("a", "b", "c") != cheats_id("ab", "", "c"), "the id keeps its parts apart");
}

int main(void)
{
	reader();
	list();
	if (failures) {
		printf("cheats: %d FAILED\n", failures);
		return 1;
	}
	printf("ok: cheats (the .cht reader and a game's list)\n");
	return 0;
}
