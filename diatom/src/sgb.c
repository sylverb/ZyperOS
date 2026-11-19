/* SPDX-License-Identifier: GPL-3.0-only */
/* The Super Game Boy per game. See src/sgb.h. */
#include "sgb.h"

#include <string.h>

#include "diatom.h"

/* The cartridge header's bytes (Pan Docs, "The Cartridge Header") */
#define HDR_CGB      0x143   /* 0x80 Color too, 0xC0 Color only */
#define HDR_SGB      0x146   /* 0x03: Super Game Boy functions */
#define HDR_LICENSEE 0x14B   /* 0x33: the new licensee code is used */
#define HDR_END      0x150

static diatom_sgb_mode g_mode;

diatom_sgb_mode diatom_sgb_parse(const char *word)
{
	if (word && !strcmp(word, "games")) return DIATOM_SGB_GAMES;
	if (word && !strcmp(word, "every")) return DIATOM_SGB_EVERY;
	return DIATOM_SGB_OFF;
}

bool diatom_sgb_wanted(diatom_sgb_mode mode, const unsigned char *rom, size_t size)
{
	bool made_for = false, color_only;

	if (mode == DIATOM_SGB_OFF || !rom || size < HDR_END) return false;
	/* mGBA's test (src/gb/gb.c GBValidModels), so a game it would run as
	 * one is the game this says is made for it */
	made_for = rom[HDR_SGB] == 0x03 && rom[HDR_LICENSEE] == 0x33;
	color_only = rom[HDR_CGB] == 0xC0;
	if (color_only) return false;
	return mode == DIATOM_SGB_EVERY || made_for;
}

void diatom_sgb_set(diatom_sgb_mode mode)
{
	g_mode = mode;
}

void diatom_sgb_apply(const unsigned char *rom, size_t size)
{
	if (!diatom_sgb_wanted(g_mode, rom, size)) return;
	diatom_options_set("mgba_gb_model", "Super Game Boy");
	diatom_options_set("mgba_sgb_borders", "ON");
}
