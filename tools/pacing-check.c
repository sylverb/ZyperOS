/* SPDX-License-Identifier: GPL-3.0-only */
/* Timing, a system menu row (src/pacing.h): its two choices, the words kept
 * and sent as RUN's pace=, the system's own default for a word that is none,
 * the database key, and stepping round. The row on the menu is check-menus';
 * RUN's keys and the systems that have the row, check-config's; what Diatom
 * does with pace=, diatom's check-displaylock. */
#include <stdio.h>
#include <string.h>

#include "../src/pacing.h"

static int checks, fails;

static void ck(int ok, const char *what)
{
	checks++;
	if (!ok) { fails++; printf("  FAIL %s\n", what); }
}

int main(void)
{
	char key[32];

	ck(!strcmp(pacing_id(PACING_SMOOTH), "smooth") && !strcmp(pacing_id(PACING_EXACT), "exact"),
	   "each choice its word, the one Diatom's RUN pace= takes");
	ck(pacing_parse("smooth", PACING_EXACT) == PACING_SMOOTH &&
	   pacing_parse("exact", PACING_SMOOTH) == PACING_EXACT, "and read back, whatever the default");
	ck(pacing_parse(NULL, PACING_SMOOTH) == PACING_SMOOTH && pacing_parse("", PACING_SMOOTH) == PACING_SMOOTH &&
	   pacing_parse("core", PACING_SMOOTH) == PACING_SMOOTH, "a word that is none is the system's own default");
	ck(pacing_parse(NULL, PACING_EXACT) == PACING_EXACT && pacing_parse("fast", PACING_EXACT) == PACING_EXACT,
	   "Exact's too");
	ck(pacing_parse(NULL, (pacing_choice)9) == PACING_EXACT, "a default that is none is Exact, the game's own rate");
	ck(!strcmp(pacing_id((pacing_choice)9), "exact"), "and so is a choice that is none");
	ck(pacing_step(PACING_SMOOTH, 1) == PACING_EXACT && pacing_step(PACING_EXACT, 1) == PACING_SMOOTH,
	   "right steps round the two");
	ck(pacing_step(PACING_SMOOTH, -1) == PACING_EXACT && pacing_step(PACING_EXACT, -1) == PACING_SMOOTH,
	   "and left");
	ck(pacing_step((pacing_choice)7, 1) == PACING_SMOOTH, "a choice that is none steps from Exact");
	ck(pacing_label_id(PACING_SMOOTH) == S_SYSMENU_PACE_SMOOTH &&
	   pacing_label_id(PACING_EXACT) == S_SYSMENU_PACE_EXACT, "each its own value on the row");
	pacing_key("GBA", key, sizeof key);
	ck(!strcmp(key, "pace.GBA"), "the system's own key in the database, by its tag");
	pacing_key("NDS", key, sizeof key);
	ck(!strcmp(key, "pace.NDS"), "another system's, another key");
	if (fails) {
		printf("pacing: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("pacing: %d checks passed\n", checks);
	return 0;
}
