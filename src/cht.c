/* SPDX-License-Identifier: GPL-3.0-only */
/* See cht.h. */
#include "cht.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	const char *desc, *code;
	int handler;
} slot;

static bool space(char c) { return c == ' ' || c == '\t' || c == '\r'; }

/* The value after `=`, in place: quoted to its closing quote, or the rest of
 * the line without the spaces around it. RetroArch writes every value
 * quoted; a player's file by hand may not, and a description of two words
 * unquoted is still both words. */
static char *value(char *v)
{
	char *end;

	while (space(*v)) v++;
	if (*v == '"') {
		v++;
		end = strchr(v, '"');
		if (end) *end = '\0';
		return v;
	}
	end = v + strlen(v);
	while (end > v && space(end[-1])) *--end = '\0';
	return v;
}

/* `s` cut to at most `max` bytes, at the start of a character */
static void cut_utf8(char *s, size_t max)
{
	size_t n = strlen(s);

	if (n <= max) return;
	n = max;
	while (n > 0 && ((unsigned char)s[n] & 0xc0) == 0x80) n--;
	s[n] = '\0';
}

int cht_parse(const char *text, size_t len, cht_fn fn, void *ctx)
{
	char *buf, *line, *next;
	slot *at;
	long count = -1;
	int last = -1, given = 0, i;
	bool any = false;

	if (!text) return -1;
	buf = malloc(len + 1);
	at = calloc(CHT_MAX, sizeof *at);
	if (!buf || !at) {
		free(buf);
		free(at);
		return -1;
	}
	memcpy(buf, text, len);
	buf[len] = '\0';
	/* A file saved with a byte-order mark: the first key is still a key */
	line = !strncmp(buf, "\xef\xbb\xbf", 3) ? buf + 3 : buf;

	for (; line; line = next) {
		char *eq, *k, *kend, *v;

		next = strchr(line, '\n');
		if (next) *next++ = '\0';
		while (space(*line)) line++;
		if (!*line || *line == '#') continue;
		eq = strchr(line, '=');
		if (!eq) continue;
		*eq = '\0';
		k = line;
		kend = eq;
		while (kend > k && space(kend[-1])) *--kend = '\0';
		v = value(eq + 1);

		if (!strcmp(k, "cheats")) {
			count = strtol(v, NULL, 10);
			any = true;
		} else if (!strncmp(k, "cheat", 5) && k[5] >= '0' && k[5] <= '9') {
			char *field;
			unsigned long n = strtoul(k + 5, &field, 10);

			if (*field != '_' || n >= CHT_MAX) continue;
			field++;
			any = true;
			if ((int)n > last) last = (int)n;
			if (!strcmp(field, "desc"))         at[n].desc = v;
			else if (!strcmp(field, "code"))    at[n].code = v;
			else if (!strcmp(field, "handler")) at[n].handler = (int)strtol(v, NULL, 10);
		}
	}

	/* `cheats =` says how many; a file without it, or one that says more
	 * than it has, counts as far as its last cheat */
	if (count < 0 || count > last + 1) count = last + 1;
	for (i = 0; i < count && i < CHT_MAX; i++) {
		/* One byte over the most, so a cut is seen and made at a
		 * character's start */
		char desc[CHT_DESC_MAX + 2];

		/* A cheat RetroArch pokes in itself, or one with no code: nothing
		 * for a core */
		if (at[i].handler != 0 || !at[i].code || !*at[i].code) continue;
		snprintf(desc, sizeof desc, "%s", at[i].desc ? at[i].desc : "");
		cut_utf8(desc, CHT_DESC_MAX);
		if (fn) fn(desc, at[i].code, ctx);
		given++;
	}
	free(at);
	free(buf);
	return any ? given : -1;
}
