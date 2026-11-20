/* SPDX-License-Identifier: GPL-3.0-only */
/* A screenshot's file name (src/shotname.h, DESIGN.md 11.68): read as the
 * Screenshot hotkey writes it, every other file in the folder left alone,
 * the time kept by the calendar and read back to the same name, and the
 * Screenshots shelf's three orders. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/shotname.h"

static int checks, failures;

static void ck(int ok, const char *what)
{
	checks++;
	if (!ok) {
		failures++;
		fprintf(stderr, "FAIL: %s\n", what);
	}
}

static int by_order;

static int cmp(const void *x, const void *y)
{
	return shot_cmp(x, y, (shot_order)by_order);
}

int main(void)
{
	char game[256], name[512];
	long when = 0, w2 = 0;
	int y, mo, d, h, mi, s;

	/* The names the hotkey writes */
	ck(shot_parse("Super Metroid (USA) 2026-11-18 21.40.05.png", game, sizeof game, &when) &&
	   !strcmp(game, "Super Metroid (USA)"), "a shot's game is the name before its time");
	shot_parts(when, &y, &mo, &d, &h, &mi, &s);
	ck(y == 2026 && mo == 11 && d == 18 && h == 21 && mi == 40 && s == 5, "and its time, to the second");
	ck(shot_file_name(game, when, name, sizeof name) &&
	   !strcmp(name, "Super Metroid (USA) 2026-11-18 21.40.05.png"), "written back to the same name");
	ck(shot_parse("Mega Man 2 2026-11-18 21.40.05.PNG", game, sizeof game, NULL) &&
	   !strcmp(game, "Mega Man 2"), ".PNG in capitals too, and a name ending in a number");
	ck(shot_parse("Final Fantasy - 2026 Edition 2026-01-02 03.04.05.png", game, sizeof game, &w2) &&
	   !strcmp(game, "Final Fantasy - 2026 Edition"), "a name with a year of its own: the time is the last part");
	ck(shot_parse("A 2000-01-01 00.00.00.png", game, sizeof game, &when) && when == 0 && !strcmp(game, "A"),
	   "the first second of 2000 is 0");

	/* What is no shot: OpenBOR's own, a picture of another name, a cut name */
	ck(!shot_parse("ScreenShot - 0000.png", game, sizeof game, NULL), "a name with no time is no shot");
	ck(!shot_parse("Game 2026-11-18 21.40.05.jpg", game, sizeof game, NULL), "nor a picture of another kind");
	ck(!shot_parse(" 2026-11-18 21.40.05.png", game, sizeof game, NULL) &&
	   !shot_parse("2026-11-18 21.40.05.png", game, sizeof game, NULL), "nor a time with no game before it");
	ck(!shot_parse("Game 2026-11-18 21:40:05.png", game, sizeof game, NULL) &&
	   !shot_parse("Game 2026-11-1x 21.40.05.png", game, sizeof game, NULL) &&
	   !shot_parse("Game_2026-11-18 21.40.05.png", game, sizeof game, NULL), "nor a time written otherwise");
	ck(!shot_parse("Game 2026-02-30 21.40.05.png", game, sizeof game, NULL) &&
	   !shot_parse("Game 2026-11-18 24.00.00.png", game, sizeof game, NULL) &&
	   !shot_parse("Game 1999-12-31 23.59.59.png", game, sizeof game, NULL), "nor a time the calendar has not got");
	ck(shot_parse("Game 2028-02-29 12.00.00.png", game, sizeof game, NULL) &&
	   !shot_parse("Game 2100-02-29 12.00.00.png", game, sizeof game, NULL), "29 February in a leap year only");
	ck(!shot_parse(NULL, game, sizeof game, NULL) && !shot_parse("", game, sizeof game, NULL),
	   "no name at all");
	{
		char small[6];

		ck(shot_parse("Super Metroid 2026-11-18 21.40.05.png", small, sizeof small, NULL) &&
		   !strcmp(small, "Super"), "a game's name cut to what holds it, never past it");
	}

	/* OpenBOR's own, as its screenshot() names them */
	{
		int num = -1;

		ck(shot_parse_openbor("Streets of Rage Remake - 0000.png", game, sizeof game, &num) &&
		   !strcmp(game, "Streets of Rage Remake") && num == 0, "OpenBOR's: the module before \" - 0000.png\"");
		ck(shot_parse_openbor("Menu - 0042.PNG", game, sizeof game, &num) && !strcmp(game, "Menu") && num == 42,
		   "one taken in its menu is Menu's, .PNG too, its number read");
		ck(shot_parse_openbor("Final Fight - LNS - 9999.png", game, sizeof game, &num) &&
		   !strcmp(game, "Final Fight - LNS") && num == 9999, "a module with \" - \" of its own: the number is the last part");
		ck(!shot_parse_openbor("Super Metroid 2026-11-18 21.40.05.png", game, sizeof game, NULL),
		   "the hotkey's names are not OpenBOR's");
		ck(!shot_parse_openbor(" - 0000.png", game, sizeof game, NULL) &&
		   !shot_parse_openbor("Game - 000.png", game, sizeof game, NULL) &&
		   !shot_parse_openbor("Game - 00a0.png", game, sizeof game, NULL) &&
		   !shot_parse_openbor("Game-0000.png", game, sizeof game, NULL) &&
		   !shot_parse_openbor("Game - 0000.bmp", game, sizeof game, NULL) &&
		   !shot_parse_openbor(NULL, game, sizeof game, NULL),
		   "nor no module, three digits, a letter, no spaces, another picture, or no name");
		ck(!shot_parse("Streets of Rage Remake - 0000.png", game, sizeof game, NULL),
		   "and OpenBOR's are not the hotkey's");
	}

	/* The game's part, as the launcher writes it */
	shot_game_part("Doom/Doom 2\tbeta\\1", game, sizeof game);
	ck(!strcmp(game, "Doom-Doom 2-beta-1"), "a name's '/', tab and '\\' each written '-'");
	shot_game_part(NULL, game, sizeof game);
	ck(!game[0], "no name is an empty part");

	/* The calendar, both ways, over years and their leap days */
	{
		int bad = 0;

		for (long k = 0; k < 36600L * 86400L; k += 86400L * 3 + 3607) {
			char n2[64];
			long back;

			shot_parts(k, &y, &mo, &d, &h, &mi, &s);
			if (shot_secs(y, mo, d, h, mi, s) != k) bad++;
			if (!shot_file_name("G", k, n2, sizeof n2) || !shot_parse(n2, game, sizeof game, &back) ||
			    back != k)
				bad++;
		}
		ck(!bad, "every time back to itself, through its parts and through its name, to 2100");
	}
	ck(shot_secs(2026, 11, 18, 0, 0, 0) - shot_secs(2026, 11, 17, 23, 59, 59) == 1, "a second across midnight");
	ck(shot_secs(2024, 3, 1, 0, 0, 0) - shot_secs(2024, 2, 28, 0, 0, 0) == 2 * 86400L, "a leap day counted");
	ck(shot_days_before(shot_secs(2026, 11, 20, 0, 0, 1), shot_secs(2026, 11, 20, 23, 0, 0)) == 0 &&
	   shot_days_before(shot_secs(2026, 11, 19, 23, 59, 59), shot_secs(2026, 11, 20, 0, 0, 1)) == 1 &&
	   shot_days_before(shot_secs(2026, 11, 13, 12, 0, 0), shot_secs(2026, 11, 20, 1, 0, 0)) == 7,
	   "days apart on the calendar, not by the hours between");
	ck(!shot_file_name("A long game", shot_secs(2026, 1, 1, 0, 0, 0), name, 12), "a name that does not fit is said");

	/* The shelf's orders: the newest first in each */
	{
		shot_key k[6] = {
			{ "Super Metroid", shot_secs(2026, 11, 18, 21, 40, 5), 2 },
			{ "Golden Sun", shot_secs(2026, 11, 20, 9, 12, 40), 1 },
			{ "Super Metroid", shot_secs(2026, 11, 20, 8, 0, 0), 2 },
			{ "Chrono Trigger", shot_secs(2026, 11, 19, 22, 5, 11), 2 },
			{ "golden sun", shot_secs(2026, 11, 17, 7, 0, 0), 1 },
			{ "Metal Gear Solid", shot_secs(2026, 11, 19, 12, 0, 0), 0 },
		};
		shot_key o[6];

		memcpy(o, k, sizeof o);
		by_order = SHOT_ORDER_ADDED;
		qsort(o, 6, sizeof *o, cmp);
		ck(o[0].when == k[1].when && o[1].when == k[2].when && o[2].when == k[3].when &&
		   o[3].when == k[5].when && o[4].when == k[0].when && o[5].when == k[4].when,
		   "Recently Added: every shot, the newest first");
		memcpy(o, k, sizeof o);
		by_order = SHOT_ORDER_NAME;
		qsort(o, 6, sizeof *o, cmp);
		ck(!strcmp(o[0].title, "Chrono Trigger") && o[1].when == k[1].when && o[2].when == k[4].when &&
		   !strcmp(o[3].title, "Metal Gear Solid") && o[4].when == k[2].when && o[5].when == k[0].when,
		   "Name: the games A to Z in any case, each one's newest first");
		memcpy(o, k, sizeof o);
		by_order = SHOT_ORDER_SYSTEM;
		qsort(o, 6, sizeof *o, cmp);
		ck(o[0].owner == 0 && o[1].owner == 1 && o[1].when == k[1].when && o[2].owner == 1 &&
		   !strcmp(o[3].title, "Chrono Trigger") && o[4].when == k[2].when && o[5].when == k[0].when,
		   "System: the home's order of the systems, then as Name");
		ck(shot_cmp(&k[0], &k[0], SHOT_ORDER_ADDED) == 0 && shot_cmp(&k[0], &k[2], SHOT_ORDER_NAME) > 0,
		   "a shot is in its own place, and an older one of a game after the newer");
	}

	printf("shotname: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
