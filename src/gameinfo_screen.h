/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_GAMEINFO_SCREEN_H
#define ZYPEROS_GAMEINFO_SCREEN_H

/* A game's details page (DESIGN.md 11.31): what the card and the scrape
 * know of it, its rows, and the rows that open the game's other pages
 * (Cover and Video, Buttons, Rename, Delete Game). The screen is
 * src/gameinfo_screen.c; game_info and gi_rows are src/game_menu.h's. */
#include "app.h"
#include "panel.h"

/* What main.c uses of it; each is described where it is defined */
extern bool shot_info_art;
void info_preview(app *a, bool net, const char *on_row);
void info_buttons_preview(app *a, bool pick);

#endif
