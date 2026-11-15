/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_TOUR_SCREEN_H
#define ZYPEROS_TOUR_SCREEN_H

/* The first-start tutorial (src/tour.h has its steps, src/tourdraw.c its
 * pictures): its run, its settings and the skip's message. The code is
 * src/tour_screen.c. */
#include "app.h"
#include "panel.h"
#include "tour.h"
#include "tourdraw.h"

/* What main.c uses of it; each is described where it is defined */
void tour_zyper_draw(void *ctx, float cx, float cy, float h, const tour_zyper *z);
void tour_poweroff_draw(void *ctx, unsigned t, float cx, float cy, float h,
                        float from_x, Uint8 alpha);
void tour_settings(const app *a, int at[TOUR_CHOICES]);
/* The tutorial, from its start (`first`, the first start's) or MENU */
void tour_run(app *a, bool first);

#endif
