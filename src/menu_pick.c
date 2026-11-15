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
	int k = snprintf(out, (size_t)n, "%s", label ? label : "");

	/* A choice in a list begins with a capital, as any list's rows do: a
	 * row's value may be "never" or "stereo" beside its name, and in a list
	 * of its own it is a name */
	if (out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 'a' + 'A');
	return k;
}

void menu_rows_print(const char *page, const menu_row *rows, int n, const unsigned char *sound,
                     const menu_choice *choices)
{
	char w[64];
	int i;

	for (i = 0; i < n; i++) {
		const menu_choice *c = menu_choice_at(choices, n, i);

		if (!rows[i].label || ROW_IS_NOTE(rows[i])) continue;
		w[0] = '\0';
		if (c && c->labels && c->cur < c->n) menu_pick_word(c->labels[c->cur], w, (int)sizeof w);
		printf("rows: %s\t%s\t%s\t%u\t%u\t%d\t%u\t%d\t%d\t%s\n", page, rows[i].label,
		       rows[i].value == MENU_TICK ? "(tick)" : rows[i].value ? rows[i].value : "",
		       (unsigned)rows[i].live, sound ? (unsigned)sound[i] : 0u, c ? c->n : 0,
		       c ? (unsigned)c->cur : 0u, c ? c->none : 0, c ? c->own : 0, w);
	}
}
