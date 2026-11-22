/* The Super Game Boy per game (src/sgb.h): which cartridge headers start as
 * one under Its Games, the word RUN sends, and the two
 * options set for such a game. Offline: src/sgb.c alone, with
 * diatom_options_set written down here instead of sent to a core. */
#include <stdio.h>
#include <string.h>

#include "sgb.h"

static int g_fail, g_pass;

/* What src/sgb.c asked of the options, in order */
static char g_set[4][64];
static int g_nset;

bool diatom_options_set(const char *key, const char *value)
{
	if (g_nset < 4) snprintf(g_set[g_nset++], sizeof g_set[0], "%s=%s", key, value);
	return true;
}

static void ck(int cond, const char *what)
{
	if (cond) { g_pass++; return; }
	g_fail++;
	printf("  FAIL %s\n", what);
}

/* A 32 KB ROM with only the three header bytes that matter set */
static unsigned char rom[0x8000];

static void cart(unsigned char cgb, unsigned char sgb, unsigned char licensee)
{
	memset(rom, 0, sizeof rom);
	rom[0x143] = cgb;
	rom[0x146] = sgb;
	rom[0x14B] = licensee;
}

int main(void)
{
	/* The words */
	ck(diatom_sgb_parse("games") == DIATOM_SGB_GAMES, "games");
	ck(diatom_sgb_parse("off") == DIATOM_SGB_OFF && diatom_sgb_parse("") == DIATOM_SGB_OFF &&
	   diatom_sgb_parse(NULL) == DIATOM_SGB_OFF && diatom_sgb_parse("Games") == DIATOM_SGB_OFF &&
	   diatom_sgb_parse("every") == DIATOM_SGB_OFF, "anything else is off (no Every Game: src/sgb.h)");

	/* A game made for it: Game Boy only, and with Color colors too (0x80,
	 * as the Pokemon Yellow and Link's Awakening DX cartridges have) */
	cart(0x00, 0x03, 0x33);
	ck(diatom_sgb_wanted(DIATOM_SGB_GAMES, rom, sizeof rom), "Its Games: a game made for it");
	ck(!diatom_sgb_wanted(DIATOM_SGB_OFF, rom, sizeof rom), "Off: never");
	cart(0x80, 0x03, 0x33);
	ck(diatom_sgb_wanted(DIATOM_SGB_GAMES, rom, sizeof rom),
	   "Its Games: a Color-too game made for it is a Super Game Boy, not a Color");

	/* The flag without the new licensee code is not one (mGBA's rule) */
	cart(0x00, 0x03, 0x01);
	ck(!diatom_sgb_wanted(DIATOM_SGB_GAMES, rom, sizeof rom), "the flag with an old licensee is not");

	/* A plain Game Boy game */
	cart(0x00, 0x00, 0x33);
	ck(!diatom_sgb_wanted(DIATOM_SGB_GAMES, rom, sizeof rom), "Its Games: not a game without the flag");
	cart(0x80, 0x00, 0x01);
	ck(!diatom_sgb_wanted(DIATOM_SGB_GAMES, rom, sizeof rom), "nor a Color-too game without it");

	/* A Color only cartridge cannot run on one */
	cart(0xC0, 0x03, 0x33);
	ck(!diatom_sgb_wanted(DIATOM_SGB_GAMES, rom, sizeof rom), "a Color only cartridge never, flag or not");

	/* Too short for a header, or none at all */
	cart(0x00, 0x03, 0x33);
	ck(!diatom_sgb_wanted(DIATOM_SGB_GAMES, rom, 0x14F), "a ROM that ends inside its header");
	ck(!diatom_sgb_wanted(DIATOM_SGB_GAMES, NULL, 0), "no ROM in memory");
	ck(diatom_sgb_wanted(DIATOM_SGB_GAMES, rom, 0x150), "exactly the header is enough");

	/* The options, for this game alone, and only when it is wanted */
	cart(0x00, 0x03, 0x33);
	diatom_sgb_set(DIATOM_SGB_GAMES);
	g_nset = 0;
	diatom_sgb_apply(rom, sizeof rom);
	ck(g_nset == 2 && !strcmp(g_set[0], "mgba_gb_model=Super Game Boy") &&
	   !strcmp(g_set[1], "mgba_sgb_borders=ON"), "a game made for it: the model and its border");
	cart(0x00, 0x00, 0x33);
	g_nset = 0;
	diatom_sgb_apply(rom, sizeof rom);
	ck(g_nset == 0, "a game that is not: nothing set, the launcher's values stand");
	diatom_sgb_set(DIATOM_SGB_OFF);
	cart(0x00, 0x03, 0x33);
	g_nset = 0;
	diatom_sgb_apply(rom, sizeof rom);
	diatom_sgb_apply(NULL, 0);
	ck(g_nset == 0, "Off sets nothing, nor a ROM the core reads itself");

	printf("sgb-test: %d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
