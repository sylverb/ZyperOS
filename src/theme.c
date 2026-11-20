/* SPDX-License-Identifier: GPL-3.0-only */
/* Which theme is on. See src/theme.h. */
#include "theme.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "i18n.h"
#include "look.h"
#include "themefile.h"

/* How long the clock is trusted for between edges, in seconds: the edges
 * themselves are foreseen (next_edge), and this is for a clock that moved
 * under them. */
#define RECHECK_S 60

/* The state, which is three facts and the place the fourth is written. Static
 * and not a struct the caller holds: there is one theme at a time and every
 * screen asks for it, and a handle every draw call had to be passed would be
 * the whole of ui.c's signatures changing for nothing. */
static appearance cur = APPEAR_LIGHT;
static int idx = DS_LIGHT;
/* Auto's hours, minutes into the local day: the Night Hours' (theme_set_hours) */
static int light_from = THEME_NIGHT_UNTIL, dark_from = THEME_NIGHT_FROM;
static time_t looked;          /* when the clock was last read, for Auto */
static time_t edge;            /* the next turn after `looked` (next_edge), 0 none */
static const char *file = THEME_FILE;

int theme_resolve_min(appearance a, int minute)
{
	switch (a) {
	case APPEAR_DARK:  return DS_DARK;
	case APPEAR_AUTO:
		/* Light from light_from up to dark_from, and dark from there round
		 * to light_from: 7:00 up to 21:00 as it ships, the Night Hours
		 * round midnight. Hours inside a day (dark from 13:00 to 15:00) are
		 * the same rule the other way; the same hour twice is never dark.
		 * Not a clock that could not be read: see theme.h. */
		if (minute < 0 || minute >= 24 * 60) return DS_LIGHT;
		if (light_from == dark_from) return DS_LIGHT;
		if (light_from < dark_from)
			return minute >= light_from && minute < dark_from ? DS_LIGHT : DS_DARK;
		return minute >= light_from || minute < dark_from ? DS_LIGHT : DS_DARK;
	case APPEAR_LIGHT:
	default:           return DS_LIGHT;
	}
}

int theme_resolve(appearance a, int hour)
{
	return theme_resolve_min(a, hour < 0 || hour > 23 ? -1 : hour * 60);
}

/* Any minute of the day, as the Night Hours keep it (night_cfg_clamp): the
 * page steps them by THEME_HOURS_STEP, and a minute set otherwise is kept,
 * so the theme turns at the minute Night Mode does */
static int hours_clamp(int m, int fallback)
{
	return m >= 0 && m < 24 * 60 ? m : fallback;
}

static time_t next_edge(time_t t);

void theme_set_hours(int light_min, int dark_min)
{
	light_from = hours_clamp(light_min, THEME_NIGHT_UNTIL);
	dark_from = hours_clamp(dark_min, THEME_NIGHT_FROM);
	/* New hours, new edges: the next from the last look, which a tick at or
	 * past it looks at again (the caller that changed them calls
	 * theme_refresh, to turn it at once) */
	if (cur == APPEAR_AUTO) edge = next_edge(looked);
}

void theme_hours(int *light_min, int *dark_min)
{
	if (light_min) *light_min = light_from;
	if (dark_min) *dark_min = dark_from;
}

const char *appearance_name(appearance a)
{
	switch (a) {
	case APPEAR_DARK: return T(S_THEME_DARK);
	case APPEAR_AUTO: return T(S_THEME_AUTO);
	case APPEAR_LIGHT:
	default:          return T(S_THEME_LIGHT);
	}
}

const char *appearance_id(appearance a)
{
	switch (a) {
	case APPEAR_DARK: return "dark";
	case APPEAR_AUTO: return "auto";
	case APPEAR_LIGHT:
	default:          return "light";
	}
}

appearance appearance_parse(const char *s)
{
	if (!s) return APPEAR_LIGHT;
	/* The whole word and nothing else: strcasecmp is a comparison of the two
	 * strings to their ends, so "darkness" and "dark " are not "dark". */
	if (!strcasecmp(s, "dark")) return APPEAR_DARK;
	if (!strcasecmp(s, "auto")) return APPEAR_AUTO;
	return APPEAR_LIGHT;
}

appearance appearance_step(appearance a, int d)
{
	int i = (int)a;

	/* Three of them in a ring, as up to 1.0.5 and again since 1.1.0. The
	 * remainder of a negative number is negative in C, hence the second
	 * modulus. */
	if (i < 0 || i >= APPEARANCES) i = APPEAR_LIGHT;
	return (appearance)((((i + d) % APPEARANCES) + APPEARANCES) % APPEARANCES);
}

/* The local minute of the day of `t`, or -1 if the time cannot be made into
 * one (a time_t so far from now that the year does not fit), which
 * theme_resolve_min reads as the default and not as night. */
static int minute_of(time_t t)
{
	struct tm tm;

	if (!localtime_r(&t, &tm)) return -1;
	return tm.tm_hour * 60 + tm.tm_min;
}

/* The first time after `t` that Auto's theme can turn: the next local
 * light_from or dark_from, today, tomorrow or the day after, each through
 * mktime (tm_isdst -1) as night_schedule finds Night Mode's nights, so the two
 * meet at the same second across summer time's ends too. 0 for none: the same
 * two hours never turn it. */
static time_t next_edge(time_t t)
{
	struct tm today;
	time_t best = 0;
	int d, k;

	if (light_from == dark_from || !localtime_r(&t, &today)) return 0;
	for (d = 0; d <= 2; d++)
		for (k = 0; k < 2; k++) {
			const int m = k ? dark_from : light_from;
			struct tm x = today;
			time_t e;

			x.tm_mday += d;
			x.tm_hour = m / 60;
			x.tm_min = m % 60;
			x.tm_sec = 0;
			x.tm_isdst = -1;
			e = mktime(&x);
			if (e != (time_t)-1 && e > t && (!best || e < best)) best = e;
		}
	return best;
}

/* A look at the clock: when, and the next edge from there */
static void look_at(time_t now)
{
	looked = now;
	edge = cur == APPEAR_AUTO ? next_edge(now) : 0;
}

int theme_saved_at(const char *saved, time_t now)
{
	/* theme_init_at's reading, without its state: the word as the launcher
	 * parses it, and the clock asked only when the setting is Auto. */
	appearance a = appearance_parse(saved);

	return theme_resolve_min(a, a == APPEAR_AUTO ? minute_of(now) : 0);
}

void theme_set_file(const char *path)
{
	file = path;
}

/* Says the theme that is on to the programs that read the file, or says on
 * stderr why not. Never more than that, as theme.h says why. */
/* The theme the file last said (publish), -1 before it said one */
static int said = -1;

static void publish(void)
{
	said = idx;
	if (!file) return;
	/* with the bar's colors when the player's look has its own (look.h) */
	if (!themefile_write_bar(file, idx, look_is_default(look_now()) ? NULL : theme_at(idx)))
		fprintf(stderr, "theme: %s: %s\n", file, strerror(errno));
}

void theme_init_at(appearance a, time_t now)
{
	/* A setting that is not one of the three is stored as the Light it acts
	 * as, so theme_appearance() never hands the menu a number it has no name
	 * for. */
	cur = a == APPEAR_DARK || a == APPEAR_AUTO ? a : APPEAR_LIGHT;
	look_at(now);
	idx = theme_resolve_min(cur, cur == APPEAR_AUTO ? minute_of(now) : 0);
	publish();
}

void theme_init(appearance a)
{
	theme_init_at(a, time(NULL));
}

void theme_show(int ix)
{
	if (ix >= 0) {
		idx = theme_clamp(ix);
		return;
	}
	/* Back: Auto's hour may have turned while a theme was shown, the
	 * screens' tick waiting (ds_hover_showing), so the file is said again
	 * where it is behind */
	look_at(time(NULL));
	idx = theme_resolve_min(cur, cur == APPEAR_AUTO ? minute_of(looked) : 0);
	if (said >= 0 && idx != said) publish();
}

bool theme_refresh_at(time_t now)
{
	int next;

	if (cur != APPEAR_AUTO) return false;

	look_at(now);
	next = theme_resolve_min(cur, minute_of(now));
	if (next == idx) return false;
	idx = next;
	publish();
	return true;
}

bool theme_refresh(void)
{
	return theme_refresh_at(time(NULL));
}

bool theme_tick_at(time_t now)
{
	if (cur != APPEAR_AUTO) return false;
	/* Trusted for a minute from the last look, but never past the next
	 * edge: at its second the theme turns, on the pass Night Mode's warmth
	 * does. A time before the last look is a clock that was set back, by
	 * hand in Date & Time: the minute it was trusted for belongs to a time
	 * that has not come, so it is read again. */
	if (now >= looked && now - looked < RECHECK_S && !(edge && now >= edge)) return false;
	return theme_refresh_at(now);
}

bool theme_tick(void)
{
	return theme_tick_at(time(NULL));
}

const ds_theme *theme(void)
{
	return theme_at(idx);
}

int theme_index(void)
{
	return idx;
}

appearance theme_appearance(void)
{
	return cur;
}
