/* SPDX-License-Identifier: MPL-2.0 */
/* Cheats: the codes the launcher hands over, and when the core gets them.
 * See cheats.h. */
#include "cheats.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cheevos.h"

/* The codes held, each its own allocation: read at a switch, never in a
 * frame, so a malloc a code costs nothing anyone sees, and the list costs
 * nothing at all for a game with none. */
static char *g_code[DIATOM_CHEATS_MAX];
static int   g_n;

static void forget(void)
{
	int i;

	for (i = 0; i < g_n; i++) free(g_code[i]);
	g_n = 0;
}

int diatom_cheats_count(void) { return g_n; }

int diatom_cheats_read(const char *path)
{
	static char line[DIATOM_CHEATS_CODE_MAX + 2];
	char *got[DIATOM_CHEATS_MAX];
	int n = 0, cut = 0, i;
	FILE *f;

	if (!path || !*path) {
		forget();
		return 0;
	}
	f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "diatom: cheats: cannot read %s\n", path);
		return -1;
	}
	while (fgets(line, sizeof line, f)) {
		size_t len = strcspn(line, "\r\n");
		bool whole = line[len] != '\0' || feof(f);

		/* A line past the bound is a code no core takes whole: dropped,
		 * with the rest of it, rather than handed over cut */
		if (!whole) {
			int ch;

			while ((ch = fgetc(f)) != EOF && ch != '\n')
				;
			cut++;
			continue;
		}
		line[len] = '\0';
		if (!len) continue;
		if (n >= DIATOM_CHEATS_MAX) { cut++; continue; }
		got[n] = strdup(line);
		if (got[n]) n++;
	}
	fclose(f);
	if (cut) fprintf(stderr, "diatom: cheats: %d left out of %s (too many, or too long)\n",
	                 cut, path);

	forget();
	for (i = 0; i < n; i++) g_code[i] = got[i];
	g_n = n;
	return n;
}

void diatom_cheats_apply(diatom_core *c)
{
	int i;

	if (!c || !c->cheat_reset || !c->cheat_set) return;
	if (!g_n && !c->cheats_held) return;

	c->cheat_reset();
	for (i = 0; i < g_n; i++) c->cheat_set((unsigned)i, true, g_code[i]);
	c->cheats_held = g_n > 0;
	/* Once one has been on, the game's memory is not what the set was
	 * written against: achievements stop until the next game, whatever is
	 * turned off after */
	if (g_n) diatom_cheevos_pause(true);
}

int diatom_cheats_begin(diatom_core *c, const char *path)
{
	int n;

	diatom_cheevos_pause(false);
	n = diatom_cheats_read(path ? path : "");
	if (n < 0) diatom_cheats_read("");
	if (g_n) fprintf(stderr, "diatom: cheats: %d on\n", g_n);
	diatom_cheats_apply(c);
	return n;
}
