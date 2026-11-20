/* SPDX-License-Identifier: GPL-3.0-only */
/* Night Mode (src/nightmode.h): its warmth in the display's table, the
 * nights the launcher writes for Diatom and portkeys, and the first start's
 * migration from Appearance's Auto, on a real database.
 *
 *   make check-nightmode
 *
 * The nights are worked out in the zone TZ names; the check sets it, so it
 * holds anywhere. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../src/db.h"
#include "../src/nightkeep.h"
#include "../src/nightmode.h"
#include "../src/panelgamma.h"
#include "../src/theme.h"

static int checks, fails;

static void ck(int ok, const char *what)
{
	checks++;
	if (!ok) {
		fails++;
		printf("  FAIL %s\n", what);
	}
}

/* plat_night, as night_keep calls it: what it was given */
static night_cfg kept_on_display;
static int kept_calls;

void plat_night(const struct night_cfg *n)
{
	kept_on_display = *n;
	kept_calls++;
}

static void zone(const char *tz)
{
	setenv("TZ", tz, 1);
	tzset();
}

static int night_is(const panelgamma_night *n, int i, long long from, long long until)
{
	return i < n->n && n->from[i] == from && n->until[i] == until;
}

static void table(void)
{
	panelgamma_color temp8 = { 8, 0, 0, 0, 0 }, night8 = { 0, 0, 0, 0, 8 };
	panelgamma_color both = { 5, 0, 0, 0, 8 }, none = { 0 };
	int ch, same = 1;

	printf("the table:\n");
	for (ch = 0; ch < 3; ch++)
		same &= panelgamma_color_out(200, &temp8, ch, 255) == panelgamma_color_out(200, &night8, ch, 255) &&
		        panelgamma_color_out(200, &temp8, ch, 128) == panelgamma_color_out(200, &night8, ch, 128);
	ck(same, "Night 8 alone is Temperature +8's table");
	ck(panelgamma_color_out(200, &both, 2, 255) ==
	   (int)lround(255 * pow((1 - 0.055 * 5) * (1 - 0.055 * 8), 1 / 2.2)),
	   "over Temperature +5 the gains multiply: white's blue at (0.725 x 0.56)^(1/2.2)");
	ck(panelgamma_color_out(200, &both, 0, 255) == 255, "and white's red stays 255");
	ck(!panelgamma_color_standard(&night8), "Night alone is not Standard");
	ck(panelgamma_color_standard(&none), "and none is");
	none.night = 99;
	panelgamma_color_clamp(&none);
	ck(none.night == PANELGAMMA_NIGHT_MAX, "a warmth past 10 is 10");
	none.night = -3;
	panelgamma_color_clamp(&none);
	ck(none.night == 0, "and under 0 is none");
}

static void file(void)
{
	char dir[] = "/tmp/nightmode-check-XXXXXX", path[64], line[256] = "";
	panelgamma_color c = { 1, -2, 3, -4, 7 }, got;
	panelgamma_night n = { 8, 0, 2, { 1794949200, 1795035600 }, { 1794985200, 1795071600 } }, gn;
	FILE *f;

	printf("the file:\n");
	if (!mkdtemp(dir)) {
		ck(0, "a scratch folder");
		return;
	}
	snprintf(path, sizeof path, "%s/screen", dir);
	ck(panelgamma_screen_write(path, &c, &n) == 0, "written with two nights");
	if ((f = fopen(path, "r"))) {
		if (!fgets(line, sizeof line, f)) line[0] = 0;
		fclose(f);
	}
	ck(!strcmp(line, "temp=1 tint=-2 contrast=3 gamma=-4 warmth=8 "
	                 "nights=1794949200-1794985200,1795035600-1795071600\n"),
	   "one line: the colors, the warmth and the nights, no night= of its own");
	ck(panelgamma_screen_read(path, &got, &gn) == 1 && got.temp == 1 && got.tint == -2 &&
	   got.contrast == 3 && got.gamma == -4 && got.night == 0, "read back: the colors, night 0");
	ck(gn.warmth == 8 && !gn.always && gn.n == 2 && night_is(&gn, 0, 1794949200, 1794985200) &&
	   night_is(&gn, 1, 1795035600, 1795071600), "and the warmth and both nights");
	ck(panelgamma_color_read(path, &got) == 1 && got.temp == 1 && got.gamma == -4,
	   "a reader of the colors alone skips the night words");
	n.always = 1;
	panelgamma_screen_write(path, &c, &n);
	panelgamma_screen_read(path, &got, &gn);
	ck(gn.always && gn.warmth == 8, "On: always");
	panelgamma_screen_write(path, &c, NULL);
	if ((f = fopen(path, "r"))) {
		if (!fgets(line, sizeof line, f)) line[0] = 0;
		fclose(f);
	}
	ck(!strcmp(line, "temp=1 tint=-2 contrast=3 gamma=-4\n"), "no Night Mode: the 1.0.6 line as before");
	panelgamma_screen_read(path, &got, &gn);
	ck(gn.warmth == 0 && gn.n == 0 && !gn.always, "and read as off");
	if ((f = fopen(path, "w"))) {
		fputs("temp=0 warmth=5 nights=20-10,30-40,junk\n", f);
		fclose(f);
	}
	panelgamma_screen_read(path, &got, &gn);
	ck(gn.warmth == 5 && gn.n == 1 && night_is(&gn, 0, 30, 40),
	   "a night that ends before it starts is none, and what follows junk is not read");
	ck(panelgamma_screen_read("/nonexistent/screen", &got, &gn) == 0 && gn.warmth == 0,
	   "no file: off");
	unlink(path);
	rmdir(dir);

	/* The clock against the nights */
	n.always = 0;
	ck(panelgamma_night_at(&n, 1794949200 - 1) == 0, "a second before the night: off");
	ck(panelgamma_night_at(&n, 1794949200) == 8, "at its start: on");
	ck(panelgamma_night_at(&n, 1794985200 - 1) == 8, "a second before its end: on");
	ck(panelgamma_night_at(&n, 1794985200) == 0, "at its end: off");
	ck(panelgamma_night_next(&n, 1794916800) == 1794949200, "the next change from noon: 21:00");
	ck(panelgamma_night_next(&n, 1794950000) == 1794985200, "and from inside the night: its end");
	ck(panelgamma_night_next(&n, 1795071600) == 0, "and past the last night: none known");
	n.always = 1;
	ck(panelgamma_night_at(&n, 0) == 8 && panelgamma_night_next(&n, 0) == 0, "always: on, no change");
	ck(panelgamma_night_at(NULL, 0) == 0, "and none: off");
}

static void schedule(void)
{
	night_cfg c = { NIGHT_HOURS, 8, 21 * 60, 7 * 60 };
	panelgamma_night n;

	printf("the nights:\n");
	zone("UTC");
	night_schedule(&c, 1794916800, &n);        /* 2026-11-17 12:00 */
	ck(n.warmth == 8 && !n.always && n.n == 3 && night_is(&n, 0, 1794949200, 1794985200) &&
	   night_is(&n, 1, 1795035600, 1795071600) && night_is(&n, 2, 1795122000, 1795158000),
	   "21:00 to 7:00 at noon: tonight's and the next two, each to the next morning");
	night_schedule(&c, 1794884400, &n);        /* 03:00, inside last night */
	ck(n.n == 3 && night_is(&n, 0, 1794862800, 1794898800) && night_is(&n, 1, 1794949200, 1794985200),
	   "at 3:00: the night it is in first");
	c.from = 13 * 60;
	c.until = 15 * 60;
	night_schedule(&c, 1794916800, &n);
	ck(n.n == 3 && night_is(&n, 0, 1794920400, 1794927600), "13:00 to 15:00: the same day's");
	c.until = c.from;
	night_schedule(&c, 1794916800, &n);
	ck(n.n == 0 && panelgamma_night_at(&n, 1794920400) == 0, "the same hour twice: no night");
	c.mode = NIGHT_ON;
	night_schedule(&c, 1794916800, &n);
	ck(n.always && n.warmth == 8, "On: always");
	c.mode = NIGHT_OFF;
	night_schedule(&c, 1794916800, &n);
	ck(n.warmth == 0 && !n.always && n.n == 0, "Off: nothing");

	/* Summer time's ends: the night as long as the clock says */
	c = (night_cfg){ NIGHT_HOURS, 6, 21 * 60, 7 * 60 };
	zone("America/New_York");
	night_schedule(&c, 1793491200, &n);        /* 2026-10-31 20:00 EDT */
	ck(n.n >= 1 && night_is(&n, 0, 1793494800, 1793534400),
	   "New York, the night summer time ends: 21:00 EDT to 7:00 EST, 11 hours");
	night_schedule(&c, 1772931600, &n);        /* 2026-03-07 20:00 EST */
	ck(n.n >= 1 && night_is(&n, 0, 1772935200, 1772967600),
	   "and the night it starts: 21:00 EST to 7:00 EDT, 9 hours");
	zone("UTC");
}

static void words(void)
{
	night_cfg c = { (night_mode)7, 0, 25 * 60, 7 * 60 + 13 };

	printf("the words:\n");
	ck(night_mode_parse("on") == NIGHT_ON && night_mode_parse("hours") == NIGHT_HOURS &&
	   night_mode_parse("off") == NIGHT_OFF, "on, hours, off");
	ck(night_mode_parse(NULL) == NIGHT_OFF && night_mode_parse("") == NIGHT_OFF &&
	   night_mode_parse("always") == NIGHT_OFF, "anything else is Off");
	ck(!strcmp(night_mode_id(NIGHT_HOURS), "hours") && !strcmp(night_mode_id((night_mode)9), "off"),
	   "and back");
	night_cfg_clamp(&c);
	ck(c.mode == NIGHT_OFF && c.warmth == NIGHT_WARMTH_DEFAULT && c.from == NIGHT_FROM_DEFAULT &&
	   c.until == 7 * 60 + 13, "put right: a mode it does not know, warmth 0 and 25:00; 7:13 kept");
}

static void migrate(void)
{
	night_migration m;

	printf("the migration:\n");
	zone("UTC");
	m = night_migrate("auto", false, 0, false, 0, false);
	ck(m.hours && m.from == 19 * 60 && m.until == 7 * 60 && !m.drop_auto,
	   "Auto, hours never set: Night Hours 19:00 to 7:00, 1.0.5's");
	m = night_migrate("Auto", true, 6 * 60, true, 21 * 60 + 30, false);
	ck(m.from == 21 * 60 + 30 && m.until == 6 * 60 && m.drop_auto,
	   "its own hours (6:00, 21:30): those hours, the keys gone");
	m = night_migrate("dark", false, 0, false, 0, false);
	ck(!m.hours && !m.drop_auto, "Dark and no hours: nothing");
	m = night_migrate("light", true, 8 * 60, true, 20 * 60, false);
	ck(m.hours && m.from == 20 * 60 && m.until == 8 * 60 && m.drop_auto,
	   "Light with hours set once: the hours kept as Night Hours");
	m = night_migrate("auto", true, 8 * 60, true, 20 * 60, true);
	ck(!m.hours && m.drop_auto, "Night Hours kept already: not overwritten");
	m = night_migrate(NULL, false, 0, false, 0, false);
	ck(!m.hours && !m.drop_auto, "a new device: nothing");
}

/* The same on a device database, as the launcher's start runs it */
static void migrate_db(void)
{
	char dir[] = "/tmp/nightmode-db-XXXXXX", dev[64], lib[64], w[16];
	night_cfg n;

	printf("the migration on a database:\n");
	zone("UTC");
	if (!mkdtemp(dir)) {
		ck(0, "a scratch folder");
		return;
	}
	snprintf(dev, sizeof dev, "%s/zyperos.db", dir);
	snprintf(lib, sizeof lib, "%s/library.db", dir);
	if (!db_init(dev, lib, NULL)) {
		ck(0, "the databases open");
		return;
	}
	/* A 1.0.5 device on Auto with its own hours, 7:00 and 19:00 */
	db_set_str(db_dev(), "appearance", "auto");
	db_set_int(db_dev(), "auto.light", 7 * 60);
	db_set_int(db_dev(), "auto.dark", 19 * 60);
	night_migrate_db();
	db_get_str(db_dev(), "appearance", w, sizeof w, "");
	ck(!strcmp(w, "auto"), "Auto stays Auto (1.1.0: a setting again)");
	ck(db_get_int(db_dev(), "night.from", -1) == 19 * 60 && db_get_int(db_dev(), "night.until", -1) == 7 * 60,
	   "Night Hours 19:00 to 7:00");
	ck(!db_has(db_dev(), "auto.light") && !db_has(db_dev(), "auto.dark"), "auto.light and auto.dark gone");
	ck(!db_has(db_dev(), "night.mode"), "Night Mode not written: Off");
	ck(db_get_int(db_dev(), NIGHT_MIGRATED_KEY, 0) == 1, "and the card marked done");
	n = night_load();
	ck(n.mode == NIGHT_OFF && n.warmth == NIGHT_WARMTH_DEFAULT && n.from == 19 * 60 && n.until == 7 * 60,
	   "night_load: Off, warmth 8, 19:00 to 7:00");
	/* A second start changes nothing */
	night_migrate_db();
	db_get_str(db_dev(), "appearance", w, sizeof w, "");
	ck(!strcmp(w, "auto") && db_get_int(db_dev(), "night.from", -1) == 19 * 60,
	   "a second start: still Auto, the same hours");
	/* Kept from the menu, and back */
	night_keep(&(night_cfg){ NIGHT_HOURS, 12, 22 * 60 + 10, 6 * 60 });
	db_get_str(db_dev(), "night.mode", w, sizeof w, "");
	ck(!strcmp(w, "hours") && db_get_int(db_dev(), "night.warmth", -1) == NIGHT_WARMTH_DEFAULT &&
	   db_get_int(db_dev(), "night.from", -1) == 22 * 60 + 10 && db_get_int(db_dev(), "night.until", -1) == 6 * 60,
	   "night_keep: hours, a warmth off the scale the default, 22:10 as it is");
	ck(kept_calls == 1 && kept_on_display.mode == NIGHT_HOURS && kept_on_display.warmth == NIGHT_WARMTH_DEFAULT,
	   "and on the display, put right");
	{
		int l, d;

		theme_hours(&l, &d);
		ck(l == 6 * 60 && d == 22 * 60 + 10, "and Auto's hours: light from 6:00, dark from 22:10");
	}
	n = night_load();
	ck(n.mode == NIGHT_HOURS && n.from == 22 * 60 + 10 && n.until == 6 * 60, "night_load reads it back");
	db_shutdown();
	unlink(dev);
	unlink(lib);
	rmdir(dir);
}

int main(void)
{
	table();
	file();
	schedule();
	words();
	migrate();
	migrate_db();
	if (fails) {
		printf("nightmode: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("nightmode: %d checks passed\n", checks);
	return 0;
}
