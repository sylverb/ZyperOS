/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_SIGNIN_SCREEN_H
#define ZYPEROS_SIGNIN_SCREEN_H

/* MENU's sign-ins: RetroAchievements', ScreenScraper's, and SteamGridDB's key
 * (its page, its wait and its check). The screens are src/signin_screen.c. */
#include "app.h"
#include "panel.h"

/* What main.c uses of it; each is described where it is defined */
menu_result ss_signin_screen(app *a);
const char *sgdb_status(void);
menu_result sgdb_screen(app *a);

#endif
