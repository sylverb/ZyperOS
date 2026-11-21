/* SPDX-License-Identifier: GPL-3.0-only */
/* MENU > Display > Home Systems' setting (src/homesys.h): the word the device
 * database keeps, read and written back, and which shelves it turns off. */
#include <stdio.h>
#include <string.h>

#include "../src/homesys.h"

static int checks, failures;

static void ck(int ok, const char *what)
{
	checks++;
	if (!ok) {
		failures++;
		fprintf(stderr, "FAIL: %s\n", what);
	}
}

int main(void)
{
	char w[HOMESYS_WORD];

	homesys_parse(NULL);
	ck(homesys_shown("PRT") && homesys_shown(HOMESYS_FAV) && homesys_shown(HOMESYS_RECENT),
	   "nothing said: every shelf on, Favorites and Recents too");
	homesys_word(w, sizeof w);
	ck(!w[0] && homesys_hidden_count() == 0, "and the word is empty");

	homesys_parse("PRT,RECENT");
	ck(!homesys_shown("PRT") && !homesys_shown("RECENT") && homesys_shown("PS1"),
	   "the word's tags off, the rest on");
	ck(!homesys_shown("RECENT") && homesys_shown("RECENTS") && homesys_shown("PR"),
	   "a tag is matched whole");
	homesys_word(w, sizeof w);
	ck(!strcmp(w, "PRT,RECENT"), "written back as read");

	homesys_parse("COLL,SHOT");
	ck(!homesys_shown(HOMESYS_COLL) && !homesys_shown(HOMESYS_SHOT) && homesys_shown(HOMESYS_FAV),
	   "Collections and Screenshots off by their tags, as any shelf");

	homesys_parse(" PRT , GB,,PRT,\n");
	homesys_word(w, sizeof w);
	ck(!strcmp(w, "PRT,GB") && homesys_hidden_count() == 2,
	   "spaces, an empty tag and a repeat, as a hand edit leaves them, read as two");

	ck(homesys_set("PS1", false) && !homesys_shown("PS1"), "turned off");
	ck(!homesys_set("PS1", false), "and again is no change");
	ck(homesys_set("PRT", true) && homesys_shown("PRT"), "turned on");
	ck(!homesys_set("PRT", true), "and again is no change");
	homesys_word(w, sizeof w);
	ck(!strcmp(w, "GB,PS1"), "in the order they went off");
	ck(!homesys_set("", false) && !homesys_set(NULL, false) &&
	   !homesys_set("A_TAG_FAR_TOO_LONG", false), "no tag, or one too long, changes nothing");

	{
		char small[8];

		homesys_parse("PRT,GB,PS1");
		homesys_word(small, sizeof small);
		ck(!strcmp(small, "PRT,GB"), "a word that does not fit loses whole tags, never half of one");
	}
	{
		char many[HOMESYS_MAX * 4 + 64] = "";
		size_t at = 0;

		for (int i = 0; i < HOMESYS_MAX + 5; i++)
			at += (size_t)snprintf(many + at, sizeof many - at, "%sS%d", i ? "," : "", i);
		homesys_parse(many);
		ck(homesys_hidden_count() == HOMESYS_MAX, "no more than HOMESYS_MAX tags are kept");
		homesys_word(w, sizeof w);
		ck(strlen(w) < sizeof w, "and their word fits HOMESYS_WORD");
	}
	printf("homesys: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
