/* SPDX-License-Identifier: GPL-3.0-only */
/* MENU > Display > Home Systems (1.0.6): which shelves the home shows.
 *
 * Asked for on the Discord (Garisaan: "can we have the option to hide/reveal
 * ROM sections on the main menu? I'd like to hide the Ports menu"), and my
 * picks: a page in MENU with a switch for every system that has games on the
 * card (a system with none is not on the home anyway, so it is not on the
 * page either: no PSP row on a card with no PSP games), and for Favorites and
 * Recents, a new shelf of the ten games played last (src/shelfbuild.c). All
 * of them On as they ship.
 *
 * A system turned off leaves the home, and with it Random Game, Search,
 * Favorites and Recents; its games stay on the card and in File Transfer,
 * and come back the moment it is turned on again. Nothing is deleted and
 * nothing is moved.
 *
 * The setting is one word in the device database, HOMESYS_KEY: the tags that
 * are off, with commas ("PRT,RECENT"), so a card with none off has no row at
 * all and a new system is On without anyone saying so. Favorites, Recents
 * and Collections are in it by their shelves' tags, FAV, RECENT and COLL.
 *
 * Plain C, no SDL and no database: the launcher reads and writes the word
 * (src/homesys_screen.c), and tools/homesys-check.c holds this. */
#ifndef ZYPEROS_HOMESYS_H
#define ZYPEROS_HOMESYS_H

#include <stdbool.h>
#include <stddef.h>

#define HOMESYS_KEY     "home.hidden"
#define HOMESYS_FAV     "FAV"
#define HOMESYS_RECENT  "RECENT"
/* Collections (src/collect.h, 1.0.7): the player's own lists, after Recents */
#define HOMESYS_COLL    "COLL"
/* What Recents holds: the games played last, newest first */
#define HOMESYS_RECENTS 10
/* Tags the word can hold, and the word's longest */
#define HOMESYS_MAX     96
#define HOMESYS_WORD    (HOMESYS_MAX * 9)

/* The word as the database keeps it, taken as the setting; NULL or "" is
 * every shelf on. Tags past HOMESYS_MAX are dropped. */
void homesys_parse(const char *word);

/* The setting as its word, into out (n bytes; HOMESYS_WORD is enough), in the
 * order the tags were turned off */
void homesys_word(char *out, size_t n);

/* Whether the shelf with `tag` is on the home: true unless it was turned off */
bool homesys_shown(const char *tag);

/* Turn it on or off; true when that changed the setting */
bool homesys_set(const char *tag, bool shown);

/* How many tags are off */
int homesys_hidden_count(void);

#endif
