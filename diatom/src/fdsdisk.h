/* SPDX-License-Identifier: GPL-3.0-only */
/* A Famicom Disk System game's disk, as its core says what L1 and R1 did.
 *
 * On FCEUmm an .fds game's L1 turns the disk to its next side and R1 puts it
 * in or takes it out (its retro_run; DESIGN.md 11.28). The core says each one
 * with RETRO_ENVIRONMENT_SET_MESSAGE, in its own English, and nothing else
 * does: there is no disk control interface for an .fds game. These are its
 * words, read from libretro-fceumm at 236ccdf (the pinned core,
 * mk/fetch-vendor.sh), src/fds.c:
 *
 *   FCEU_FDSSelect   "Disk %d of %d Side %s Selected"   (L1, the disk out)
 *                    "Eject disk before selecting"      (L1, the disk in)
 *   FCEU_FDSInsert   "Disk %d of %d Side %s Inserted"   (R1, the disk out)
 *                    "Disk %d of %d Side %s Ejected"    (R1, the disk in)
 *
 * The disk is 1-based, the count is of disks (two sides each), the side "A"
 * or "B". Read into what the launcher's pop-up says in the player's language
 * ("Disk 1 · Side B"); any other text, from this core or another, is none of
 * these. Plain C, no Diatom state: test/fdsdisk_test.c holds it. */
#ifndef DIATOM_FDSDISK_H
#define DIATOM_FDSDISK_H

#include <stdbool.h>

typedef enum {
	FDSDISK_NONE,         /* not a disk message */
	FDSDISK_SELECTED,     /* L1 turned the disk, out of the drive */
	FDSDISK_INSERTED,     /* R1 put it in */
	FDSDISK_EJECTED,      /* R1 took it out */
	FDSDISK_EJECT_FIRST,  /* L1 with the disk in: it is not turned */
} fdsdisk_what;

typedef struct {
	fdsdisk_what what;
	int disk, of;   /* 1-based; 0 for FDSDISK_EJECT_FIRST */
	char side;      /* 'A' or 'B'; 0 for FDSDISK_EJECT_FIRST */
} fdsdisk_msg;

/* `text` read as one of FCEUmm's disk messages, into `out`. False, with
 * `out->what` FDSDISK_NONE, for any other text and for NULL. */
bool diatom_fdsdisk_parse(const char *text, fdsdisk_msg *out);

/* The word Diatom's DISK line says for each: "selected", "in", "out",
 * "eject-first"; NULL for FDSDISK_NONE */
const char *diatom_fdsdisk_word(fdsdisk_what what);

#endif
