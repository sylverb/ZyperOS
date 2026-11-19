/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_COLLECT_SCREEN_H
#define ZYPEROS_COLLECT_SCREEN_H

/* Collections' screens (DESIGN.md 11.53; src/collect.h keeps the lists):
 * Add to Collection on a game's details page, and the Collections shelf's
 * Rename, Remove from Collection and Delete Collection. */
#include "app.h"
#include "library.h"

#include <stdbool.h>
#include <stddef.h>

/* Add to Collection's value for the game: "In 2", or "" when it is in none */
void collect_value(const char *folder, const char *file, char *out, size_t n);

/* Add to Collection's list for game `g` of system `owner`: a row for each
 * collection, ticked when the game is in it, A putting it in or taking it
 * out, written through; then New Collection, the keyboard for a name, the
 * game its first. True when a collection changed, for the shelf to be made
 * again once the details page closes (refresh_collections_shelf). */
bool collect_page(app *a, int owner, const game_entry *g, unsigned accent);

/* On the Collections shelf, from its menu, for the collection under the
 * cursor (its top) or shown (in one); the shelf is made again after a change,
 * on the collection or game where it is now. Rename: the keyboard on its
 * name; true when renamed. Remove from Collection: the game under the cursor
 * out of the collection shown. Delete Collection: the question, its cursor
 * on Cancel, then the list; its games stay; true when deleted. */
bool collect_rename_screen(app *a);
void collect_remove_game(app *a);
bool collect_delete_screen(app *a);

/* One frame over the shelf, for a shot: Add to Collection's list for the
 * game under the cursor, the cursor on row `sel` (--collect-page); Delete
 * Collection's question for the collection the shelf is on, the cursor on
 * Cancel (--collect-delete) */
void collect_page_preview(app *a, int sel);
void collect_delete_preview(app *a);

#endif
