/* SPDX-License-Identifier: GPL-3.0-only */
/* Night Mode's settings, nights and migration. See src/nightmode.h. */
#include "nightmode.h"

#include <string.h>
#include <strings.h>

#include "theme.h"

night_cfg night_default(void)
{
	return (night_cfg){ NIGHT_OFF, NIGHT_WARMTH_DEFAULT, NIGHT_FROM_DEFAULT, NIGHT_UNTIL_DEFAULT };
}

night_mode night_mode_parse(const char *s)
{
	if (s && !strcasecmp(s, "on")) return NIGHT_ON;
	if (s && !strcasecmp(s, "hours")) return NIGHT_HOURS;
	return NIGHT_OFF;
}

const char *night_mode_id(night_mode m)
{
	switch (m) {
	case NIGHT_ON:    return "on";
	case NIGHT_HOURS: return "hours";
	case NIGHT_OFF:
	default:          return "off";
	}
}

/* A minute of the day on `step`, or `fallback` when off the day */
static int minute_clamp(int m, int step, int fallback)
{
	return m >= 0 && m < 24 * 60 ? m - m % step : fallback;
}

void night_cfg_clamp(night_cfg *n)
{
	if (n->mode != NIGHT_ON && n->mode != NIGHT_HOURS) n->mode = NIGHT_OFF;
	if (n->warmth < 1 || n->warmth > PANELGAMMA_NIGHT_MAX) n->warmth = NIGHT_WARMTH_DEFAULT;
	/* Any minute of the day: the page steps them by NIGHT_HOURS_STEP, and a
	 * minute set otherwise (a test's, by hand) is kept as it is */
	n->from = minute_clamp(n->from, 1, NIGHT_FROM_DEFAULT);
	n->until = minute_clamp(n->until, 1, NIGHT_UNTIL_DEFAULT);
}

/* `minute` minutes into the local day `day` (its date only) plus `days`, in
 * the zone TZ names: mktime puts the date right across a month's end and
 * finds summer time's offset for that day itself (tm_isdst -1) */
static time_t local_at(const struct tm *day, int days, int minute)
{
	struct tm t = *day;

	t.tm_mday += days;
	t.tm_hour = minute / 60;
	t.tm_min = minute % 60;
	t.tm_sec = 0;
	t.tm_isdst = -1;
	return mktime(&t);
}

void night_schedule(const night_cfg *cfg, time_t now, panelgamma_night *out)
{
	night_cfg n = *cfg;
	struct tm today;
	int d;

	memset(out, 0, sizeof *out);
	night_cfg_clamp(&n);
	if (n.mode == NIGHT_OFF) return;
	out->warmth = n.warmth;
	if (n.mode == NIGHT_ON) {
		out->always = 1;
		return;
	}
	if (n.from == n.until || !localtime_r(&now, &today)) return;
	/* From yesterday's night, which may not have ended, on */
	for (d = -1; d <= PANELGAMMA_NIGHTS && out->n < PANELGAMMA_NIGHTS; d++) {
		time_t a = local_at(&today, d, n.from);
		time_t b = local_at(&today, d + (n.until < n.from), n.until);

		if (a == (time_t)-1 || b == (time_t)-1 || b <= a || b <= now) continue;
		out->from[out->n] = (long long)a;
		out->until[out->n] = (long long)b;
		out->n++;
	}
}

/* Whether Auto was dark at `minute` by its hours: src/theme.c's
 * theme_resolve_min, light from `light` up to `dark` and dark the rest, the
 * same two never dark, and a minute that could not be read light */
static bool auto_dark_at(int light, int dark, int minute)
{
	if (minute < 0 || minute >= 24 * 60 || light == dark) return false;
	if (light < dark) return !(minute >= light && minute < dark);
	return !(minute >= light || minute < dark);
}

night_migration night_migrate(const char *appearance, bool have_light, int auto_light,
                              bool have_dark, int auto_dark, bool have_night_hours,
                              time_t now)
{
	night_migration m = { 0 };
	const bool is_auto = appearance && !strcasecmp(appearance, "auto");
	const int light = have_light ? minute_clamp(auto_light, THEME_HOURS_STEP, THEME_LIGHT_FROM)
	                             : THEME_LIGHT_FROM;
	const int dark = have_dark ? minute_clamp(auto_dark, THEME_HOURS_STEP, THEME_DARK_FROM)
	                           : THEME_DARK_FROM;

	if (is_auto) {
		struct tm tm;
		int minute = localtime_r(&now, &tm) ? tm.tm_hour * 60 + tm.tm_min : -1;

		m.appearance = auto_dark_at(light, dark, minute) ? "dark" : "light";
	}
	if (!have_night_hours && (is_auto || have_light || have_dark)) {
		m.hours = true;
		m.from = dark;
		m.until = light;
	}
	m.drop_auto = have_light || have_dark;
	return m;
}
