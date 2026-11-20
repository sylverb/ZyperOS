/* SPDX-License-Identifier: GPL-3.0-only */
/* The boot screen's theme words: see boottheme.h. */
#include "boottheme.h"

#include <stdio.h>
#include <string.h>

/* A zone name the kernel's line and U-Boot's env import carry as one word:
 * the zone database's own letters, digits and _ + - / */
static int zone_ok(const char *z)
{
	if (!z || !*z || strlen(z) > 48) return 0;
	for (; *z; z++)
		if (!((*z >= 'a' && *z <= 'z') || (*z >= 'A' && *z <= 'Z') || (*z >= '0' && *z <= '9') ||
		      *z == '_' || *z == '+' || *z == '-' || *z == '/'))
			return 0;
	return 1;
}

/* A language's code as a word the line carries: letters and a hyphen, as
 * src/i18n.h's codes are; "en" for anything else */
static int lang_ok(const char *l)
{
	if (!l || !*l || strlen(l) > 15) return 0;
	for (; *l; l++)
		if (!((*l >= 'a' && *l <= 'z') || (*l >= 'A' && *l <= 'Z') || *l == '-')) return 0;
	return 1;
}

/* A minute of the day, or `fallback` */
static int day_minute(int m, int fallback)
{
	return m >= 0 && m < 24 * 60 ? m : fallback;
}

void boottheme_words(const char *appearance, int night_from, int night_until, const char *zone,
                     const char *lang, int bright, char *line, size_t n, char *file, size_t fn)
{
	const char *z = zone_ok(zone) ? zone : "-";
	const char *l = lang_ok(lang) ? lang : "en";
	char b[16] = "-", t[24] = "light";

	if (appearance && !strcmp(appearance, "dark"))
		snprintf(t, sizeof t, "dark");
	else if (appearance && !strcmp(appearance, "auto"))
		snprintf(t, sizeof t, "auto-%d-%d", day_minute(night_from, 21 * 60),
		         day_minute(night_until, 7 * 60));
	if (bright >= 1) snprintf(b, sizeof b, "%d", bright);
	if (line && n) snprintf(line, n, "zytheme=%s zyzone=%s zylang=%s zybright=%s", t, z, l, b);
	if (file && fn) snprintf(file, fn, "zytheme=%s\nzyzone=%s\nzylang=%s\nzybright=%s\n", t, z, l, b);
}

/* The value of word `key` on the line, up to the next space, into out */
static int word_of(const char *cmdline, const char *key, char *out, size_t n)
{
	size_t kl = strlen(key);
	const char *p = cmdline;

	while (p && (p = strstr(p, key))) {
		if ((p == cmdline || p[-1] == ' ') && p[kl] == '=') {
			size_t len = strcspn(p + kl + 1, " \t\r\n");
			if (len >= n) len = n - 1;
			memcpy(out, p + kl + 1, len);
			out[len] = 0;
			return 1;
		}
		p += kl;
	}
	return 0;
}

void boottheme_from_cmdline(const char *cmdline, char *line, size_t n)
{
	char t[24] = "", z[64] = "", l[16] = "", b[16] = "";
	int has_t = word_of(cmdline ? cmdline : "", "zytheme", t, sizeof t);
	int has_z = word_of(cmdline ? cmdline : "", "zyzone", z, sizeof z);
	int has_l = word_of(cmdline ? cmdline : "", "zylang", l, sizeof l);
	int has_b = word_of(cmdline ? cmdline : "", "zybright", b, sizeof b);
	int k;

	if (!n) return;
	if (!has_t && !has_z && !has_l) { line[0] = 0; return; }
	k = snprintf(line, n, "zytheme=%s zyzone=%s", t, z);
	if (has_l && k >= 0 && (size_t)k < n) k += snprintf(line + k, n - (size_t)k, " zylang=%s", l);
	if (has_b && k >= 0 && (size_t)k < n) snprintf(line + k, n - (size_t)k, " zybright=%s", b);
}

/* `want` without its word `key` and what follows it, where `said` has none */
static size_t cut_where_absent(const char *want, size_t len, const char *said, const char *key)
{
	const char *w = strstr(want, key);

	return w && (size_t)(w - want) < len && !strstr(said, key) ? (size_t)(w - want) : len;
}

int boottheme_same(const char *want, const char *said)
{
	size_t len = strlen(want);

	/* A boot.scr with no zybright: the first three words; with no zylang
	 * either, the first two */
	len = cut_where_absent(want, len, said, " zybright=");
	len = cut_where_absent(want, len, said, " zylang=");
	return strlen(said) == len && !strncmp(want, said, len);
}
