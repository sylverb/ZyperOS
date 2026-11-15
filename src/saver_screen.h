/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_SAVER_SCREEN_H
#define ZYPEROS_SAVER_SCREEN_H

/* The screensaver (src/saver.h has its rules; DESIGN.md 11.37): a mode of
 * the shelf's loop, drawn where the shelf would be, with its pool of covers
 * and video snaps. The code is src/saver_screen.c. */
#include "app.h"
#include "panel.h"

/* What main.c uses of it; each is described where it is defined */
void saver_stop(app *a, const char *why);
void saver_shot(app *a);
bool saver_pass(app *a);

#endif
