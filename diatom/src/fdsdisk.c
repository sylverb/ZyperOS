/* SPDX-License-Identifier: GPL-3.0-only */
/* A Famicom Disk System disk's messages. See src/fdsdisk.h. */
#include "fdsdisk.h"

#include <stdio.h>
#include <string.h>

bool diatom_fdsdisk_parse(const char *text, fdsdisk_msg *out)
{
	static const struct { const char *word; fdsdisk_what what; } END[] = {
		{ "Selected", FDSDISK_SELECTED },
		{ "Inserted", FDSDISK_INSERTED },
		{ "Ejected",  FDSDISK_EJECTED },
	};
	char side[2], end[16], extra;
	int disk, of, i;

	memset(out, 0, sizeof *out);
	if (!text) return false;
	if (!strcmp(text, "Eject disk before selecting")) {
		out->what = FDSDISK_EJECT_FIRST;
		return true;
	}
	/* The whole line and nothing after it: %c past the end word finds
	 * anything that follows */
	if (sscanf(text, "Disk %d of %d Side %1[AB] %15s%c", &disk, &of, side, end, &extra) != 4)
		return false;
	/* FCEUmm caps a game at 8 sides, four disks; the disk is never past the
	 * count it gives */
	if (disk < 1 || of < 1 || of > 4 || disk > of) return false;
	for (i = 0; i < (int)(sizeof END / sizeof END[0]); i++)
		if (!strcmp(end, END[i].word)) {
			out->what = END[i].what;
			out->disk = disk;
			out->of = of;
			out->side = side[0];
			return true;
		}
	return false;
}

const char *diatom_fdsdisk_word(fdsdisk_what what)
{
	switch (what) {
	case FDSDISK_SELECTED:    return "selected";
	case FDSDISK_INSERTED:    return "in";
	case FDSDISK_EJECTED:     return "out";
	case FDSDISK_EJECT_FIRST: return "eject-first";
	case FDSDISK_NONE:        break;
	}
	return NULL;
}
