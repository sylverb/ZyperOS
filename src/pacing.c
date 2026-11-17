/* SPDX-License-Identifier: GPL-3.0-only */
/* Timing, a system menu row. See src/pacing.h. */
#include "pacing.h"

#include <stdio.h>
#include <string.h>

static const char *const IDS[PACING_CHOICES] = {
	[PACING_SMOOTH] = "smooth",
	[PACING_EXACT]  = "exact",
};

const char *pacing_id(pacing_choice c)
{
	return c >= 0 && c < PACING_CHOICES ? IDS[c] : IDS[PACING_EXACT];
}

pacing_choice pacing_parse(const char *word, pacing_choice fallback)
{
	int c;

	if (fallback < 0 || fallback >= PACING_CHOICES) fallback = PACING_EXACT;
	if (!word) return fallback;
	for (c = 0; c < PACING_CHOICES; c++)
		if (!strcmp(word, IDS[c])) return (pacing_choice)c;
	return fallback;
}

pacing_choice pacing_step(pacing_choice c, int d)
{
	int n = (int)PACING_CHOICES;

	if (c < 0 || c >= PACING_CHOICES) c = PACING_EXACT;
	return (pacing_choice)((((int)c + (d < 0 ? -1 : 1)) % n + n) % n);
}

str_id pacing_label_id(pacing_choice c)
{
	return c == PACING_SMOOTH ? S_SYSMENU_PACE_SMOOTH : S_SYSMENU_PACE_EXACT;
}

void pacing_key(const char *tag, char *out, size_t n)
{
	snprintf(out, n, "%s%s", PACING_SETTING_PREFIX, tag ? tag : "");
}
