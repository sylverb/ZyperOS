/* SPDX-License-Identifier: GPL-3.0-only */
/* The Super Game Boy, a Game Boy shelf's row (the launcher's src/sgbmode.h),
 * made per game from the cartridge's own header.
 *
 * mGBA runs a Game Boy game as a Super Game Boy when its model option says
 * so, and draws the game's border when mgba_sgb_borders is ON. Its model
 * option is one value for every game: "Autodetect" would start a game that
 * also has Game Boy Color colors (a 0x80 cartridge) as a Game Boy Color, not
 * as a Super Game Boy, and "Super Game Boy" starts every game as one. So the
 * launcher says it wants the games made for it (RUN sgb=games), and Diatom
 * reads the header in the ROM it is about to hand the core and sets the two
 * options for this game alone, over what the launcher sent: a game made for
 * it (the header's SGB flag, 0x146 = 0x03, with the old licensee 0x33 at
 * 0x14B: mGBA's own test, GBValidModels) starts as a Super Game Boy, its
 * border on, unless it is a Game Boy Color only cartridge (0xC0 at 0x143),
 * which cannot run on one. Others as before.
 *
 * As before means the launcher's values stand: the Game Boy shelf's model
 * "Game Boy", the Color shelf's own, the border off (src/coreopts.c). The
 * options are mGBA's (libretro/mgba e31759b, libretro_core_options.h), read
 * at the game's load, so set before it. Plain C: test/sgb_test.c holds it. */
#ifndef DIATOM_SGB_H
#define DIATOM_SGB_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
	DIATOM_SGB_OFF,     /* nothing set: the launcher's options stand */
	DIATOM_SGB_GAMES,   /* the games made for it */
} diatom_sgb_mode;

/* RUN's sgb= word: "games"; anything else, empty or NULL, off */
diatom_sgb_mode diatom_sgb_parse(const char *word);

/* Whether the game in `rom` (`size` bytes, a Game Boy ROM) starts as a Super
 * Game Boy under `mode`. A ROM too short for its header never does. */
bool diatom_sgb_wanted(diatom_sgb_mode mode, const unsigned char *rom, size_t size);

/* The mode for the next game (RUN's), kept until the next RUN */
void diatom_sgb_set(diatom_sgb_mode mode);

/* Before the core loads the game: the two options set for it when the mode
 * wants a Super Game Boy (diatom_options_set, dropped when the game ends as
 * every option the launcher sends is). Nothing for a ROM the core reads
 * from its path itself (`rom` NULL). */
void diatom_sgb_apply(const unsigned char *rom, size_t size);

#endif
