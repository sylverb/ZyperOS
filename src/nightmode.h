/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_NIGHTMODE_H
#define ZYPEROS_NIGHTMODE_H
/* Night Mode (1.0.6, MENU > Display): the whole screen warmer, always or by
 * the hours the player sets, over whatever Screen Colors they have.
 *
 * It replaces Appearance's Auto, which turned the theme dark by the same
 * kind of hours (Tarik, after the Screen Colors look, 2026-11-16: "instead
 * of auto hours affecting appearance, let's have it be night mode hours and
 * remove auto from appearance"). Night Mode never changes the theme.
 *
 * The warmth is the display's gamma table's (src/panelgamma.h,
 * panelgamma_color.night): Temperature's gains again, multiplied after the
 * player's. A table load shows one frame with no table, so Night Mode turns
 * on and off at once, one blink at the hour, and never fades: at the two
 * lowest brightness steps, which dim through the table and which are the
 * ones used at night, each step of a fade would flash full brightness.
 *
 * Three programs load the table, whichever holds the display: the launcher,
 * Diatom over a game and portkeys over a port. The launcher works out the
 * next nights in its own time zone and leaves them in PANELGAMMA_COLOR_FILE
 * as UTC seconds (night_schedule); the other two compare the clock with
 * them and need no zone.
 *
 * Plain C, no SDL and no database, so tools/nightmode-check.c links it:
 * the launcher reads and keeps the settings (src/nightkeep.c). */
#include <stdbool.h>
#include <time.h>

#include "panelgamma.h"
#include "theme.h"

/* MENU > Display > Night Mode, as its list orders it */
typedef enum { NIGHT_OFF, NIGHT_ON, NIGHT_HOURS } night_mode;
#define NIGHT_MODES 3

/* Night Warmth, 1 to PANELGAMMA_NIGHT_MAX: 8 is the Night look Screen
 * Colors had before (Temperature +8) */
#define NIGHT_WARMTH_DEFAULT 8
/* Night Hours, minutes into the local day, on NIGHT_HOURS_STEP: from 21:00
 * to 7:00 as it ships (the sheet Tarik approved, 2026-11-16) */
#define NIGHT_FROM_DEFAULT  (21 * 60)
#define NIGHT_UNTIL_DEFAULT (7 * 60)
#define NIGHT_HOURS_STEP    THEME_HOURS_STEP

typedef struct night_cfg {
	night_mode mode;
	int        warmth;      /* 1..PANELGAMMA_NIGHT_MAX */
	int        from, until; /* Night Hours: minutes into the local day */
} night_cfg;

/* Off, 8, 21:00 to 7:00 */
night_cfg night_default(void);

/* The database's words, under night.mode: "off", "on", "hours". Anything
 * else is Off, so a setting gone wrong never warms the screen. */
night_mode night_mode_parse(const char *s);
const char *night_mode_id(night_mode m);

/* n's values put right: a mode it does not know is Off, a warmth off the
 * scale the default, and an hour off the day the default. An hour off
 * NIGHT_HOURS_STEP is kept: the page steps by it, nothing else needs it. */
void night_cfg_clamp(night_cfg *n);

/* The Night Hours' nights about `now`, as the file carries them: the one
 * `now` is in, if any, and the next, up to PANELGAMMA_NIGHTS, each from its
 * start to its end in UTC seconds. A night starts at `from` on a day and
 * ends at the next `until` after it, the next day's when until is earlier
 * (21:00 to 7:00), the same day's when later (13:00 to 15:00). Each edge is
 * the local time in the zone TZ names, through mktime, so a night across a
 * change of summer time is as long as the clock says. From and until the
 * same: no night. On: always. Off: none, warmth 0. */
void night_schedule(const night_cfg *n, time_t now, panelgamma_night *out);

/* The first start of 1.0.6 over a 1.0.5 device's settings: what to write.
 *
 *   - Appearance "auto" becomes the theme Auto gave at `now` by its hours,
 *     "light" or "dark" (Tarik's pick 1: the theme that matches the time).
 *   - Auto's hours become the Night Hours, Light From as Until and Dark
 *     From as From, unless Night Hours are kept already: an Auto player's
 *     hours as they were in force (19:00 to 7:00 if never set), anyone
 *     else's only when they set them.
 *   - Night Mode stays Off: nothing is written for it.
 *   - The auto.light and auto.dark keys go, so this happens once.
 *
 * `appearance` is the saved word (NULL: none); the hours are minutes, with
 * whether each was in the database. Pure: the caller reads and writes. */
typedef struct {
	const char *appearance;   /* "light" or "dark" to write, or NULL */
	bool        hours;        /* write night.from and night.until */
	int         from, until;
	bool        drop_auto;    /* delete auto.light and auto.dark */
} night_migration;

night_migration night_migrate(const char *appearance, bool have_light, int auto_light,
                              bool have_dark, int auto_dark, bool have_night_hours,
                              time_t now);

#endif
