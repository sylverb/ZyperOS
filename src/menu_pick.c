/* SPDX-License-Identifier: GPL-3.0-only */
/* An option row's list (src/menu_pick.h): its words as the list shows them,
 * and the rows report the checks read. The list itself, run by the menu
 * runner, is menu_pick in src/menu_panel.c, beside the panel it draws. */
#include "menu_pick.h"

#include "menu.h"

#include <stdio.h>
#include <string.h>

int menu_pick_word(const char *label, char *out, int n)
{
	/* The row's value word for word: a value begins with a capital where it
	 * is written (res/strings/GLOSSARY.md, held by tools/i18n-shots.py and
	 * tools/menu-rows-check.py), and a name keeps its own case, a core's
	 * "pcsx_rearmed" or a song's, in the list as on the row */
	return snprintf(out, (size_t)n, "%s", label ? label : "");
}

void menu_rows_print(const char *page, const menu_row *rows, int n, const unsigned char *sound,
                     const menu_choice *choices)
{
	char w[64];
	int i, k;

	for (i = 0; i < n; i++) {
		const menu_choice *c = menu_choice_at(choices, n, i);

		if (!rows[i].label || ROW_IS_NOTE(rows[i])) continue;
		w[0] = '\0';
		if (c && c->labels && c->cur < c->n) menu_pick_word(c->labels[c->cur], w, (int)sizeof w);
		printf("rows: %s\t%s\t%s\t%u\t%u\t%d\t%u\t%d\t%d\t%s\t", page, rows[i].label,
		       rows[i].value == MENU_TICK ? "(tick)" : rows[i].value ? rows[i].value : "",
		       (unsigned)rows[i].live, sound ? (unsigned)sound[i] : 0u, c ? c->n : 0,
		       c ? (unsigned)c->cur : 0u, c ? c->none : 0, c ? c->own : 0, w);
		/* Every choice of its table, each a value the row can say, apart
		 * by the unit separator (0x1f) */
		for (k = 0; c && c->labels && k < c->n; k++)
			printf("%s%s", k ? "\x1f" : "", c->labels[k] ? c->labels[k] : "");
		printf("\n");
	}
}
