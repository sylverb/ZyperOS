/* SPDX-License-Identifier: GPL-3.0-only */
/* When Slow, the Nintendo DS shelf's row. See src/dsslow.h. */
#include "dsslow.h"

#include <string.h>

static const char *const IDS[DSS_CHOICES] = {
	[DSS_KEEP]   = "keep",
	[DSS_STEADY] = "steady",
};

const char *dsslow_id(dss_choice c)
{
	return c >= 0 && c < DSS_CHOICES ? IDS[c] : IDS[DSS_KEEP];
}

dss_choice dsslow_parse(const char *word)
{
	int c;

	if (!word) return DSS_KEEP;
	for (c = 0; c < DSS_CHOICES; c++)
		if (!strcmp(word, IDS[c])) return (dss_choice)c;
	return DSS_KEEP;
}

dss_choice dsslow_step(dss_choice c, int d)
{
	int n = (int)DSS_CHOICES;

	if (c < 0 || c >= DSS_CHOICES) c = DSS_KEEP;
	return (dss_choice)((((int)c + (d < 0 ? -1 : 1)) % n + n) % n);
}

const char *dsslow_frameskip(dss_choice c)
{
	return c == DSS_STEADY ? "steady" : "late";
}

str_id dsslow_label_id(dss_choice c)
{
	return c == DSS_STEADY ? S_SYSMENU_DSSLOW_STEADY : S_SYSMENU_DSSLOW_KEEP;
}
