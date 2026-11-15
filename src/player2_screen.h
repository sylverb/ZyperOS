/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_PLAYER2_SCREEN_H
#define ZYPEROS_PLAYER2_SCREEN_H

/* NES Player 2's page (DESIGN.md 11.38, src/p2.h): the page over a paused
 * game or over the shelf, its rows and its view, and p2_ops, what Player 2
 * is done with on the device. The code is src/player2_screen.c. */
#include "app.h"
#include "panel.h"

/* What the page shows, gathered: from src/p2.c and the hotspot, or made up
 * by a shot (--player2). `link` is the one it is on, or while off the one A
 * would start. */
typedef struct {
	p2_state    st;
	p2_link     link;
	bool        starting;   /* the hotspot coming up, or about to (A's wait) */
	bool        closing;    /* the stop's wait */
	unsigned    guests;
	int         players;
	const char *ssid, *pass, *qr, *url;
} p2_view;

/* What main.c uses of it; each is described where it is defined */
extern p2_view g_shot_p2_view;
void p2_panel(app *a, const char *heading, const menu_row *rows, int n,
              unsigned accent, mm_pose pose);
void p2_show(app *a, const p2_view *v);
void p2_setup(void);

#endif
