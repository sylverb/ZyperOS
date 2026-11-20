/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_CHEATS_H
#define ZYPEROS_CHEATS_H

/* A game's cheats (DESIGN.md 11.66): the .cht files on the card for it, the
 * list the in-game menu shows, which are on, and the codes Diatom is handed.
 *
 * WHERE THEY ARE. Under Cheats/ at the card's top, beside Roms/:
 *
 *   Cheats/<Roms folder>/<game>.cht           the player's own file
 *   Cheats/<Roms folder>/<game file>.cht      the same, named as NextUI names
 *                                             it: the file with its extension
 *   Cheats/<tag>/<game file>.cht and          NextUI's folder, the system's
 *   Cheats/<tag>/<game>.cht                   tag ("SFC"), so a card moves
 *                                             over as it is
 *   Cheats/<Roms folder>/.libretro/<game>/    libretro's files for the game,
 *                                             as the Downloads page writes
 *                                             them, a file each as in
 *                                             libretro's zip; an empty .none
 *                                             in it says libretro has none
 *
 * <game> is the game's file name without its folders and its extension, so
 * a game's cheats go with it as its saves do. The player's own files are shown first ("Your
 * File") and nothing but the player writes them. libretro's files follow, a
 * group a file, each under a seam named for its code type, the part of the
 * file's name after the game's ("Game Genie").
 *
 * WHICH ARE ON is kept per game in the card's database (cheats_key): the
 * cheats' ids, a hash of the group's file name, the description and the
 * code, so a list that gains or loses a cheat in an update keeps the others
 * as they were, and one whose code changed turns itself off. Nothing is
 * ever written into a .cht file.
 *
 * Every cheat starts off: a file's own cheatN_enable is not read, since a
 * cheat that is on stops the game's achievements (Diatom's cheats.h).
 *
 * Pure but for reading the card: no SDL, no database (the state goes in and
 * out as a string), so tools/cheats-check.c holds it to the cases. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cht.h"

/* A game's list at most: more is cut. libretro's longest is about 900. */
#define CHEATS_MAX    1024
#define CHEATS_GROUPS 16
#define CHEATS_LABEL  96

/* Where the codes that are on go for Diatom, a code a line (RUN cheats=,
 * SETCHEATS path=): on the device's /tmp, never the card */
#define CHEATS_RUN_FILE "/tmp/zyperos-cheats.txt"

typedef struct {
	char    *desc;    /* "" for a cheat its file gives no description */
	char    *code;
	uint32_t id;      /* cheats_id */
	uint8_t  group;   /* into cheat_list.g */
	bool     on;
} cheat;

typedef struct {
	char label[CHEATS_LABEL];   /* the code type, "" for none */
	bool yours;                 /* the player's own file */
} cheat_group;

/* Why a game has no cheats, for the list's one sentence */
typedef enum {
	CHEATS_HAVE,          /* it has some */
	CHEATS_NOT_CHECKED,   /* no file, and Downloads has not looked yet */
	CHEATS_NONE_LIBRETRO  /* Downloads looked: libretro has none (.none) */
} cheats_where;

typedef struct {
	cheat        c[CHEATS_MAX];
	int          n;
	cheat_group  g[CHEATS_GROUPS];
	int          ng;
	cheats_where where;
} cheat_list;

/* The id of one cheat: FNV-1a over its group's file name, description and
 * code, so the same cheat in the same file is the same id on every card */
uint32_t cheats_id(const char *file, const char *desc, const char *code);

/* The game `file` (its name in Roms/<folder>, extension and all) on the
 * system whose Roms folder is `folder` and tag is `tag`, with the card at
 * `card`: every cheat its files give, all off. False when it has none
 * (`where` says why). Free it with cheats_free. */
bool cheats_load(cheat_list *L, const char *card, const char *folder,
                 const char *tag, const char *file);
void cheats_free(cheat_list *L);

/* Which are on, from and to the database's string: ids in hex, comma
 * separated. An id no cheat in the list has is dropped. */
void cheats_state_apply(cheat_list *L, const char *state);
void cheats_state_str(const cheat_list *L, char *out, size_t n);
int  cheats_on_count(const cheat_list *L);

/* The database's key for a game's state, cheats.<tag>.<file>, and the one
 * that says the first cheat has been asked about (cheats-asked.<tag>.<file>) */
void cheats_key(const char *tag, const char *file, char *out, size_t n);
void cheats_asked_key(const char *tag, const char *file, char *out, size_t n);

/* The codes that are on, a line each, into `path` (atomic). How many, or -1
 * when it could not be written. */
int  cheats_write_codes(const cheat_list *L, const char *path);

/* The order the list is shown in: groups as they are, or (`on_first`) the
 * cheats that are on before the rest, each in its place otherwise. Into
 * `order`, CHEATS_MAX of them; how many. */
int  cheats_order(const cheat_list *L, bool on_first, int *order);

#endif
