/* A game's own file, judged before its core is (src/gamefile.h).
 *
 * On 2026-11-16 every PSP .iso on a test card was a 0-byte placeholder, and
 * PPSSPP, handed one, crashed Diatom at address 0; the shelf came back with
 * nothing said.
 *
 * This holds: each kind's least size, by extension and in any case, and a
 * plain ROM, a .cue and an .m3u held to none but not being empty; in a
 * scratch folder, a 0-byte file of every kind refused as empty, a file under
 * its kind's least refused as incomplete, one at it taken, a folder of a
 * game's name and a file that is not there left to the library and the core;
 * and what the shelf says, in English and in French's own words for a size.
 *
 * Links src/gamefile.c, src/biosfile.c (its sizes for a player), src/config.c
 * and the strings, and NOT SDL. Writes under a scratch directory of its own.
 */
#include "../src/gamefile.h"
#include "../src/i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;

static void ck(int cond, const char *what)
{
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

static char dir[64];

/* `name` in the scratch folder with `bytes` bytes; its path in `out` */
static void make(const char *name, size_t bytes, char *out, size_t n)
{
	FILE *f;
	size_t i;

	snprintf(out, n, "%s/%s", dir, name);
	f = fopen(out, "wb");
	if (!f) { printf("  FAIL cannot write %s\n", out); fails++; return; }
	for (i = 0; i < bytes; i++) fputc(0x5a, f);
	fclose(f);
}

static void judge(const char *name, size_t bytes, gamefile_state want, const char *what)
{
	char path[256], line[320];
	unsigned long long have;
	gamefile_state got;

	make(name, bytes, path, sizeof path);
	got = gamefile_check(path, &have);
	snprintf(line, sizeof line, "%s (%s, %zu bytes): state %d, wanted %d", what, name, bytes, got, want);
	ck(got == want, line);
	snprintf(line, sizeof line, "%s: its size said back", what);
	ck(have == bytes, line);
	unlink(path);
}

int main(void)
{
	char path[256], msg[160];
	unsigned long long have;

	/* ---- each kind's least size ---- */
	ck(gamefile_least("Game.zip") == 22, "a zip: its end record, 22 bytes");
	ck(gamefile_least("Game.ZIP") == 22, "a zip in capitals: the same");
	ck(gamefile_least("Game.7z") == 32, "a 7z: its start header, 32 bytes");
	ck(gamefile_least("Game.chd") == 76, "a CHD: the shortest header, 76 bytes");
	ck(gamefile_least("Game.cso") == 24, "a CSO: its header, 24 bytes");
	ck(gamefile_least("EBOOT.PBP") == 40, "a PBP: its header, 40 bytes");
	ck(gamefile_least("God of War.iso") == 2048, "an iso: a sector, 2048 bytes");
	ck(gamefile_least("Contra.nes") == 1, "a ROM: no least size");
	ck(gamefile_least("Sonic.bin") == 1, "a .bin: a ROM as often as a disc, no least size");
	ck(gamefile_least("Disc.cue") == 1, "a .cue: text, no least size");
	ck(gamefile_least("Game.m3u") == 1, "an .m3u: text, no least size");
	ck(gamefile_least("README") == 1, "no extension: no least size");
	ck(gamefile_least("Pack.zip/Game") == 1, "a dot in a folder's name is not the file's");
	ck(gamefile_least(NULL) == 1, "no name: no least size");

	/* ---- files on a card ---- */
	snprintf(dir, sizeof dir, "/tmp/gamefile-check-XXXXXX");
	if (!mkdtemp(dir)) { printf("  FAIL no scratch folder\n"); return 1; }
	judge("God of War.iso", 0, GAMEFILE_EMPTY, "a 0-byte PSP iso (the crash)");
	judge("Contra.nes", 0, GAMEFILE_EMPTY, "a 0-byte ROM");
	judge("Disc.cue", 0, GAMEFILE_EMPTY, "a 0-byte cue");
	judge("Game.zip", 0, GAMEFILE_EMPTY, "a 0-byte zip");
	judge("Game.zip", 10, GAMEFILE_SHORT, "a zip of 10 bytes");
	judge("Game.zip", 22, GAMEFILE_OK, "a zip of 22 bytes");
	judge("God of War.iso", 2047, GAMEFILE_SHORT, "an iso a byte short of a sector");
	judge("God of War.iso", 2048, GAMEFILE_OK, "an iso of a sector");
	judge("Tiny.nes", 1, GAMEFILE_OK, "a ROM of 1 byte (not judged by size)");
	judge("Disc.cue", 30, GAMEFILE_OK, "a short cue");

	snprintf(path, sizeof path, "%s/Folder.iso", dir);
	mkdir(path, 0755);
	ck(gamefile_check(path, &have) == GAMEFILE_OK, "a folder of a game's name: not judged here");
	rmdir(path);
	snprintf(path, sizeof path, "%s/Gone.iso", dir);
	ck(gamefile_check(path, &have) == GAMEFILE_OK, "a file that is not there: not judged here");
	ck(gamefile_check(NULL, &have) == GAMEFILE_OK, "no path: not judged here");
	rmdir(dir);

	/* ---- what the shelf says ---- */
	i18n_set(LANG_EN);
	gamefile_message(GAMEFILE_EMPTY, 0, msg, sizeof msg);
	ck(!strcmp(msg, "This game file is empty"), "English: empty");
	gamefile_message(GAMEFILE_SHORT, 10, msg, sizeof msg);
	ck(!strcmp(msg, "This game file is incomplete: 10 bytes"), "English: 10 bytes");
	gamefile_message(GAMEFILE_SHORT, 1, msg, sizeof msg);
	ck(!strcmp(msg, "This game file is incomplete: 1 byte"), "English: 1 byte, one");
	gamefile_message(GAMEFILE_SHORT, 1500, msg, sizeof msg);
	ck(!strcmp(msg, "This game file is incomplete: 1 KB"), "English: 1,500 bytes is 1 KB");
	gamefile_message(GAMEFILE_OK, 4096, msg, sizeof msg);
	ck(msg[0] == '\0', "nothing said for a good file");
	i18n_set(LANG_FR);
	gamefile_message(GAMEFILE_SHORT, 10, msg, sizeof msg);
	ck(!strcmp(msg, "Ce fichier de jeu est incomplet : 10 octets"), "French: its own word for bytes");
	i18n_set(LANG_EN);

	if (fails) { printf("gamefile-check: %d FAILED\n", fails); return 1; }
	printf("ok: an empty game file of any kind, and a zip, 7z, CHD, CSO, PBP or iso shorter than its kind can be, "
	       "is refused and said; a ROM, a cue or an m3u is judged by being empty alone\n");
	return 0;
}
