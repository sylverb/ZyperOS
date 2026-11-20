/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_DLCOUNT_H
#define ZYPEROS_DLCOUNT_H
/* What MENU > Downloads counts (src/dl_screen.h): each system's games and,
 * for each kind of game data, what it has and what it lacks.
 *
 * A count reads the card: a system's folder, its covers' and videos'
 * folders, the library's titles, its .cheevos folder. On a big card that is
 * seconds, and the page used to take all of them before it drew (Tarik,
 * 1.1.0: "we need a loading state. It took a while to open"). So a count
 * goes a system at a time, inside a frame's budget, while the page draws and
 * takes keys; the page shows what it has, and a count it made before stays
 * on the screen while a new one goes (the library's is kept between
 * openings, and MENU's row reads it). The counting is the Scraper's and the
 * achievements download's own (scraper_count_sys, radl_plan_one), so the
 * kind pages and the runs agree with it.
 *
 * The switches are read when the totals are made, not when a system is
 * counted, so Left and Right on the page change the numbers at once with no
 * count. */
#include "app.h"
#include "snaps.h"

#include <stdbool.h>

typedef enum { DLK_COVERS, DLK_VIDEOS, DLK_NAMES, DLK_ACH, DLK_KINDS } dl_kind;

typedef struct {
	char folder[CFG_STR], tag[8], name[CFG_STR], key[CFG_STR];
	int  games;
	int  cov_miss;
	int  vid_need[SNAPS_FOR_COUNT], vid_both[SNAPS_FOR_COUNT], vid_have;
	unsigned long long vid_bytes;
	int  names_todo, names_have;
	bool names_ok;            /* an account and ScreenScraper knows the system */
	bool ach_sys;             /* RetroAchievements has games of it on the card */
	int  ach_games, ach_ready, ach_todo, ach_none;
	bool fx_on[DLK_KINDS];    /* a shot's switches (dl_count_fixture) */
} dl_sys;

typedef struct {
	dl_sys s[CFG_MAX_SYSTEMS];
	int  n;
	char only[CFG_STR];       /* a console's count, or "" for the library */
	unsigned long long free;  /* the card's room, 0 when it cannot say */
	double mb_cover, mb_video;
	bool ss_in, ra_in;
	bool fx_in[DLK_KINDS];    /* a shot's kinds in Download (dl_count_fixture) */
} dl_counts;

/* ---- counting ---------------------------------------------------------------- */

/* A new count of the library (`only` NULL or "") or of one console: from
 * its first system, the last count of the same kept on show meanwhile */
void dl_count_again(app *a, const char *only);
/* Count on for at most `budget_ms`: true while it still counts */
bool dl_count_step(app *a, unsigned budget_ms);
bool dl_counting(void);
/* How far: systems counted, of how many */
void dl_count_progress(int *done, int *of);
/* The last whole count for the scope, NULL when there is none yet. A
 * console's comes from the library's when that has it. */
const dl_counts *dl_count_shown(const char *only);
/* Its turn: a number that changes each time a count finishes */
unsigned dl_count_gen(void);
/* The library's count, as a run changed the card: the next page counts again */
void dl_count_stale(void);
bool dl_count_is_stale(void);

/* A shot's stand-in count (NULL: none): shown for every scope, and its own
 * switches read in place of the database's */
void dl_count_fixture(const dl_counts *fx);

/* ---- the switches ------------------------------------------------------------- */

/* A kind in Download or out of it (Left and Right on its row): Covers,
 * Videos and Achievements are dl.off.<kind> (absent is in); Names and
 * Details is the Scraper's own switch (scraper.names) */
bool dl_kind_in(dl_kind k);
void dl_kind_set(dl_kind k, bool in);
/* A system's switch for a kind (Systems' cell): Covers and Videos are the
 * Scraper's (scraper.covers.<folder>, snaps.get.<folder>), Names follow the
 * system's covers unless dl.off.names.<folder>, Achievements are
 * dl.off.ach.<folder> (absent is on) */
bool dl_sys_on(dl_kind k, const char *folder);
void dl_sys_set(dl_kind k, const char *folder, bool on);
/* Why a kind cannot be had at all, or NULL: Videos and Names need a
 * ScreenScraper account, Achievements a sign-in (a string id's text) */
const char *dl_kind_blocked(const dl_counts *c, dl_kind k);

/* ---- the totals ------------------------------------------------------------------ */

typedef struct {
	int todo;        /* what a download would get */
	int have;        /* on the card */
	int none;        /* the source has none (achievements) */
	int on;          /* systems whose switch is on */
	int all;         /* systems that have the kind at all */
	int have_sys;    /* systems with some of it on the card */
	int todo_sys;    /* systems with some of it to get */
	unsigned long long bytes;   /* on the card */
} dl_sum;
void dl_sum_of(const dl_counts *c, dl_kind k, dl_sum *o);
/* One system's cell (Systems): what is missing, and whether it is on */
int dl_sys_todo(const dl_counts *c, int i, dl_kind k, bool *on);

/* What Download gets with the kinds that are in (`only` one kind, or -1 for
 * every one that is in): covers to look for, videos, names lookups and
 * achievement games; the bytes it adds and about how long it takes, the two
 * servers side by side (the longer of the two); whether it fits with
 * DL_RESERVE to spare; and `n`, the things it would get (MENU's row) */
typedef struct {
	int covers, videos, names, looks, ach, ach_sys;
	int n;
	double bytes, art_secs, ra_secs, secs;
	bool fits;
} dl_plan;
void dl_plan_of(const dl_counts *c, int only, dl_plan *o);
#define DL_RESERVE (200ULL * 1000 * 1000)

/* Estimates the pages say: a set and its badges (radl_screen.c's), the
 * time a RetroAchievements game and system take */
#define DL_MB_SET   0.12
#define DL_S_SET    1.3
#define DL_S_SYSTEM 2.0
#define DL_S_SS     2.0
#define DL_S_LR     1.0
#define DL_S_VIDEO  2.0

#endif
