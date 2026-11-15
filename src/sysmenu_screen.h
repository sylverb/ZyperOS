/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_SYSMENU_SCREEN_H
#define ZYPEROS_SYSMENU_SCREEN_H

/* MENU (DESIGN.md 11.3): its rows built tab by tab, its keys, the top menu
 * and a system's, and MENU > Display's Auto Hours and Theme pages. The
 * screens are src/sysmenu_screen.c; the rows' words are src/sys_menu.c's. */
#include "app.h"
#include "linebreak.h"
#include "panel.h"

/* MENU > Display > Auto Hours: Light From and Dark From, by the half hour,
 * Left and Right round the day, A forward; applied at once, so a theme that
 * turns on the change turns on screen */
typedef struct { app *a; char l[24], d[24]; } hours_view;

/* MENU > Display > Theme: Accent, Light Background, Dark Background,
 * Corners, Zyper and Reset to Default. Left and Right step a row round and
 * apply it at once; A steps forward, and on Reset puts all four back. */
enum { LK_ACCENT, LK_LIGHT, LK_DARK, LK_CORNERS, LK_ZYPER, LK_RESET, LK_ROWS };

/* What main.c uses of it; each is described where it is defined */
int usb_auto_lines(app *a, char lines[4][LB_LINE], menu_row rows[4]);
void cheevos_unchecked_message(app *a);
int menu_build(app *a, screen_id screen, int sys,
               menu_row *out, menu_bufs *b, const char **heading);
void look_bar_colors(void);
void look_load(void);
int hours_build(void *ctx, menu_row *rows, int max, const char **heading);
void look_row_mark(app *a, const menu_row *row, int right, int cy, float on_fill);
void look_header(app *a, const SDL_Rect *q);
int look_build(void *ctx, menu_row *rows, int max, const char **heading);
menu_result sysmenu_key(app *a, void *ctx, in_button key, int sel);
void zyperos_menu(app *a);

#endif
