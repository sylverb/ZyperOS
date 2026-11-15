/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_DEVMODE_SCREEN_H
#define ZYPEROS_DEVMODE_SCREEN_H

/* MENU > Developer Mode's page and its SSH on a release image (src/devmode.h,
 * DESIGN.md 11.36): the keys it reads, its rows, and the SSH it keeps up.
 * The code is src/devmode_screen.c. */
#include "app.h"
#include "panel.h"

/* What main.c uses of it; each is described where it is defined */
int dev_rows(menu_row rows[6], char *cmd, size_t cn, char *keys, size_t kn, bool up,
             const char *ip, const char *pw, int nkeys, const char *ssh);

#endif
