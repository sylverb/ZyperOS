/* SPDX-License-Identifier: GPL-3.0-only */
/* Super Game Boy, a system menu row (src/sgbmode.h): its two choices, the
 * words kept and sent as RUN's sgb=, Off for a word that is none, the
 * database key, stepping round, and which shelves have it. The row on the
 * menu is check-menus'; what Diatom does with sgb=, diatom's check-sgb. */
#include <stdio.h>
#include <string.h>

#include "../src/sgbmode.h"

static int checks, fails;

static void ck(int ok, const char *what)
{
	checks++;
	if (!ok) { fails++; printf("  FAIL %s\n", what); }
}

int main(void)
{
	char key[32];

	ck(SGBM_CHOICES == 2, "two choices: Off and Its Games (no Every Game: src/sgbmode.h says why)");
	ck(!strcmp(sgbm_id(SGBM_OFF), "off") && !strcmp(sgbm_id(SGBM_GAMES), "games"),
	   "each choice its word, the one Diatom's RUN sgb= takes");
	ck(sgbm_parse("off") == SGBM_OFF && sgbm_parse("games") == SGBM_GAMES, "and read back");
	ck(sgbm_parse(NULL) == SGBM_OFF && sgbm_parse("") == SGBM_OFF && sgbm_parse("on") == SGBM_OFF &&
	   sgbm_parse("Games") == SGBM_OFF && sgbm_parse("every") == SGBM_OFF,
	   "a word that is none is Off, as before the row");
	ck(!strcmp(sgbm_id((sgbm_choice)9), "off"), "and so is a choice that is none");
	ck(sgbm_step(SGBM_OFF, 1) == SGBM_GAMES && sgbm_step(SGBM_GAMES, 1) == SGBM_OFF,
	   "right steps round the two");
	ck(sgbm_step(SGBM_OFF, -1) == SGBM_GAMES && sgbm_step(SGBM_GAMES, -1) == SGBM_OFF, "and left");
	ck(sgbm_step((sgbm_choice)7, 1) == SGBM_GAMES, "a choice that is none steps from Off");
	ck(sgbm_label_id(SGBM_OFF) == S_MENU_OFF_TITLE && sgbm_label_id(SGBM_GAMES) == S_SYSMENU_SGB_GAMES,
	   "each its own value on the row, Off the shared word");
	ck(sgbm_note_id(SGBM_OFF) == S_SYSMENU_SGB_OFF_NOTE && sgbm_note_id(SGBM_GAMES) == S_SYSMENU_SGB_GAMES_NOTE,
	   "and its own note under the list's picture");
	sgbm_key("GB", key, sizeof key);
	ck(!strcmp(key, "sgb.GB"), "the system's own key in the database, by its tag");
	sgbm_key("GBC", key, sizeof key);
	ck(!strcmp(key, "sgb.GBC"), "the Color's, its own");
	ck(sgbm_shelf("GB", "mgba") && sgbm_shelf("GBC", "mgba"), "the Game Boy's and the Color's shelves on mGBA");
	ck(!sgbm_shelf("GB", "gambatte") && !sgbm_shelf("GBC", "gambatte"), "not on Gambatte, which has none");
	ck(!sgbm_shelf("GBA", "mgba") && !sgbm_shelf("NES", "fceumm") && !sgbm_shelf(NULL, "mgba") &&
	   !sgbm_shelf("GB", NULL), "nor any other shelf");
	if (fails) {
		printf("sgbmode: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("sgbmode: %d checks passed\n", checks);
	return 0;
}
