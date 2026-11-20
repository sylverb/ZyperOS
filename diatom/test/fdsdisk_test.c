/* A Famicom Disk System disk's messages (src/fdsdisk.h): FCEUmm's own words,
 * as libretro-fceumm 236ccdf's src/fds.c formats them, read into what the
 * pop-up says; and every other text left alone. Offline: src/fdsdisk.c
 * alone. */
#include <stdio.h>
#include <string.h>

#include "fdsdisk.h"

static int g_fail, g_pass;

static void ck(int cond, const char *what)
{
	if (cond) { g_pass++; return; }
	g_fail++;
	printf("  FAIL %s\n", what);
}

/* The core's format strings, as the core fills them */
static const char *core_says(char *buf, size_t n, const char *fmt, int disk, int of, int side)
{
	snprintf(buf, n, fmt, 1 + (disk >> 1), of, side ? "B" : "A");
	return buf;
}

static void one(const char *text, fdsdisk_what what, int disk, int of, char side, const char *why)
{
	fdsdisk_msg m;
	bool ok = diatom_fdsdisk_parse(text, &m);

	ck(ok == (what != FDSDISK_NONE) && m.what == what && m.disk == disk && m.of == of && m.side == side,
	   why);
}

int main(void)
{
	char buf[96];
	fdsdisk_msg m;
	int sides;

	/* FCEU_FDSSelect's and FCEU_FDSInsert's, each side of a two-disk game:
	 * SelectDisk 0..3 is disk 1 A, 1 B, 2 A, 2 B, and the count is
	 * (TotalSides + 1) >> 1 */
	for (sides = 0; sides < 4; sides++) {
		char why[96];

		snprintf(why, sizeof why, "Selected, side %d of 4", sides);
		one(core_says(buf, sizeof buf, "Disk %d of %d Side %s Selected", sides, 2, sides & 1),
		    FDSDISK_SELECTED, 1 + (sides >> 1), 2, (sides & 1) ? 'B' : 'A', why);
		snprintf(why, sizeof why, "Inserted, side %d of 4", sides);
		one(core_says(buf, sizeof buf, "Disk %d of %d Side %s Inserted", sides, 2, sides & 1),
		    FDSDISK_INSERTED, 1 + (sides >> 1), 2, (sides & 1) ? 'B' : 'A', why);
		snprintf(why, sizeof why, "Ejected, side %d of 4", sides);
		one(core_says(buf, sizeof buf, "Disk %d of %d Side %s Ejected", sides, 2, sides & 1),
		    FDSDISK_EJECTED, 1 + (sides >> 1), 2, (sides & 1) ? 'B' : 'A', why);
	}
	one("Disk 1 of 1 Side B Selected", FDSDISK_SELECTED, 1, 1, 'B', "a one-disk game's side B");
	one("Eject disk before selecting", FDSDISK_EJECT_FIRST, 0, 0, 0, "L1 with the disk in");

	/* Everything else is not a disk message */
	one(NULL, FDSDISK_NONE, 0, 0, 0, "NULL");
	one("", FDSDISK_NONE, 0, 0, 0, "empty");
	one("System: NTSC", FDSDISK_NONE, 0, 0, 0, "FCEUmm's region message");
	one("FDS BIOS image (disksys.rom) missing", FDSDISK_NONE, 0, 0, 0, "its BIOS message");
	one("Disk 1 of 2 Side C Selected", FDSDISK_NONE, 0, 0, 0, "a side that is no side");
	one("Disk 3 of 2 Side A Selected", FDSDISK_NONE, 0, 0, 0, "a disk past the count");
	one("Disk 0 of 2 Side A Selected", FDSDISK_NONE, 0, 0, 0, "disk 0");
	one("Disk 1 of 9 Side A Selected", FDSDISK_NONE, 0, 0, 0, "more disks than the core takes");
	one("Disk 1 of 2 Side A Turned", FDSDISK_NONE, 0, 0, 0, "another end word");
	one("Disk 1 of 2 Side A Selected now", FDSDISK_NONE, 0, 0, 0, "words after the end");
	one("Disk 1 of 2 Side A", FDSDISK_NONE, 0, 0, 0, "no end word");
	one("Eject disk before selecting!", FDSDISK_NONE, 0, 0, 0, "not the whole warning");

	/* The DISK line's words */
	ck(!strcmp(diatom_fdsdisk_word(FDSDISK_SELECTED), "selected") &&
	   !strcmp(diatom_fdsdisk_word(FDSDISK_INSERTED), "in") &&
	   !strcmp(diatom_fdsdisk_word(FDSDISK_EJECTED), "out") &&
	   !strcmp(diatom_fdsdisk_word(FDSDISK_EJECT_FIRST), "eject-first") &&
	   diatom_fdsdisk_word(FDSDISK_NONE) == NULL, "each kind's word on the DISK line");
	diatom_fdsdisk_parse("x", &m);
	ck(m.what == FDSDISK_NONE && !m.disk && !m.side, "a refusal leaves nothing behind");

	printf("fdsdisk-test: %d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
