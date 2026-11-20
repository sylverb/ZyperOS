/* SPDX-License-Identifier: GPL-3.0-only */
/* A shelf's firmware in Bios/, looked at the way its core will look at it.
 *
 * config/systems.cfg gives a shelf its disc bios names, with commas when the
 * core takes more than one, and the size a good copy of each name is
 * (firmware|name|bytes). The cores look for nothing but the name and take the
 * FIRST name that is there: a 0-byte saturn_bios.bin starts every Saturn game
 * to a black screen with no sound, and is used over a good mpr-17933.bin
 * beside it. So bios_check takes the names in that order and judges the
 * first one there, as the core will: a game is refused, with the file named,
 * when that file is not a size a good copy is.
 *
 * A file is there when it is a file and can be read; a folder of the name is
 * not (the launcher's check before sizes, kept). A name with no firmware line
 * is judged by being there alone. No checksum: other good dumps of one chip
 * have other sums (systems.cfg says why). */
#ifndef ZYPEROS_BIOSFILE_H
#define ZYPEROS_BIOSFILE_H

#include <stddef.h>

#include "config.h"

typedef enum {
	BIOS_OK,          /* the first name there is a size a good copy is */
	BIOS_MISSING,     /* none of the names is there */
	BIOS_WRONG_SIZE,  /* the first name there is not */
} bios_state;

typedef struct {
	bios_state state;
	/* The name a refusal names, as systems.cfg spells it: the first of
	 * the shelf's when none is there, the file's own when it is the wrong
	 * size. Empty for BIOS_OK. */
	char name[CFG_STR];
	unsigned long long have;  /* BIOS_WRONG_SIZE: the file's bytes */
	unsigned want;            /* BIOS_WRONG_SIZE: a good copy's (the first size given) */
	/* BIOS_WRONG_SIZE: a later name that is there and the right size, which
	 * the core would use once the bad file is gone; empty when none is */
	char good[CFG_STR];
} bios_result;

/* The shelf's disc bios in `dir` (the card's Bios/), as above. A shelf with
 * no disc bios is BIOS_OK. */
bios_state bios_check(const system_cfg *s, const char *dir, bios_result *r);

/* Whether `file`, a game on shelf `s`, needs firmware: a file of a type a
 * `bios|tag|names|extensions` line names needs that line's (an .fds image
 * on the NES shelf, disksys.rom), and a disc (`disc`, lib_is_disc's
 * answer, which this file does not decide) or any game on a shelf whose
 * whole library needs it (bios_every) the disc bios */
bool bios_needs(const system_cfg *s, const char *file, bool disc);

/* bios_check for the firmware `file` needs, as bios_needs picks it. A
 * file that needs none is BIOS_OK. */
bios_state bios_check_file(const system_cfg *s, const char *file, bool disc, const char *dir,
                           bios_result *r);

/* A file's size for a player, in the language that is on: "0 bytes",
 * "10 bytes", "8 KB", "512 KB", "2 MB" (1024s, as firmware sizes are said).
 * KB and MB round down. */
void bios_size_text(unsigned long long bytes, char *out, size_t n);

/* What the shelf says under the game when it is refused: "needs
 * Bios/saturn_bios.bin", "Bios/saturn_bios.bin is 0 bytes; it should be
 * 512 KB", or, when a good later name is there, the one delete that fixes it:
 * "Bios/saturn_bios.bin is 0 bytes; delete it to use mpr-17933.bin". A size that would read as a good copy's is said in bytes.
 * Empty for BIOS_OK. */
void bios_message(const bios_result *r, char *out, size_t n);

#endif
