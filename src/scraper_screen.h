/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_SCRAPER_SCREEN_H
#define ZYPEROS_SCRAPER_SCREEN_H

/* MENU > Scraper (DESIGN.md 11.19): one page - its plan, its run and what
 * it found, its questions and settings, a test of it on a host and its
 * --scraper-page shot - and the ScreenScraper and Video Snaps pages it
 * opens. The screens are src/scraper_screen.c; the run is src/artscrape.c. */
#include "app.h"
#include "panel.h"

/* What main.c uses of it; each is described where it is defined */
extern int shot_scraper_page, shot_scraper_page_t;
extern int g_scp_names;
void scraper_page_shot(app *a);
menu_result ss_page(app *a);
menu_result snaps_screen(app *a);

/* --rows-report: Video Snaps' rows (menu_rows_print) */
void snaps_rows_report(app *a);

#endif
