/* SPDX-License-Identifier: GPL-3.0-only */
/* See homesys.h. */
#include "homesys.h"

#include <stdio.h>
#include <string.h>

#define TAG_LEN 16

static char g_off[HOMESYS_MAX][TAG_LEN];
static int  g_noff;

static int find(const char *tag)
{
	for (int i = 0; tag && i < g_noff; i++)
		if (!strcmp(g_off[i], tag)) return i;
	return -1;
}

void homesys_parse(const char *word)
{
	const char *p = word;

	g_noff = 0;
	while (p && *p) {
		const char *end = strchr(p, ',');
		size_t len = end ? (size_t)(end - p) : strlen(p);
		char tag[TAG_LEN];

		/* Spaces around a tag, as a person editing the word leaves them */
		while (len && (*p == ' ' || *p == '\t')) { p++; len--; }
		while (len && (p[len - 1] == ' ' || p[len - 1] == '\t' || p[len - 1] == '\n')) len--;
		if (len > 0 && len < TAG_LEN && g_noff < HOMESYS_MAX) {
			memcpy(tag, p, len);
			tag[len] = '\0';
			if (find(tag) < 0) snprintf(g_off[g_noff++], TAG_LEN, "%s", tag);
		}
		p = end ? end + 1 : NULL;
	}
}

void homesys_word(char *out, size_t n)
{
	size_t at = 0;

	if (!n) return;
	out[0] = '\0';
	for (int i = 0; i < g_noff; i++) {
		int w = snprintf(out + at, n - at, "%s%s", i ? "," : "", g_off[i]);

		if (w < 0 || (size_t)w >= n - at) {
			out[at] = '\0';             /* never half a tag */
			return;
		}
		at += (size_t)w;
	}
}

bool homesys_shown(const char *tag)
{
	return find(tag) < 0;
}

bool homesys_set(const char *tag, bool shown)
{
	int i = find(tag);

	if (!tag || !tag[0] || strlen(tag) >= TAG_LEN) return false;
	if (shown) {
		if (i < 0) return false;
		memmove(&g_off[i], &g_off[i + 1], (size_t)(g_noff - i - 1) * sizeof g_off[0]);
		g_noff--;
		return true;
	}
	if (i >= 0 || g_noff >= HOMESYS_MAX) return false;
	snprintf(g_off[g_noff++], TAG_LEN, "%s", tag);
	return true;
}

int homesys_hidden_count(void)
{
	return g_noff;
}
