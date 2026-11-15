/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_HOMESYS_SCREEN_H
#define ZYPEROS_HOMESYS_SCREEN_H

/* MENU > Display > Home Systems (src/homesys.h): its page of switches, the
 * setting read at start, and the MENU row's value. */
#include "app.h"

#include <stddef.h>

void homesys_screen(app *a);
/* The setting from the device database, once, before the first scan */
void homesys_load(void);
/* The row's value, "All" or "12 of 15", into out when it is the second */
const char *homesys_value(char *out, size_t n);
/* --homesys-page ROW: the page over the shelf, as menu_run draws it, the
 * cursor on ROW (a host shot) */
void homesys_shot_draw(app *a, int row);

#endif
