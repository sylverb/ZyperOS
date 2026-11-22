/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_DL_SCREEN_H
#define ZYPEROS_DL_SCREEN_H
/* MENU > Downloads (DESIGN.md 11.53): one row and one page for every
 * download of game data, in place of MENU's Scraper and Achievements rows
 * (my pick in the downloads lab, 2026-11-19: one door, a list with Download
 * on top). Its rows: Download, which gets every kind that is in it, side by
 * side (src/dlq.h); the kinds - Covers, Videos, Names and Details,
 * Achievements - each with what it lacks, Left and Right taking it in or out
 * of Download and A opening its own page (the Scraper's, the achievements
 * download's); and Settings: the three accounts, Video Snaps and Names and
 * Details. */
#include "app.h"
#include "dlq.h"
#include "panel.h"

menu_result downloads_screen(app *a);
/* The same page for one console (its menu's Downloads row): `only` its folder */
void downloads_page(app *a, const char *only);
/* MENU's row (and a console's, `only` its folder): how far a run behind the
 * screens has got, else what the last count left; NULL before any count */
const char *downloads_menu_value(const char *only, char *out, size_t n);
/* The shelf's news when a run ended behind the screens, in two lines: false
 * when there is none */
bool downloads_news(char *l1, size_t n1, char *l2, size_t n2);

/* A kind out of Download (dl.off.<kind> in the device's database; absent is
 * in): what the page's Download leaves out */
bool downloads_kind_in(const char *kind);
/* --rows-report: the page's rows (menu_rows_print) */
void downloads_rows_report(app *a);
/* What a lane's step brought, kept for the page while it is closed (the
 * queue's step hook, dlq_on_step, installed once at start): the stream and
 * done's wall come back as they were when the page opens again */
void downloads_stepped(dlq_lane l);
/* The renderer is about to go (a game in the fallback, launch_screen.c):
 * the kept pictures go now, and come back from their files when the page
 * next opens */
void downloads_video_gone(void);
/* ZYPEROS_DOWNLOADS_TEST: the page played by a script, and the steps
 * behind it once it closes (dl_screen.c, dl_test) */
void downloads_test(app *a);
/* --downloads STATE: the page drawn on stand-in counts (downloads_shot) */
extern int shot_downloads;
void downloads_shot(app *a);
#endif
