/* SPDX-License-Identifier: GPL-3.0-only */
/* Collections on the card (src/collect.h): the lists read in their every
 * form, kept as written, added to, taken from, renamed and deleted, and a
 * game's rename and delete followed. In a folder of its own under /tmp. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/collect.h"

static int checks, failures;

static void ck(int ok, const char *what)
{
	checks++;
	if (!ok) {
		failures++;
		fprintf(stderr, "FAIL: %s\n", what);
	}
}

static char g_card[256];

static void put(const char *name, const char *text)
{
	char path[512];
	FILE *f;

	snprintf(path, sizeof path, "%s/Collections/%s", g_card, name);
	if (!(f = fopen(path, "wb"))) { perror(path); exit(2); }
	fputs(text, f);
	fclose(f);
}

static char *slurp(const char *name)
{
	static char buf[8192];
	char path[512];
	size_t n;
	FILE *f;

	snprintf(path, sizeof path, "%s/Collections/%s", g_card, name);
	buf[0] = '\0';
	if (!(f = fopen(path, "rb"))) return NULL;
	n = fread(buf, 1, sizeof buf - 1, f);
	buf[n] = '\0';
	fclose(f);
	return buf;
}

static int game(int i, int k, const char *folder, const char *file)
{
	char f[COLL_LINE], p[COLL_LINE];

	return coll_game(i, k, f, sizeof f, p, sizeof p) && !strcmp(f, folder) && !strcmp(p, file);
}

int main(void)
{
	char f[COLL_LINE], p[COLL_LINE], dir[300];
	const char *s;
	int i;

	snprintf(g_card, sizeof g_card, "/tmp/collect-check.%d", (int)getpid());
	snprintf(dir, sizeof dir, "%s/Collections", g_card);
	mkdir(g_card, 0755);

	coll_load(g_card);
	ck(coll_count() == 0, "no Collections folder: no collection");

	mkdir(dir, 0755);
	put("Couch Co-op.txt", "\xEF\xBB\xBF/Roms/SNES/Kingdom Run.sfc\r\n"
	                       "Roms/GBA/Golden Sun.gba\n"
	                       "\n"
	                       "# kept for later\n"
	                       "PS/Night Drive/Night Drive (Disc 1).cue\n"
	                       "/mnt/SDCARD/Roms/GB/Pocket Quest.gb\n"
	                       "C:\\Games\\Roms\\NES\\Brick Hero.nes\n"
	                       "nothing here\n\n");
	put("rpgs to finish.TXT", "/Roms/GBC/Sky Fort.gbc\n");
	put("._Couch Co-op.txt", "macOS's shadow\n");
	put(".hidden.txt", "/Roms/NES/a.nes\n");
	put("notes.md", "/Roms/NES/a.nes\n");
	coll_load(g_card);
	ck(coll_count() == 2, "the .txt lists only, no dot file and no other kind");
	ck(!strcmp(coll_name(0), "Couch Co-op") && !strcmp(coll_name(1), "rpgs to finish"),
	   "named by their stems, A to Z in any case");
	ck(coll_find("COUCH CO-OP") == 0 && coll_find("Puzzles") < 0, "found by name in any case");
	ck(coll_lines(0) == 8, "every line kept, the file's own last end dropped");
	ck(coll_games(0) == 5, "five of them name a game");
	ck(game(0, 0, "SNES", "Kingdom Run.sfc"), "the BOM and the CR dropped, /Roms/ read");
	ck(game(0, 1, "GBA", "Golden Sun.gba"), "Roms/ without its slash");
	ck(!coll_game(0, 2, f, sizeof f, p, sizeof p), "an empty line is no game");
	ck(!coll_game(0, 3, f, sizeof f, p, sizeof p), "a note is no game");
	ck(game(0, 4, "PS", "Night Drive/Night Drive (Disc 1).cue"), "a path from Roms/ itself, a disc folder in it");
	ck(game(0, 5, "GB", "Pocket Quest.gb"), "a card's own mount before /Roms/");
	ck(game(0, 6, "NES", "Brick Hero.nes"), "backslashes read as slashes");
	ck(!coll_game(0, 7, f, sizeof f, p, sizeof p), "a line with no folder is no game");
	ck(coll_has(0, "gba", "Golden Sun.gba") && !coll_has(0, "GBA", "golden sun.gba"),
	   "the folder in any case, the path as it is");

	/* Added: at the end, written through, and once */
	ck(coll_add(1, "GBA", "Golden Sun.gba"), "a game added");
	s = slurp("rpgs to finish.TXT");
	ck(s && !strcmp(s, "/Roms/GBC/Sky Fort.gbc\n/Roms/GBA/Golden Sun.gba\n"),
	   "at the end, in the first form");
	ck(coll_add(1, "GBA", "Golden Sun.gba") && coll_lines(1) == 2, "a second add writes nothing");

	/* Taken out: every line naming it, the rest as written */
	ck(coll_remove(0, "SNES", "Kingdom Run.sfc"), "a game taken out");
	s = slurp("Couch Co-op.txt");
	ck(s && !strncmp(s, "Roms/GBA/Golden Sun.gba\n\n# kept for later\n", 42),
	   "the other lines kept as they were written");
	ck(coll_remove(0, "SNES", "Kingdom Run.sfc"), "taken out twice is still out");

	/* A game renamed and one deleted: every list follows */
	ck(coll_follow("GBA", "Golden Sun.gba", "Golden Sun (USA).gba") == 2, "a rename follows into both lists");
	ck(game(1, 1, "GBA", "Golden Sun (USA).gba") && coll_has(0, "GBA", "Golden Sun (USA).gba"),
	   "under its new name in each");
	ck(coll_follow("GBA", "Golden Sun.gba", "x.gba") == 0, "the old name is in none now");
	ck(coll_forget("GBA", "Golden Sun (USA).gba") == 2 && !coll_has(0, "GBA", "Golden Sun (USA).gba") &&
	   coll_lines(1) == 1, "a delete takes it out of both");

	/* Names */
	ck(coll_name_check("  Puzzles  ", -1) == COLL_NAME_OK, "spaces at its ends do not count");
	ck(coll_name_check("   ", -1) == COLL_NAME_EMPTY, "nothing but spaces is no name");
	ck(coll_name_check("a/b", -1) == COLL_NAME_BAD && coll_name_check("what?", -1) == COLL_NAME_BAD &&
	   coll_name_check(".x", -1) == COLL_NAME_BAD && coll_name_check("x.", -1) == COLL_NAME_BAD,
	   "a character a file name cannot hold");
	ck(coll_name_check("couch co-op", -1) == COLL_NAME_TAKEN, "a name taken, in any case");
	ck(coll_name_check("Couch co-op", 0) == COLL_NAME_OK, "but its own, for a rename");
	{
		char long_name[200];

		memset(long_name, 'x', sizeof long_name - 1);
		long_name[sizeof long_name - 1] = '\0';
		ck(coll_name_check(long_name, -1) == COLL_NAME_LONG, "too long for a file's stem");
	}

	/* Made, renamed and deleted */
	i = coll_create(" Puzzles ", "NES", "Brick Hero.nes");
	ck(i == 1 && coll_count() == 3 && !strcmp(coll_name(1), "Puzzles"), "made, in its place A to Z");
	s = slurp("Puzzles.txt");
	ck(s && !strcmp(s, "/Roms/NES/Brick Hero.nes\n"), "its file holds its first game");
	ck(coll_create("puzzles", "NES", "a.nes") < 0 && coll_count() == 3, "a taken name makes none");
	i = coll_rename(1, "A Puzzle Box");
	ck(i == 0 && !strcmp(coll_name(0), "A Puzzle Box") && slurp("A Puzzle Box.txt") && !slurp("Puzzles.txt"),
	   "renamed, its file with it, and moved in the order");
	ck(coll_rename(0, "couch co-op") < 0 && !strcmp(coll_name(0), "A Puzzle Box"), "not onto another's name");
	ck(coll_delete(0) && coll_count() == 2 && !slurp("A Puzzle Box.txt") && !strcmp(coll_name(0), "Couch Co-op"),
	   "deleted, its file with it");

	/* Read again: the same */
	coll_load(g_card);
	ck(coll_count() == 2 && coll_has(1, "GBC", "Sky Fort.gbc") && coll_games(0) == 3,
	   "read again as written");

	coll_free();
	{
		char cmd[400];

		snprintf(cmd, sizeof cmd, "rm -rf '%s'", g_card);
		if (system(cmd) != 0) fprintf(stderr, "collect-check: could not remove %s\n", g_card);
	}
	printf("collect: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
