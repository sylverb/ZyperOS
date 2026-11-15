/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_DEVMODE_SCREEN_H
#define ZYPEROS_DEVMODE_SCREEN_H

/* MENU > Developer Mode's page and its SSH on a release image (src/devmode.h,
 * DESIGN.md 11.36): the keys it reads, its rows, and the SSH it keeps up.
 * The code is src/devmode_screen.c. */
#include "app.h"
#include "linebreak.h"
#include "panel.h"

/* What main.c uses of it; each is described where it is defined */
/* The Developer page's rows: SSH over Wi-Fi, Developer Mode, a rule, up to
 * four full-width lines, the legend */
#define DEV_ROWS     9
#define DEV_LINES    4
#define DEV_LINE     96
#define DEV_ROW_WIFI 0
#define DEV_ROW_MODE 1
int dev_rows(menu_row rows[DEV_ROWS], char lines[DEV_LINES][DEV_LINE], bool over_wifi, bool wifi,
             bool up, const char *ip, const char *name, const char *pw, int nkeys, const char *ssh);
int dev_wifi_lines(app *a, char lines[4][LB_LINE], menu_row rows[5]);

#endif
