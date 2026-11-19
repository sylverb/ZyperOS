/* SPDX-License-Identifier: GPL-3.0-only */
/* Collections (DESIGN.md 11.56): lists of games of your own, from every
 * system, kept on the card as Collections/<name>.txt.
 *
 * NextUI's top ten, and my picks in the Library lab (2026-11-19): "Add to
 * Collection" on a game's details, a group on the home after Favorites and
 * Recents once there is one collection, Home Systems can hide it, and Delete
 * Collection asks first.
 *
 * ONE GAME A LINE, ITS PATH FROM THE CARD'S ROOT: "/Roms/GBA/Golden Sun.gba",
 * the plain format MinUI and NextUI write, so a list made by hand, or on
 * another handheld, shows up too. A line is read as written ("/Roms/...",
 * "Roms/...", or "GBA/..." from Roms/ itself, with \ as /) and kept as
 * written: a game that is off the card today keeps its line, and a list is
 * rewritten only when this changes it. What this writes is the first form.
 * Empty lines and lines starting with # are kept and never a game.
 *
 * The collections are in A to Z order of their names, the files' stems, as
 * a shelf's folders are; a collection's games in its file's order, which is
 * the order they were added (Sort By's Added).
 *
 * Plain C and stdio, no SDL and no database: tools/collect-check.c runs it in
 * a folder of its own. */
#ifndef ZYPEROS_COLLECT_H
#define ZYPEROS_COLLECT_H

#include <stdbool.h>
#include <stddef.h>

#define COLL_DIR    "Collections"
/* Collections read; a card with more has the first in A to Z order. With
 * its rule and New Collection, Add to Collection's list fits a menu
 * (MENU_MAX_ROWS). */
#define COLL_MAX    36
/* A name's bytes, its terminator in: the file's stem */
#define COLL_NAME   96
/* Lines a collection holds, games or not */
#define COLL_LINES  2000
/* A line's bytes: "/Roms/" and a system's folder before a launch path */
#define COLL_LINE   640

/* Why a name cannot be a collection's (coll_name_check) */
typedef enum {
	COLL_NAME_OK,
	COLL_NAME_EMPTY,       /* nothing but spaces */
	COLL_NAME_BAD,         /* a character a card's file name cannot hold */
	COLL_NAME_LONG,        /* longer than COLL_NAME - 1 bytes */
	COLL_NAME_TAKEN,       /* another collection has it, in any case */
} coll_name_err;

/* Every Collections/<name>.txt on `card`, read again; a missing folder is no
 * collection. The card's root is kept for the writes. */
void coll_load(const char *card);

/* Let go of what coll_load read */
void coll_free(void);

int         coll_count(void);
/* Collection i's name, "" past the end */
const char *coll_name(int i);
/* The collection named `name`, in any case, or -1 */
int         coll_find(const char *name);

/* How many lines collection i has, games or not */
int  coll_lines(int i);
/* How many of them name a game, on the card or not */
int  coll_games(int i);
/* Its k-th line's game: the system's folder under Roms/ and the launch path
 * in it. False past the end or for a line that names none. */
bool coll_game(int i, int k, char *folder, size_t fn, char *file, size_t n);
/* Whether collection i has the game */
bool coll_has(int i, const char *folder, const char *file);

/* The game added at the end of collection i, written through; true when it
 * is in it now. Already in it: nothing written, true. */
bool coll_add(int i, const char *folder, const char *file);
/* The game's lines taken out of collection i, written through; true when it
 * is not in it now */
bool coll_remove(int i, const char *folder, const char *file);

/* Whether `name` (spaces at its ends dropped) can be a new collection's, or
 * collection `self`'s new one (-1 for a new one) */
coll_name_err coll_name_check(const char *name, int self);
/* A collection made with its first game, its file written; its index, or -1
 * when the name cannot be one (coll_name_check) or the card refused */
int  coll_create(const char *name, const char *folder, const char *file);
/* Collection i renamed, its file with it; its index after (the order is A to
 * Z), or -1 and nothing changed */
int  coll_rename(int i, const char *name);
/* Collection i deleted, its file with it; its games stay on the card */
bool coll_delete(int i);

/* A game renamed (src/gamern.h) or deleted (src/gamedel.h): every list that
 * names it follows, and is written through. How many lists changed. */
int  coll_follow(const char *folder, const char *file, const char *new_file);
int  coll_forget(const char *folder, const char *file);

/* The line this writes for a game, "/Roms/<folder>/<file>" */
void coll_line_for(const char *folder, const char *file, char *out, size_t n);
/* A line as read, taken apart: the system's folder and the launch path in
 * it. False for an empty line, a note (#) or one that names no game. */
bool coll_parse(const char *line, char *folder, size_t fn, char *file, size_t n);

#endif
