/* SPDX-License-Identifier: GPL-3.0-only */
/* The list an option row opens on A (src/menu.h's menu_choice, DESIGN.md
 * 9): its choices under the row's name, a tick on the one that is on and
 * the cursor on it, as MENU > Language is. A chooses one, B leaves it as it
 * was. In the menu it opens from's accent and width, over its backdrop,
 * with its power rule (the in-game menu's paused game and its stop). */
#ifndef ZYPEROS_MENU_PICK_H
#define ZYPEROS_MENU_PICK_H

#include "panel.h"

#define MENU_PICK_POWER (-2)

/* The choice picked, or -1 for B (or the menu closing under it), or
 * MENU_PICK_POWER when the menu it opens from's power rule closed it (the
 * in-game menu's stop), so that menu closes too. `ticked`
 * is the one that is on (-1: none), `at` the one the cursor starts on. */
int menu_pick(app *a, const menu_style *from, void *from_ctx, const char *heading,
              const char *const *labels, int n, int ticked, int at);

/* A choice's words as its list shows them, begun with a capital: "never" is
 * "Never" there. The shots draw a list with it too. */
int menu_pick_word(const char *label, char *out, int n);

/* For tools/menu-rows-check.py (--rows-report): each row of `page` on a
 * line, "rows: page, label, value, live, sound class, choices, cur, none,
 * own, the word at cur", the words as a list shows them */
void menu_rows_print(const char *page, const menu_row *rows, int n, const unsigned char *sound,
                     const menu_choice *choices);

#endif
