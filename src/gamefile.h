/* SPDX-License-Identifier: GPL-3.0-only */
/* A game's own file, looked at before its core is: one that is empty, or too
 * short to be what its name says, is refused on the shelf with that said.
 *
 * A 0-byte file is no game for any core. On 2026-11-16 every PSP .iso on a
 * test card was a 0-byte placeholder, and PPSSPP, handed one, said "[BOOT]
 * File is empty" and crashed Diatom at address 0 (SIGSEGV); the shelf came
 * back with nothing said. Other cores refuse such a file the way they refuse a
 * corrupt one, which the shelf says as "couldn't start".
 *
 * A container has a size under which it cannot be one: a zip's end record is
 * 22 bytes, a 7z's start header 32, the oldest CHD header 76, a CSO's 24, a
 * PBP's 40, a disc image's first sector 2048. A ROM has no such size (an Atari
 * 2600 game can be 2 KB, a homebrew less), so a plain ROM is judged by being
 * empty alone, and so is a .cue or .m3u, which are text naming other files. */
#ifndef ZYPEROS_GAMEFILE_H
#define ZYPEROS_GAMEFILE_H

#include <stddef.h>

typedef enum {
	GAMEFILE_OK,      /* not empty, and not shorter than its kind can be */
	GAMEFILE_EMPTY,   /* 0 bytes */
	GAMEFILE_SHORT,   /* shorter than its kind's least (gamefile_least) */
} gamefile_state;

/* The fewest bytes a file of `name`'s kind can have, by its extension; 1 for
 * a kind with no such size (any ROM, a .cue, an .m3u) */
unsigned gamefile_least(const char *name);

/* The game file at `path`. Not a file (a folder, a game a port starts) or
 * not there is GAMEFILE_OK here: those are the library's and the core's to
 * say. `*have` gets its bytes. */
gamefile_state gamefile_check(const char *path, unsigned long long *have);

/* What the shelf says under the game: "This game file is empty", "This game
 * file is cut short: 10 bytes". Empty for GAMEFILE_OK. */
void gamefile_message(gamefile_state st, unsigned long long have, char *out, size_t n);

#endif
