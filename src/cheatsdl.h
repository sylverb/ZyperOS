/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_CHEATSDL_H
#define ZYPEROS_CHEATSDL_H
/* Cheats, a kind of MENU > Downloads (DESIGN.md 11.53): libretro's cheat
 * list, only the card's games' files, so the in-game Cheats list (11.66)
 * has them offline.
 *
 * libretro's buildbot keeps the list as one zip, cheats.zip (about 39 MB,
 * 28,757 files in a folder per system, rebuilt every day from
 * libretro-database's cht/ folder, CC BY-SA 4.0). The server takes byte
 * ranges, so a run reads the zip's last 64 KB (the end of its index, which
 * says where the index is and how long), then the index itself (about
 * 4.3 MB: every file's name, size, checksum and place), and then fetches only
 * the entries that match games on the card, a few KB each, inflating each
 * one here. The index is kept on the card with the place and size it was
 * at: the next run asks the last 64 KB only, and reads the index again only
 * when libretro rebuilt it.
 *
 * Matching: libretro's folder is the system's collection name (the Scraper's
 * map, art_lr_collection), and a file is named after the game: the ROM's
 * name exactly, then the same name with a code type after it ("(Game
 * Genie)"), then the same title in libretro's loose rule (art_norm) and the
 * same region.
 *
 * The files, as agreed with the in-game list (the cheats PR's contract):
 *   Cheats/<Roms folder>/.libretro/<ROM base>/<the entry's name>
 *       the entry's bytes exactly, written as .part and renamed, so a file's
 *       CRC-32 is the index's (the update check); several entries, several
 *       files
 *   Cheats/<Roms folder>/.libretro/<ROM base>/.none
 *       checked: libretro has none
 * No folder: not checked yet. Your own Cheats/<Roms folder>/<ROM base>.cht is
 * never written or deleted here. An update fetches only what changed. */
#include "app.h"

#include <stdbool.h>

#define CHT_URL "https://buildbot.libretro.com/assets/frontend/cheats.zip"
/* A cheat file's room on the card, about (the lab's measure: 3 MB for 428) */
#define CHT_MB_FILE 0.007
/* The index's room, read once a rebuild */
#define CHT_MB_INDEX 4.3

/* A system's cheats as the card has them: games, checked with files, checked
 * with none, not checked; whether libretro has a folder for it at all */
typedef struct {
	int games, have, none, todo;
	bool mapped;
} cheats_count;
bool cheats_mapped(const char *folder);
void cheats_count_sys(const char *folder, const game_list *gl, cheats_count *out);
/* A game's state: 1 files on the card, 0 not checked, -1 libretro has none */
int cheats_game_state(const char *folder, const char *rom_name);

/* ---- the run (a lane of src/dlq.h) -------------------------------------------- */

enum { CHT_WORKING, CHT_NO_NETWORK, CHT_RETRYING };
enum { CHT_OK, CHT_NO_ANSWER, CHT_BAD_LIST, CHT_NO_ROOM };

/* Start, on `sys`'s systems (`only` one console's folder, or NULL), with
 * `games` the games it will look at (the page's count; 0: each folder is
 * listed for it). False when one is going. Step a frame's work: 1 going, 0
 * done, -1 not going. */
bool cheats_begin(const systems_cfg *sys, const char *only, int games);
int  cheats_step(void);
void cheats_cancel(void);
bool cheats_running(void);

typedef struct {
	int state, problem;
	int games, games_done;
	int fetched;          /* games whose files came new */
	int updated;          /* games whose files changed and came again */
	int none;             /* games libretro has nothing for */
	int failed;
	unsigned resume_at;   /* a retry's time, in plat_now_ms */
	char now[256];        /* the game it looks at */
	char folder_now[CFG_STR];
	/* what came last, for a screen that shows them landing */
	unsigned seq;
	char got_game[256], got_folder[CFG_STR];
	int got_count;        /* its cheats */
} cheats_progress;
void cheats_status(cheats_progress *out);

/* ---- one game: the in-game list's Get Cheats ----------------------------------- */

/* The same for one game alone, `rom_file` its launch path under
 * Roms/<folder>: false when a run is going or the system has no list.
 * Step: 1 still working, 0 done (its files, or .none, are on the card),
 * -1 failed. Uses its own network slot (NET_SLOT_CHT). */
bool cheats_dl_one_begin(const char *folder, const char *rom_file);
int  cheats_dl_one_step(void);

/* Every downloaded cheat (On the Card): each system's .libretro folder, the
 * index kept, and the in-game list's switches (the library's cheats.* keys:
 * cheats that were on turn off). Your own files stay. One system's, for
 * Systems' X. */
void cheats_delete_all(const systems_cfg *sys);
void cheats_delete_sys(const char *folder);

#endif
