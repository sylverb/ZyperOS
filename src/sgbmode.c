/* SPDX-License-Identifier: GPL-3.0-only */
/* Super Game Boy, a system menu row. See src/sgbmode.h. */
#include "sgbmode.h"

#include <stdio.h>
#include <string.h>

static const char *const IDS[SGBM_CHOICES] = {
	[SGBM_OFF]   = "off",
	[SGBM_GAMES] = "games",
};

const char *sgbm_id(sgbm_choice c)
{
	return c >= 0 && c < SGBM_CHOICES ? IDS[c] : IDS[SGBM_OFF];
}

sgbm_choice sgbm_parse(const char *word)
{
	int c;

	if (!word) return SGBM_OFF;
	for (c = 0; c < SGBM_CHOICES; c++)
		if (!strcmp(word, IDS[c])) return (sgbm_choice)c;
	return SGBM_OFF;
}

sgbm_choice sgbm_step(sgbm_choice c, int d)
{
	int n = (int)SGBM_CHOICES;

	if (c < 0 || c >= SGBM_CHOICES) c = SGBM_OFF;
	return (sgbm_choice)((((int)c + (d < 0 ? -1 : 1)) % n + n) % n);
}

str_id sgbm_label_id(sgbm_choice c)
{
	return c == SGBM_GAMES ? S_SYSMENU_SGB_GAMES : S_MENU_OFF_TITLE;
}

str_id sgbm_note_id(sgbm_choice c)
{
	return c == SGBM_GAMES ? S_SYSMENU_SGB_GAMES_NOTE : S_SYSMENU_SGB_OFF_NOTE;
}

void sgbm_key(const char *tag, char *out, size_t n)
{
	snprintf(out, n, "%s%s", SGBM_SETTING_PREFIX, tag ? tag : "");
}

bool sgbm_shelf(const char *tag, const char *core)
{
	return tag && core && (!strcmp(tag, "GB") || !strcmp(tag, "GBC")) && !strcmp(core, "mgba");
}
