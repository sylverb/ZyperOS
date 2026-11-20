/* SPDX-License-Identifier: GPL-3.0-only */
/* A screenshot's file name. See src/shotname.h. */
#include "shotname.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

void shot_game_part(const char *name, char *out, size_t n)
{
	size_t i;

	if (!n) return;
	snprintf(out, n, "%s", name ? name : "");
	for (i = 0; out[i]; i++)
		if (out[i] == '/' || out[i] == '\t' || out[i] == '\\') out[i] = '-';
}

/* Days from 2000-01-01 to a date of the proleptic Gregorian calendar, by
 * the calendar alone (the civil-days rule: years from March, so the leap
 * day is a year's last) */
static long days_from_civil(int y, int m, int d)
{
	long era, yoe, doy, doe;

	y -= m <= 2;
	era = (y >= 0 ? y : y - 399) / 400;
	yoe = y - era * 400;
	doy = (153L * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 730425;          /* 730425: 0000-03-01 to 2000-01-01 */
}

static void civil_from_days(long z, int *y, int *m, int *d)
{
	long era, doe, yoe, doy, mp;

	z += 730425;
	era = (z >= 0 ? z : z - 146096) / 146097;
	doe = z - era * 146097;
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	*d = (int)(doy - (153 * mp + 2) / 5 + 1);
	*m = (int)(mp < 10 ? mp + 3 : mp - 9);
	*y = (int)(yoe + era * 400 + (*m <= 2));
}

static int month_days(int y, int m)
{
	static const int D[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
	return D[m - 1];
}

long shot_secs(int year, int mon, int day, int hour, int min, int sec)
{
	if (year < 2000 || year > 9999 || mon < 1 || mon > 12 || day < 1 || day > month_days(year, mon) ||
	    hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 59)
		return -1;
	return days_from_civil(year, mon, day) * 86400L + hour * 3600L + min * 60L + sec;
}

void shot_parts(long when, int *year, int *mon, int *day, int *hour, int *min, int *sec)
{
	long days = when / 86400, s = when % 86400;

	civil_from_days(days, year, mon, day);
	*hour = (int)(s / 3600);
	*min = (int)(s / 60 % 60);
	*sec = (int)(s % 60);
}

/* `n` digits at p, as a number; -1 when one is not a digit */
static int digits(const char *p, int n)
{
	int v = 0;

	for (int i = 0; i < n; i++) {
		if (!isdigit((unsigned char)p[i])) return -1;
		v = v * 10 + (p[i] - '0');
	}
	return v;
}

bool shot_parse(const char *file, char *game, size_t n, long *when)
{
	size_t len = file ? strlen(file) : 0;
	const char *t;
	long w;

	/* the game's name, a space, "YYYY-MM-DD HH.MM.SS", ".png" */
	if (len <= SHOT_TAIL_LEN || strcasecmp(file + len - 4, ".png")) return false;
	t = file + len - SHOT_TAIL_LEN;
	if (t[0] != ' ' || t[5] != '-' || t[8] != '-' || t[11] != ' ' || t[14] != '.' || t[17] != '.')
		return false;
	w = shot_secs(digits(t + 1, 4), digits(t + 6, 2), digits(t + 9, 2),
	              digits(t + 12, 2), digits(t + 15, 2), digits(t + 18, 2));
	if (w < 0) return false;
	if (game && n) {
		size_t g = (size_t)(t - file);

		if (g >= n) g = n - 1;
		memcpy(game, file, g);
		game[g] = '\0';
	}
	if (when) *when = w;
	return true;
}

bool shot_file_name(const char *game, long when, char *out, size_t n)
{
	int y, mo, d, h, mi, s;
	int k;

	shot_parts(when, &y, &mo, &d, &h, &mi, &s);
	k = snprintf(out, n, "%s %04d-%02d-%02d %02d.%02d.%02d.png", game, y, mo, d, h, mi, s);
	return k >= 0 && (size_t)k < n;
}

bool shot_parse_openbor(const char *file, char *game, size_t n, int *num)
{
	size_t len = file ? strlen(file) : 0;
	const char *t;
	int k;

	/* the module, " - ", four digits, ".png" */
	if (len <= SHOT_OPENBOR_TAIL_LEN || strcasecmp(file + len - 4, ".png")) return false;
	t = file + len - SHOT_OPENBOR_TAIL_LEN;
	if (t[0] != ' ' || t[1] != '-' || t[2] != ' ' || (k = digits(t + 3, 4)) < 0) return false;
	if (game && n) {
		size_t g = (size_t)(t - file);

		if (g >= n) g = n - 1;
		memcpy(game, file, g);
		game[g] = '\0';
	}
	if (num) *num = k;
	return true;
}

long shot_days_before(long then, long now)
{
	return now / 86400 - then / 86400;
}

int shot_cmp(const shot_key *a, const shot_key *b, shot_order order)
{
	int c = 0;

	if (order == SHOT_ORDER_SYSTEM && a->owner != b->owner) return a->owner < b->owner ? -1 : 1;
	if (order != SHOT_ORDER_ADDED) c = strcasecmp(a->title ? a->title : "", b->title ? b->title : "");
	if (c) return c;
	/* the newest first, in every order */
	if (a->when != b->when) return a->when > b->when ? -1 : 1;
	if (order == SHOT_ORDER_ADDED) c = strcasecmp(a->title ? a->title : "", b->title ? b->title : "");
	if (c) return c;
	return a->owner < b->owner ? -1 : a->owner > b->owner;
}
