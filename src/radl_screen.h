/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_RADL_SCREEN_H
#define ZYPEROS_RADL_SCREEN_H

/* MENU > Achievements: one page that downloads every game's RetroAchievements
 * set ahead of time, so achievements work offline (src/radl.h) - its plan, its
 * run and what it did, the account behind Y. The run is src/radl.c. */
#include "app.h"
#include "panel.h"

/* The MENU row's page */
menu_result radl_screen(app *a);

/* --radl-page STATE: the page drawn on stand-in systems (radl_page_shot) */
extern int shot_radl_page;
void radl_page_shot(app *a);

#endif
