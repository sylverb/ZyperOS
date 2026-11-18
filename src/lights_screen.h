/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_LIGHTS_SCREEN_H
#define ZYPEROS_LIGHTS_SCREEN_H

/* MENU > Side Lights (src/sidelights.h) and MENU > Language (src/i18n.h):
 * two pages of one list each, and the language put in place. The screens
 * are src/lights_screen.c. */
#include "app.h"
#include "panel.h"

/* What main.c uses of it; each is described where it is defined */
int language_build(void *ctx, menu_row *rows, int max, const char **heading);
void lights_shot_draw(app *a, int row);

#endif
