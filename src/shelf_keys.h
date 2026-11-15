/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_SHELF_KEYS_H
#define ZYPEROS_SHELF_KEYS_H

/* The shelves' keys: into a system and out, a card's steps and its letter
 * jump, Favorites' Y, and the games shelf's and the systems row's presses.
 * The code is src/shelf_keys.c; the drawing is src/shelf_draw.c. */
#include "app.h"
#include "panel.h"

/* What main.c uses of it; each is described where it is defined */
void fav_toggle_heard(const char *tag, const char *file);
void update_systems(app *a);
void update_games(app *a);

#endif
