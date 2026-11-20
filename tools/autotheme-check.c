/* SPDX-License-Identifier: GPL-3.0-only */
/* make check-autotheme: Appearance's Auto, back in 1.1.0 on Night Mode's
 * Night Hours (src/theme.h, src/nightkeep.h; DESIGN.md 11.42).
 *
 * A player missed Auto on 2026-11-20. It came back with one schedule for
 * both: the Night Hours turn the theme dark and Night Mode warm, each on its
 * own setting. Held here, on a scratch database, as the launcher's start and
 * Night Mode's page run it:
 *
 *   - the Night Hours as the database keeps them make Auto's: light inside
 *     the day, dark inside the night, across midnight (21:00 to 7:00), at
 *     the exact minute of each edge, inside a day (13:00 to 15:00), and never
 *     dark for the same two; and a change kept on Night Mode's page moves
 *     Auto at once, Night Mode Off or not
 *   - Auto dark at exactly the minutes Night Mode's own nights are on, every
 *     minute of a day, here and in New York across summer time's end
 *   - the row's three, round both ways
 *   - the first start's migration (night_migrate_db): once a card, a 1.0.5
 *     card's Auto kept as Auto on its old hours, and a new Auto picked in
 *     1.1.0 left alone at every start after, hours and all
 *   - the boot screen's word (src/boottheme.h) carrying the hours, and the
 *     splash reading it (plastron's splash/src/zytheme.h) to the same theme
 *     as the launcher, every minute of the day
 *
 * And Tarik's rule for it, "add in a way that doesn't conflict with the
 * night mode settings" (2026-11-20), as four:
 *
 *   1. Auto writes no night.* key: picking it, leaving it, starts and the
 *      hours passing leave Night Mode's settings as they were, byte for byte
 *   2. Night Mode is the same under Light, Dark and Auto: each of its three
 *      modes, kept and read back, warm at the same times with the same
 *      warmth, and the same settings handed to the display
 *   3. at From and Until the theme turns on the same second, so the same
 *      pass of the launcher's loop, as Night Mode's warmth (plat_night_tick's
 *      own rule), and asks for no table load of its own
 *   4. a 1.0.6 card's Night Mode settings come through its first 1.1.0
 *      start untouched
 *
 * The clock is the check's: every time is handed in. Never touches
 * /tmp/zyperos-theme (theme_set_file(NULL)): a launcher on this host may be
 * running. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../src/boottheme.h"
#include "../src/db.h"
#include "../src/design.h"
#include "../src/nightkeep.h"
#include "../src/nightmode.h"
#include "../src/panelgamma.h"
#include "../src/theme.h"
#include "../plastron/buildroot/package/splash/src/zytheme.h"

static int checks, fails;

static void ck(int ok, const char *what)
{
	checks++;
	if (!ok) {
		fails++;
		printf("  FAIL %s\n", what);
	}
}

/* platform.h's, which night_keep calls: the one way to the display's
 * table from Night Mode's settings. Counted, and what it was handed kept. */
static int night_calls;
static night_cfg night_handed;

void plat_night(const struct night_cfg *n)
{
	night_calls++;
	night_handed = *n;
}

/* Every night.* key and its value, "key=value;" in key order: Night Mode's
 * settings as the database keeps them, for a byte-for-byte compare */
static bool night_row(const char *key, const char *value, void *ctx)
{
	char *out = ctx;
	size_t len = strlen(out);

	snprintf(out + len, 512 - len, "%s=%s;", key, value);
	return true;
}

static void night_keys(char out[512])
{
	out[0] = 0;
	db_each_prefix(db_dev(), "night.", night_row, out);
}

static void zone(const char *tz)
{
	setenv("TZ", tz, 1);
	tzset();
}

/* 2026-11-20 at h:m:s, local time in the zone the check set */
static time_t at(int h, int m, int s)
{
	struct tm tm;

	memset(&tm, 0, sizeof tm);
	tm.tm_year = 2026 - 1900;
	tm.tm_mon = 10;
	tm.tm_mday = 20;
	tm.tm_hour = h;
	tm.tm_min = m;
	tm.tm_sec = s;
	tm.tm_isdst = -1;
	return mktime(&tm);
}

/* Auto's theme at `t`, from the start as the launcher makes it */
static int auto_at(time_t t)
{
	theme_init_at(APPEAR_AUTO, t);
	return theme_index();
}

/* The launcher's start on the database: the migration, then the Night Hours
 * into Auto's (sysmenu_screen.c's look_load), then the saved word */
static appearance start(void)
{
	char w[16];
	night_cfg n;

	night_migrate_db();
	n = night_load();
	night_theme_hours(&n);
	db_get_str(db_dev(), "appearance", w, sizeof w, "light");
	return appearance_parse(w);
}

static void fresh(void)
{
	static const char *keys[] = { "appearance", "night.mode", "night.warmth", "night.from", "night.until",
	                              "auto.light", "auto.dark", AUTO_MIGRATED_KEY };
	size_t i;

	for (i = 0; i < sizeof keys / sizeof keys[0]; i++) db_del(db_dev(), keys[i]);
}

static void hours(void)
{
	printf("the Night Hours are Auto's:\n");
	zone("UTC");
	/* A 1.1.0 card on Auto with Night Hours 21:00 to 7:00 */
	fresh();
	db_set_str(db_dev(), AUTO_MIGRATED_KEY, "1");
	db_set_str(db_dev(), "appearance", "auto");
	db_set_int(db_dev(), "night.from", 21 * 60);
	db_set_int(db_dev(), "night.until", 7 * 60);
	ck(start() == APPEAR_AUTO, "the card starts on Auto");
	ck(auto_at(at(12, 0, 0)) == DS_LIGHT, "noon: light");
	ck(auto_at(at(20, 59, 59)) == DS_LIGHT, "20:59:59: still light");
	ck(auto_at(at(21, 0, 0)) == DS_DARK, "21:00:00, the night's first minute: dark");
	ck(auto_at(at(23, 59, 59)) == DS_DARK, "23:59:59: dark");
	ck(auto_at(at(0, 0, 0)) == DS_DARK, "0:00, across midnight: dark");
	ck(auto_at(at(6, 59, 59)) == DS_DARK, "6:59:59: still dark");
	ck(auto_at(at(7, 0, 0)) == DS_LIGHT, "7:00:00, the day's first minute: light");

	/* The minute's tick across the edge, as the shelf and a game's wait ask */
	theme_init_at(APPEAR_AUTO, at(20, 59, 30));
	ck(theme_index() == DS_LIGHT && !theme_tick_at(at(20, 59, 50)), "20:59:30 light, and no turn inside the minute");
	ck(theme_tick_at(at(21, 0, 30)) && theme_index() == DS_DARK, "21:00:30: the tick turns it dark");
	theme_init_at(APPEAR_AUTO, at(6, 59, 30));
	ck(theme_tick_at(at(7, 0, 30)) && theme_index() == DS_LIGHT, "7:00:30: and light again");

	/* A minute off the page's half hours, as a hand edit keeps it: the
	 * theme turns at that minute, as Night Mode does */
	db_set_int(db_dev(), "night.from", 21 * 60 + 15);
	db_set_int(db_dev(), "night.until", 6 * 60 + 45);
	start();
	ck(auto_at(at(21, 14, 59)) == DS_LIGHT && auto_at(at(21, 15, 0)) == DS_DARK, "21:15 to the minute");
	ck(auto_at(at(6, 44, 59)) == DS_DARK && auto_at(at(6, 45, 0)) == DS_LIGHT, "and 6:45");

	/* Inside one day, and the same two */
	db_set_int(db_dev(), "night.from", 13 * 60);
	db_set_int(db_dev(), "night.until", 15 * 60);
	start();
	ck(auto_at(at(12, 59, 0)) == DS_LIGHT && auto_at(at(13, 0, 0)) == DS_DARK &&
	   auto_at(at(14, 59, 0)) == DS_DARK && auto_at(at(15, 0, 0)) == DS_LIGHT &&
	   auto_at(at(23, 0, 0)) == DS_LIGHT, "13:00 to 15:00: dark inside it, light the rest, the night too");
	db_set_int(db_dev(), "night.until", 13 * 60);
	start();
	ck(auto_at(at(13, 0, 0)) == DS_LIGHT && auto_at(at(2, 0, 0)) == DS_LIGHT,
	   "the same two times: never dark, as Night Mode has no night");

	/* None kept: the Night Hours as they ship */
	db_del(db_dev(), "night.from");
	db_del(db_dev(), "night.until");
	start();
	ck(auto_at(at(20, 59, 0)) == DS_LIGHT && auto_at(at(21, 0, 0)) == DS_DARK && auto_at(at(7, 0, 0)) == DS_LIGHT,
	   "no hours kept: 21:00 to 7:00, as they ship");

	/* Light and Dark ignore them */
	theme_init_at(APPEAR_LIGHT, at(23, 0, 0));
	ck(theme_index() == DS_LIGHT && !theme_tick_at(at(23, 30, 0)), "Light at 23:00 is light and never turns");
	theme_init_at(APPEAR_DARK, at(12, 0, 0));
	ck(theme_index() == DS_DARK, "Dark at noon is dark");
}

/* Night Mode's page keeping new hours: Auto moves with them at once */
static void page(void)
{
	night_cfg c = { NIGHT_OFF, 8, 20 * 60, 8 * 60 };
	int l, d;

	printf("Night Mode's page moves Auto:\n");
	zone("UTC");
	theme_init_at(APPEAR_AUTO, at(20, 30, 0));
	night_keep(&c);
	theme_hours(&l, &d);
	ck(l == 8 * 60 && d == 20 * 60, "kept 20:00 to 8:00: Auto's hours, light from 8:00, dark from 20:00");
	ck(theme_refresh_at(at(20, 30, 0)) && theme_index() == DS_DARK,
	   "20:30, Night Mode Off: Auto turns dark at once (the page's theme_refresh)");
	c.from = 22 * 60;
	night_keep(&c);
	ck(theme_refresh_at(at(20, 30, 1)) && theme_index() == DS_LIGHT, "From moved to 22:00: light again at 20:30");
	c.mode = NIGHT_HOURS;
	night_keep(&c);
	ck(!theme_refresh_at(at(20, 30, 2)) && theme_index() == DS_LIGHT, "By Hours, the same hours: the theme stays");
	/* the database has them, for the next start */
	ck(db_get_int(db_dev(), "night.from", -1) == 22 * 60 && db_get_int(db_dev(), "night.until", -1) == 8 * 60,
	   "and the database keeps them, for the next start");
}

/* Auto dark at exactly the minutes Night Mode's nights warm the screen */
static int same_minutes(int from, int until, time_t day0, const char *what)
{
	night_cfg c = { NIGHT_HOURS, 8, from, until };
	panelgamma_night n;
	int m, wrong = 0;

	night_theme_hours(&c);
	for (m = 0; m < 24 * 60; m++) {
		time_t t = day0 + (time_t)m * 60;
		int warm, dark;

		night_schedule(&c, t, &n);
		warm = panelgamma_night_at(&n, (long long)t) > 0;
		dark = auto_at(t) == DS_DARK;
		if (warm != dark && wrong++ < 3) {
			struct tm tm;

			localtime_r(&t, &tm);
			printf("  %s: at %02d:%02d Night Mode %s, Auto %s\n", what, tm.tm_hour, tm.tm_min,
			       warm ? "warm" : "off", dark ? "dark" : "light");
		}
	}
	return !wrong;
}

static void one_schedule(void)
{
	printf("one schedule for both:\n");
	zone("UTC");
	ck(same_minutes(21 * 60, 7 * 60, at(0, 0, 0), "21:00-7:00"), "21:00 to 7:00: every minute of a day");
	ck(same_minutes(21 * 60 + 15, 6 * 60 + 45, at(0, 0, 0), "21:15-6:45"), "21:15 to 6:45");
	ck(same_minutes(13 * 60, 15 * 60, at(0, 0, 0), "13:00-15:00"), "13:00 to 15:00");
	ck(same_minutes(0, 30, at(0, 0, 0), "0:00-0:30"), "0:00 to 0:30");
	ck(same_minutes(9 * 60, 9 * 60, at(0, 0, 0), "9:00-9:00"), "the same two: no night, no dark");
	/* New York's day summer time ends, 2026-11-01, local minute by minute */
	zone("America/New_York");
	{
		struct tm tm;

		memset(&tm, 0, sizeof tm);
		tm.tm_year = 2026 - 1900;
		tm.tm_mon = 10;
		tm.tm_mday = 1;
		tm.tm_isdst = -1;
		ck(same_minutes(21 * 60, 7 * 60, mktime(&tm), "New York"),
		   "and in New York the day summer time ends");
	}
	zone("UTC");
}

static void row(void)
{
	printf("the row:\n");
	ck(appearance_step(APPEAR_LIGHT, 1) == APPEAR_DARK && appearance_step(APPEAR_DARK, 1) == APPEAR_AUTO &&
	   appearance_step(APPEAR_AUTO, 1) == APPEAR_LIGHT, "Right: Light, Dark, Auto, Light");
	ck(appearance_step(APPEAR_LIGHT, -1) == APPEAR_AUTO && appearance_step(APPEAR_AUTO, -1) == APPEAR_DARK,
	   "Left: Light, Auto, Dark");
	ck(!strcmp(appearance_id(APPEAR_AUTO), "auto") && appearance_parse("auto") == APPEAR_AUTO,
	   "kept as auto, read back as Auto");
	/* portkeys over a port: the saved word and the hours as it reads them */
	theme_set_hours(7 * 60, 21 * 60);
	ck(theme_saved_at("auto", at(20, 59, 0)) == DS_LIGHT && theme_saved_at("auto", at(21, 0, 0)) == DS_DARK,
	   "over a port (theme_saved_at): 20:59 light, 21:00 dark");
}

static void migration(void)
{
	char w[16];

	printf("the migration:\n");
	zone("UTC");
	/* A 1.0.5 card on Auto that never set its hours */
	fresh();
	db_set_str(db_dev(), "appearance", "auto");
	ck(start() == APPEAR_AUTO, "a 1.0.5 card on Auto starts on Auto");
	ck(db_get_int(db_dev(), "night.from", -1) == 19 * 60 && db_get_int(db_dev(), "night.until", -1) == 7 * 60,
	   "its Night Hours 1.0.5's, 19:00 to 7:00");
	ck(auto_at(at(18, 59, 0)) == DS_LIGHT && auto_at(at(19, 0, 0)) == DS_DARK && auto_at(at(7, 0, 0)) == DS_LIGHT,
	   "so its theme turns at 19:00 and 7:00 as it did");
	ck(!db_has(db_dev(), "night.mode"), "and Night Mode is not turned on");
	ck(db_get_int(db_dev(), AUTO_MIGRATED_KEY, 0) == 1, "the card is marked done");

	/* A 1.0.5 card on Auto with its own hours */
	fresh();
	db_set_str(db_dev(), "appearance", "auto");
	db_set_int(db_dev(), "auto.light", 6 * 60 + 30);
	db_set_int(db_dev(), "auto.dark", 22 * 60);
	ck(start() == APPEAR_AUTO && db_get_int(db_dev(), "night.from", -1) == 22 * 60 &&
	   db_get_int(db_dev(), "night.until", -1) == 6 * 60 + 30 && !db_has(db_dev(), "auto.light") &&
	   !db_has(db_dev(), "auto.dark"), "its own hours (6:30, 22:00): Auto, Night Hours 22:00 to 6:30, auto.* gone");

	/* A 1.0.6 card, Light (its Auto turned Light by 1.0.6), no mark yet */
	fresh();
	db_set_str(db_dev(), "appearance", "light");
	ck(start() == APPEAR_LIGHT && !db_has(db_dev(), "night.from"), "a 1.0.6 card on Light stays Light, no hours");
	ck(db_get_int(db_dev(), AUTO_MIGRATED_KEY, 0) == 1, "and is marked at its first 1.1.0 start");
	/* ...whose player picks Auto, the menu saving the word */
	db_set_str(db_dev(), "appearance", appearance_id(APPEAR_AUTO));
	ck(start() == APPEAR_AUTO, "Auto picked in 1.1.0: the next start is Auto");
	ck(!db_has(db_dev(), "night.from") && !db_has(db_dev(), "night.until"),
	   "and its hours are not written: not taken for a 1.0.5 card's 19:00");
	ck(auto_at(at(19, 0, 0)) == DS_LIGHT && auto_at(at(21, 0, 0)) == DS_DARK,
	   "so it turns at 21:00, the Night Hours it was picked with");
	{
		int k, ok = 1;

		for (k = 0; k < 5; k++) {
			if (start() != APPEAR_AUTO) ok = 0;
			db_get_str(db_dev(), "appearance", w, sizeof w, "");
			if (strcmp(w, "auto") || db_has(db_dev(), "night.from")) ok = 0;
		}
		ck(ok, "five starts more: still auto, still no hours written");
	}
	/* With hours of its own from the page, kept through starts */
	night_keep(&(night_cfg){ NIGHT_OFF, 8, 20 * 60 + 30, 6 * 60 });
	start();
	start();
	ck(db_get_int(db_dev(), "night.from", -1) == 20 * 60 + 30 && db_get_int(db_dev(), "night.until", -1) == 6 * 60,
	   "and hours set on the page stay theirs through starts");
	/* A new card: nothing but the mark */
	fresh();
	ck(start() == APPEAR_LIGHT && !db_has(db_dev(), "night.from") && db_get_int(db_dev(), AUTO_MIGRATED_KEY, 0) == 1,
	   "a new card: Light, no hours, marked");
}

static void boot(void)
{
	char line[BOOTTHEME_MAX], file[BOOTTHEME_MAX], said[BOOTTHEME_MAX];
	int pairs[][2] = { { 21 * 60, 7 * 60 }, { 21 * 60 + 15, 6 * 60 + 45 }, { 13 * 60, 15 * 60 }, { 0, 30 },
	                   { 9 * 60, 9 * 60 }, { 23 * 60 + 59, 0 } };
	size_t i;

	printf("the boot screen:\n");
	boottheme_words("auto", 21 * 60, 7 * 60, "America/Sao_Paulo", "pt-BR", 74, line, sizeof line,
	                file, sizeof file);
	ck(!strcmp(line, "zytheme=auto-1260-420 zyzone=America/Sao_Paulo zylang=pt-BR zybright=74"),
	   "Auto's word carries the Night Hours: auto-1260-420");
	ck(!strcmp(file, "zytheme=auto-1260-420\nzyzone=America/Sao_Paulo\nzylang=pt-BR\nzybright=74\n"),
	   "and theme.txt, a line each");
	boottheme_words("auto", -1, 99999, "UTC", "en", 74, line, sizeof line, NULL, 0);
	ck(!strncmp(line, "zytheme=auto-1260-420 ", 22), "hours off the day: the ones that ship");
	boottheme_words("dark", 21 * 60, 7 * 60, "UTC", "en", 74, line, sizeof line, NULL, 0);
	ck(!strncmp(line, "zytheme=dark ", 13), "Dark carries no hours");
	boottheme_from_cmdline("root=/dev/mmcblk0p2 zytheme=auto-1439-1439 zyzone=America/Argentina/ComodRivadavia "
	                       "zylang=zh-Hans zybright=255", said, sizeof said);
	boottheme_words("auto", 1439, 1439, "America/Argentina/ComodRivadavia", "zh-Hans", 255, line, sizeof line,
	                NULL, 0);
	ck(boottheme_same(line, said), "the longest word read back from the kernel's line is the same words");
	boottheme_words("auto", 22 * 60, 7 * 60, "UTC", "en", 74, line, sizeof line, NULL, 0);
	ck(!boottheme_same(line, said), "and new hours are new words, so theme.txt is written again");

	/* The splash, every minute of the day, against the launcher */
	for (i = 0; i < sizeof pairs / sizeof pairs[0]; i++) {
		char word[32];
		int m, wrong = 0;

		boottheme_words("auto", pairs[i][0], pairs[i][1], "UTC", "en", 74, line, sizeof line, NULL, 0);
		sscanf(line, "zytheme=%31s", word);
		theme_set_hours(pairs[i][1], pairs[i][0]);
		for (m = 0; m < 24 * 60; m++)
			if (splash_zytheme_light(word, m) != (theme_resolve_min(APPEAR_AUTO, m) == DS_LIGHT)) wrong++;
		snprintf(said, sizeof said, "the splash reads %s as the launcher's Auto, every minute", word);
		ck(!wrong, said);
	}
	ck(splash_zytheme_light("auto", 18 * 60 + 59) && !splash_zytheme_light("auto", 19 * 60) &&
	   splash_zytheme_light("auto", 7 * 60), "plain auto, a 1.0.5 launcher's file: 7:00 to 19:00 light, as then");
	ck(!splash_zytheme_light("dark", 12 * 60) && splash_zytheme_light("light", 0) && splash_zytheme_light(NULL, 0) &&
	   splash_zytheme_light("-", 0), "dark, light, none and - as before");
	ck(splash_zytheme_light("auto-1260", 22 * 60) && splash_zytheme_light("auto-1260-420x", 22 * 60) &&
	   splash_zytheme_light("auto-1440-420", 22 * 60) && splash_zytheme_light("autox", 22 * 60) &&
	   splash_zytheme_light("auto--1-420", 22 * 60), "a word cut or gone wrong: Beard, the default");
	ck(splash_zytheme_light("auto-1260-420", -1), "no clock: Beard");
}

/* Rule 1: Auto writes no night.* key */
static void rule_no_night_writes(void)
{
	char before[512], after[512];
	int calls, h, ok = 1;
	time_t t;

	printf("rule 1, Auto writes no night.* key:\n");
	zone("UTC");
	fresh();
	db_set_str(db_dev(), AUTO_MIGRATED_KEY, "1");
	db_set_str(db_dev(), "appearance", "light");
	db_set_str(db_dev(), "night.mode", "hours");
	db_set_int(db_dev(), "night.warmth", 5);
	db_set_int(db_dev(), "night.from", 22 * 60 + 30);
	db_set_int(db_dev(), "night.until", 6 * 60);
	start();
	night_keys(before);
	calls = night_calls;
	/* Auto picked, as the row does: the theme, and the word kept */
	theme_init_at(APPEAR_AUTO, at(12, 0, 0));
	db_set_str(db_dev(), "appearance", appearance_id(APPEAR_AUTO));
	night_keys(after);
	ck(!strcmp(before, after), "picking Auto");
	/* A day and a half of the launcher's passes, a second apart round both
	 * edges and a minute apart between */
	for (t = at(12, 0, 0); t < at(12, 0, 0) + 36 * 3600; t += 60) {
		theme_tick_at(t);
		for (h = 0; h < 4; h++) theme_tick_at(t + 1 + h * 13);
	}
	night_keys(after);
	ok &= !strcmp(before, after);
	ck(ok, "the hours passing, a day and a half of ticks across 22:30 and 6:00");
	{
		int k;

		for (k = 0; k < 4; k++) start();
	}
	night_keys(after);
	ck(!strcmp(before, after), "four starts on Auto");
	theme_init_at(APPEAR_DARK, at(23, 0, 0));
	db_set_str(db_dev(), "appearance", appearance_id(APPEAR_DARK));
	start();
	theme_init_at(APPEAR_LIGHT, at(23, 0, 0));
	db_set_str(db_dev(), "appearance", appearance_id(APPEAR_LIGHT));
	start();
	night_keys(after);
	ck(!strcmp(before, after), "leaving Auto, for Dark and for Light, and a start on each");
	ck(night_calls == calls, "and not one call to the display's table on the way");
	ck(!strcmp(before, "night.from=1350;night.mode=hours;night.until=360;night.warmth=5;"),
	   "the settings as kept: By Hours, 5, 22:30 to 6:00");

	/* A card with no Night Mode settings at all: none appear */
	fresh();
	db_set_str(db_dev(), AUTO_MIGRATED_KEY, "1");
	db_set_str(db_dev(), "appearance", "auto");
	start();
	for (t = at(20, 0, 0); t < at(20, 0, 0) + 12 * 3600; t += 30) theme_tick_at(t);
	start();
	night_keys(after);
	ck(!after[0], "a card with none: Auto, starts and the night's ticks write none");
}

/* Rule 2: Night Mode the same under every Appearance */
static void rule_night_mode_same(void)
{
	static const int hours[] = { 12 * 60, 21 * 60 + 59, 22 * 60, 23 * 60 + 30, 3 * 60,
	                             5 * 60 + 59, 6 * 60, 6 * 60 + 1 };
	const int NT = (int)(sizeof hours / sizeof hours[0]);
	int got[APPEARANCES][NIGHT_MODES][8];
	night_cfg handed[APPEARANCES][NIGHT_MODES], loaded[APPEARANCES][NIGHT_MODES];
	int a, m, k, same = 1, right = 1;

	printf("rule 2, Night Mode the same under Light, Dark and Auto:\n");
	zone("UTC");
	for (a = 0; a < APPEARANCES; a++) {
		fresh();
		db_set_str(db_dev(), AUTO_MIGRATED_KEY, "1");
		db_set_str(db_dev(), "appearance", appearance_id((appearance)a));
		start();
		theme_init_at((appearance)a, at(12, 0, 0));
		for (m = 0; m < NIGHT_MODES; m++) {
			night_cfg c = { (night_mode)m, 6, 22 * 60, 6 * 60 };
			panelgamma_night n;

			night_keep(&c);   /* Night Mode's page */
			handed[a][m] = night_handed;
			start();          /* and a start, which reads it back */
			loaded[a][m] = night_load();
			for (k = 0; k < NT; k++) {
				time_t t = at(hours[k] / 60, hours[k] % 60, 0);

				/* the hours passing under the theme, as a pass does */
				theme_tick_at(t);
				night_schedule(&loaded[a][m], t, &n);
				got[a][m][k] = panelgamma_night_at(&n, (long long)t);
			}
		}
	}
	for (a = 1; a < APPEARANCES; a++)
		for (m = 0; m < NIGHT_MODES; m++) {
			if (memcmp(&handed[a][m], &handed[0][m], sizeof handed[0][m]) ||
			    memcmp(&loaded[a][m], &loaded[0][m], sizeof loaded[0][m]) ||
			    memcmp(got[a][m], got[0][m], sizeof got[0][m])) {
				same = 0;
				printf("  %s, Night Mode %s: not as under Light\n", appearance_id((appearance)a),
				       night_mode_id((night_mode)m));
			}
		}
	for (k = 0; k < NT; k++) {
		const bool night = hours[k] >= 22 * 60 || hours[k] < 6 * 60;

		if (got[0][NIGHT_OFF][k] != 0 || got[0][NIGHT_ON][k] != 6 || got[0][NIGHT_HOURS][k] != (night ? 6 : 0))
			right = 0;
	}
	ck(same, "3 appearances x 3 modes x 8 times: the same warmth, the same settings kept and handed on");
	ck(right, "and the right ones: Off none, On 6 always, By Hours 6 from 22:00 up to 6:00");
}

/* Rule 3: at From and Until, the theme and the warmth on the same second.
 * The launcher's pass, a second at a time: Night Mode's warmth by
 * plat_night_tick's rule (the nights planned once a minute, the warmth the
 * clock is in), the theme by theme_tick. Each run starts at an odd second,
 * so the theme's once-a-minute look never falls on an edge by chance. */
static int together(time_t from_t, time_t to_t, const night_cfg *c, const char *what)
{
	panelgamma_night n;
	time_t t, planned = 0;
	int warm, was_warm = -1, turns = 0, wrong = 0, calls = night_calls;

	night_theme_hours(c);
	theme_init_at(APPEAR_AUTO, from_t);
	for (t = from_t; t <= to_t; t++) {
		bool turned;

		if (!planned || t - planned >= 60) {
			night_schedule(c, t, &n);
			planned = t;
		}
		if (n.n > 0 && (long long)t >= n.until[0]) night_schedule(c, t, &n);
		warm = panelgamma_night_at(&n, (long long)t);
		turned = theme_tick_at(t);
		if (turned) turns++;
		if (was_warm >= 0 && (warm != was_warm) != turned) {
			if (wrong++ < 3)
				printf("  %s: at +%lds the warmth %s and the theme %s\n", what, (long)(t - from_t),
				       warm != was_warm ? "changed" : "stayed", turned ? "turned" : "stayed");
		}
		if ((theme_index() == DS_DARK) != (warm > 0)) {
			if (wrong++ < 3) printf("  %s: at +%lds warm %d, theme %s\n", what, (long)(t - from_t), warm,
			                        theme_index() == DS_DARK ? "dark" : "light");
		}
		was_warm = warm;
	}
	if (night_calls != calls) {
		wrong++;
		printf("  %s: the theme asked for the table %d times\n", what, night_calls - calls);
	}
	return wrong ? 0 : turns;
}

static void rule_together(void)
{
	night_cfg c = { NIGHT_HOURS, 8, 22 * 60, 6 * 60 };

	printf("rule 3, the theme and the warmth turn together:\n");
	zone("UTC");
	ck(together(at(21, 57, 37), at(22, 2, 0), &c, "22:00") == 1, "at 22:00:00 both, one turn, no table asked by the theme");
	ck(together(at(5, 57, 41), at(6, 2, 0), &c, "6:00") == 1, "at 6:00:00 both");
	ck(together(at(12, 0, 23), at(12, 0, 23) + 26 * 3600, &c, "a day") == 2,
	   "a whole day a second at a time: two turns, each on the warmth's second");
	c.from = 21 * 60 + 17;
	c.until = 6 * 60 + 43;
	ck(together(at(21, 16, 30), at(21, 18, 0), &c, "21:17") == 1, "and at a minute set by hand, 21:17");
	zone("America/New_York");
	{
		struct tm tm;
		time_t eve;

		memset(&tm, 0, sizeof tm);
		tm.tm_year = 2026 - 1900;
		tm.tm_mon = 9;
		tm.tm_mday = 31;
		tm.tm_hour = 21;
		tm.tm_isdst = -1;
		eve = mktime(&tm);
		c.from = 22 * 60;
		c.until = 6 * 60;
		ck(together(eve + 13, eve + 11 * 3600, &c, "New York") == 2,
		   "New York, the night summer time ends: 22:00 EDT and 6:00 EST, both together");
	}
	zone("UTC");
}

/* Rule 4: a 1.0.6 card's Night Mode settings through its first 1.1.0 start */
static void rule_106_card(void)
{
	char before[512], after[512];
	static const char *looks[] = { "light", "dark" };
	size_t i;

	printf("rule 4, a 1.0.6 card's Night Mode untouched:\n");
	zone("UTC");
	for (i = 0; i < 2; i++) {
		fresh();
		db_set_str(db_dev(), "appearance", looks[i]);
		db_set_str(db_dev(), "night.mode", i ? "on" : "hours");
		db_set_int(db_dev(), "night.warmth", 3);
		db_set_int(db_dev(), "night.from", 21 * 60 + 30);
		db_set_int(db_dev(), "night.until", 6 * 60 + 30);
		night_keys(before);
		start();
		start();
		night_keys(after);
		ck(!strcmp(before, after), i ? "on Dark, Night Mode On: every night.* key as it was"
		                             : "on Light, Night Mode By Hours: every night.* key as it was");
	}
	/* Its hours only, Night Mode Off (1.0.6 kept them from a 1.0.5 Auto) */
	fresh();
	db_set_str(db_dev(), "appearance", "light");
	db_set_int(db_dev(), "night.from", 19 * 60);
	db_set_int(db_dev(), "night.until", 7 * 60);
	night_keys(before);
	start();
	night_keys(after);
	ck(!strcmp(before, after), "Night Mode Off with hours kept: as it was");
	/* None at all */
	fresh();
	db_set_str(db_dev(), "appearance", "dark");
	start();
	night_keys(after);
	ck(!after[0], "none at all: none written");
}

int main(void)
{
	char dir[] = "/tmp/autotheme-check-XXXXXX", dev[64], lib[64];

	theme_set_file(NULL);
	if (!mkdtemp(dir)) {
		printf("autotheme: no scratch folder\n");
		return 1;
	}
	snprintf(dev, sizeof dev, "%s/zyperos.db", dir);
	snprintf(lib, sizeof lib, "%s/library.db", dir);
	if (!db_init(dev, lib, NULL)) {
		printf("autotheme: the databases do not open\n");
		return 1;
	}
	hours();
	page();
	one_schedule();
	row();
	migration();
	boot();
	rule_no_night_writes();
	rule_night_mode_same();
	rule_together();
	rule_106_card();
	db_shutdown();
	unlink(dev);
	unlink(lib);
	rmdir(dir);
	if (fails) {
		printf("autotheme: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("autotheme: %d checks passed\n", checks);
	return 0;
}
