/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_HKSTORE_H
#define ZYPEROS_HKSTORE_H

#include <stdbool.h>

#include "hotkeys.h"

/* What is kept on the card for the hotkeys (src/hotkeys.h), in the library
 * database (db_lib), and nothing else: no SDL. Three scopes, as Applies To
 * says them in a game's Controls tab (DESIGN.md 11.28):
 *
 *     hotkeys                 Every Game (MENU > System > Hotkeys)
 *     hotkeys.<TAG>           the system's
 *     hotkeys.<TAG>.<file>    This Game, the whole table copied at its
 *                             first change, as a game's buttons are
 *
 * A game plays with its own, else its system's, else Every Game's, else
 * ZyperOS's own. A key never holds a table equal to what its scope falls back
 * to, so undoing a change undoes the key. */
typedef enum { HKS_EVERY, HKS_SYSTEM, HKS_GAME } hks_scope;

/* The table in force for a scope: for a game the game's, the system's, Every
 * Game's or ZyperOS's, the first there is; for a system its own or Every
 * Game's; for Every Game its own or ZyperOS's. True when the scope has a key
 * of its own. `tag` and `file` as the scope needs them (NULL past it). */
bool hks_get(hks_scope scope, const char *tag, const char *file, hk_table *out);

/* What scope falls back to, the table a key equal to is deleted */
void hks_fallback(hks_scope scope, const char *tag, const char *file, hk_table *out);

/* Keeps t for the scope; deletes the key when t equals its fallback */
void hks_put(hks_scope scope, const char *tag, const char *file, const hk_table *t);

/* Deletes the scope's key: Reset Hotkeys */
void hks_reset(hks_scope scope, const char *tag, const char *file);

/* The keys of a game's scope, for Rename and Delete Game (src/gamern.c) */
void hks_game_key(const char *tag, const char *file, char *out, int n);

#endif
