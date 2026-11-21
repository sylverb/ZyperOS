/* SPDX-License-Identifier: GPL-3.0-only */
/* The Saturn core's CHD files closed when a game ends
 * (mk/patches/yabasanshiro-chd-close.patch), checked by
 * mk/build-yabasanshiro.sh inside its build container, against the patched
 * tree it has just built the core from.
 *
 * The core's CD reader (yabause/src/cd-libretro.c, the ISOCD interface:
 * Init when a game loads, DeInit from retro_unload_game through YabauseDeInit
 * and Cs2DeInit) loads a CD's CHD and puts it down three times, as a resident
 * Diatom plays three games, and after each one the process has exactly the
 * files it had before it. While the disc is in, the CHD is one file, and the
 * disc is there: a disc present, its track in the TOC from the CHD's own
 * metadata, its lead-out after it. Before the patch each game left two files open: the one
 * ISOCDInit opened for the header and LoadCHD never closed, and the CHD's own
 * (a GKD Pixel 2, 1.1.0-rc.8, 2026-11-20: four deleted Saturn games held
 * 1.3 GB of the card). The disc is tools/fixtures/rahash/ps1-cd.chd, a
 * one-track CD made by chdman: the reader reads any CD, not only a Saturn's.
 *
 *   yabasanshiro-chd-check DISC.chd
 *
 * Linux only: the files are counted in /proc/self/fd. */
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "cdbase.h"

/* What cd-libretro.c calls of the rest of the core, which is not linked */
void YabSetError(int type, const void *extra)
{
	fprintf(stderr, "  (the reader said error %d: %s)\n", type, extra ? (const char *)extra : "");
}

void YuiMsg(const char *format, ...)
{
	(void)format;
}

/* yabause.c's, as core.h names it for strdup */
char *strdup_(const char *s)
{
	size_t n = strlen(s) + 1;
	char *d = malloc(n);

	if (d) memcpy(d, s, n);
	return d;
}

static int open_files(void)
{
	DIR *d = opendir("/proc/self/fd");
	struct dirent *e;
	int n = 0;

	if (!d) return -1;
	while ((e = readdir(d)) != NULL)
		if (e->d_name[0] != '.') n++;
	closedir(d);
	return n - 1;   /* not the directory's own */
}

int main(int argc, char **argv)
{
	static u32 toc[0xCC / 2];
	int before, i, fails = 0;

	if (argc != 2) {
		fprintf(stderr, "usage: %s DISC.chd\n", argv[0]);
		return 2;
	}
	before = open_files();
	if (before < 0) {
		fprintf(stderr, "no /proc/self/fd\n");
		return 2;
	}
	for (i = 1; i <= 3; i++) {
		int in, after;

		if (ISOCD.Init(argv[1]) != 0) {
			printf("  FAIL game %d: the CHD did not load\n", i);
			return 1;
		}
		in = open_files();
		ISOCD.ReadTOC(toc);
		if (in != before + 1) {
			printf("  FAIL game %d: %d files open with the disc in, %d before it (one, the CHD, is right)\n",
			       i, in, before);
			fails++;
		}
		/* The track at FAD 150, the lead-out past it (isoTOC's 0 and
		 * 101, BuildTOC). Not a sector read: this reader looks the last
		 * track's end up one entry past its table, which a one-track
		 * fixture does not survive, a fault of its own and not this
		 * check's. */
		if (ISOCD.GetStatus() != 0 || (toc[0] & 0xFFFFFF) != 150 ||
		    (toc[101] & 0xFFFFFF) <= 150) {
			printf("  FAIL game %d: no disc there (status %d, track 1 at %u, lead-out %u)\n", i,
			       ISOCD.GetStatus(), (unsigned)(toc[0] & 0xFFFFFF), (unsigned)(toc[101] & 0xFFFFFF));
			fails++;
		}
		ISOCD.DeInit();
		after = open_files();
		if (after != before) {
			printf("  FAIL game %d: %d files open after it ended, %d before it\n", i, after, before);
			fails++;
		}
	}
	if (fails) return 1;
	printf("  ok   three CHD games: one file open while each runs, none left after it\n");
	return 0;
}
