/* SPDX-License-Identifier: GPL-3.0-only */
#include "gamefile.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "biosfile.h"
#include "i18n.h"

/* The kinds with a least size, and why (gamefile.h) */
static const struct { const char *ext; unsigned least; } LEAST[] = {
	{ "zip", 22 },    /* the end of central directory record */
	{ "7z", 32 },     /* the signature header */
	{ "chd", 76 },    /* the shortest CHD header, version 1 */
	{ "cso", 24 },    /* the CISO header */
	{ "pbp", 40 },    /* the PBP header and its eight offsets */
	{ "iso", 2048 },  /* one sector of a disc */
};

unsigned gamefile_least(const char *name)
{
	const char *dot = name ? strrchr(name, '.') : NULL;
	size_t i;

	if (!dot || strchr(dot, '/')) return 1;
	for (i = 0; i < sizeof LEAST / sizeof LEAST[0]; i++)
		if (!strcasecmp(dot + 1, LEAST[i].ext)) return LEAST[i].least;
	return 1;
}

gamefile_state gamefile_check(const char *path, unsigned long long *have)
{
	struct stat st;

	*have = 0;
	if (!path || stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return GAMEFILE_OK;
	*have = (unsigned long long)st.st_size;
	if (*have == 0) return GAMEFILE_EMPTY;
	if (*have < gamefile_least(path)) return GAMEFILE_SHORT;
	return GAMEFILE_OK;
}

void gamefile_message(gamefile_state st, unsigned long long have, char *out, size_t n)
{
	char size[48];

	out[0] = '\0';
	if (st == GAMEFILE_EMPTY) {
		snprintf(out, n, "%s", T(S_LAUNCH_GAME_EMPTY));
	} else if (st == GAMEFILE_SHORT) {
		bios_size_text(have, size, sizeof size);
		snprintf(out, n, T(S_LAUNCH_GAME_SHORT), size);
	}
}
