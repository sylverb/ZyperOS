/* SPDX-License-Identifier: GPL-3.0-only */
/* The boot animation's theme from zytheme, the Appearance setting's word that
 * ZyperOS keeps in theme.txt on the boot partition (its src/boottheme.h) and
 * boot.scr puts on the kernel's line: Beard or Salt & Pepper, as the launcher
 * would pick it at that minute (its src/theme.c, theme_resolve_min). Pure, so
 * ZyperOS's check-autotheme holds it against the launcher's own rule. */
#ifndef SPLASH_ZYTHEME_H
#define SPLASH_ZYTHEME_H

#include <string.h>

/* A minute of the day from `p`: digits only, under 24 * 60, ended by `end`
 * (the next character must be it); -1 for anything else. *next is past it. */
static inline int splash_zytheme_minute(const char *p, char end, const char **next)
{
	int v = 0, digits = 0;

	for (; *p >= '0' && *p <= '9'; p++, digits++) {
		v = v * 10 + (*p - '0');
		if (v >= 24 * 60) return -1;
	}
	if (!digits || digits > 4 || *p != end) return -1;
	*next = p;
	return v;
}

/* 1 for Beard (light), 0 for Salt & Pepper (dark), for `word` at `minute`
 * into the local day (-1: no clock to read):
 *
 *   dark           Salt & Pepper
 *   auto-F-U       ZyperOS 1.1.0's Auto on the Night Hours: Salt & Pepper
 *                  from minute F up to minute U, round midnight when U is
 *                  earlier (auto-1260-420: 21:00 to 7:00), and Beard the rest
 *                  of the day; F and U the same, never dark
 *   auto           a 1.0.5 launcher's Auto: Beard from 7:00 up to 19:00
 *   anything else  Beard, the launcher's default and a new card's
 *
 * A clock that could not be read is not night: Beard. */
static inline int splash_zytheme_light(const char *word, int minute)
{
	const char *p;
	int from, until;

	if (!word) return 1;
	if (!strcmp(word, "dark")) return 0;
	if (strncmp(word, "auto", 4)) return 1;
	if (!word[4]) {
		from = 19 * 60;
		until = 7 * 60;
	} else {
		if (word[4] != '-') return 1;
		p = word + 5;
		if ((from = splash_zytheme_minute(p, '-', &p)) < 0) return 1;
		if ((until = splash_zytheme_minute(p + 1, '\0', &p)) < 0) return 1;
	}
	if (minute < 0 || minute >= 24 * 60 || from == until) return 1;
	if (from < until) return !(minute >= from && minute < until);
	return !(minute >= from || minute < until);
}

#endif
