/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_SHOTS_SCREEN_H
#define ZYPEROS_SHOTS_SCREEN_H

/* The Screenshots shelf (DESIGN.md 11.68; src/shotname.h names the files,
 * src/shelfbuild.c builds the group): what is its own on a shelf that is
 * otherwise every shelf - its words under a shot and at the foot, A opening
 * a shot in the Video view, L1 and R1 to the next game's shots, X deleting
 * one, and the picture the viewer shows.
 *
 * My picks in the Screenshots lab (2026-11-20), each in one place below. A
 * different pick is a change here, or at the line that says which. */
#include "app.h"
#include "keyhints.h"
#include "library.h"

#include <SDL.h>
#include <stdbool.h>
#include <stddef.h>

/* 1. Where it lives: a group after Collections (src/shelfbuild.c,
 *    build_shots_shelf's `at`), with a Home Systems row, On as it ships.
 * 2. When it shows: once the card has a shot, and not before
 *    (build_shots_shelf: no shot, no shelf); no empty page.
 * 3. Its order: one stream, the newest first, MENU > Sort By's three
 *    (SHOTS_SORT_DEFAULT, sort.h's SORT_SHOTS_ORDERS). */
#define SHOTS_SORT_DEFAULT "added"
/* 4. Its card: the shot at its own shape with the game's cover small in its
 *    lower left corner, Recents' Last Screen card (src/screencard.h); 0 is
 *    the shot alone */
#define SHOTS_CARD_COVER 1
/* 5. A on a shot opens it in the Video view (src/videoview.h), the shot
 *    where the snap goes (shots_keys).
 * 6. The viewer is for looking: no key plays the shot's game there, A plays
 *    deny (shots_keys); the game starts from its own shelf. */
/* 7. X deletes it, asked first (IN_Y for Y, IN_NONE for no delete) */
#define SHOTS_DELETE_KEY IN_X
/* 8. OpenBOR's own shots, Saves/openbor/ScreenShots, are on the shelf too,
 *    under OpenBOR (src/shelfbuild.c's shots_read, src/shotname.h). */

/* The viewer's picture, asked of the art workers apart from the shelf's
 * cards: shelf_put hands it to shots_view_put */
#define SHOTS_VIEW_SYS (-200)

/* Whether shelf `s` is the Screenshots shelf */
bool shots_shelf(app *a, int s);

/* The picture of shot `g`, its path on the card (n bytes): its `file` is
 * its path from the card's root */
void shots_png(const game_entry *g, char *out, size_t n);

/* The line under a shot's name: its system and when it was taken ("SNES ·
 * today 21:40", "PlayStation · 3d ago"), in a buffer of its own */
const char *shots_facts(app *a, int o, const game_entry *g);

/* The shelf's A word and foot, in `video` (the viewer) or the other views:
 * B, START, X; `foot` holds 4 */
void shots_hints(app *a, sysview *v, bool video, key_hint *foot, int *nfoot, const char **a_word);

/* This pass's keys of the Screenshots shelf. shots_jump, before the shelf
 * moves: L1 and R1 in the viewer to the next or the last game's shots; true
 * when it moved. shots_keys, after: A, B, X, Y and START where they are the
 * shelf's own; true when it took the press. */
bool shots_jump(app *a, sysview *v);
bool shots_keys(app *a, sysview *v);

/* The focused shot's picture for the viewer, or NULL while it comes (the
 * view draws its card till then); its size in w, h */
SDL_Texture *shots_view_tex(app *a, sysview *v, int *w, int *h);
void shots_view_put(app *a, int idx, SDL_Surface *s, bool card);
/* Let go: the shelf is built again and its places name other shots */
void shots_view_drop(void);

/* Diatom said a shot was written (main.c's on_hotkey): the shelf is read
 * again when the game ends (shots_take_new, launch_screen.c) */
void shots_taken(void);
bool shots_take_new(void);

/* One frame of Delete Screenshot's question over the shelf, the cursor on
 * Cancel (--shots-delete) */
void shots_delete_preview(app *a);
/* Delete Screenshot's yes on the shot under the cursor: its file off the
 * card and the shelf read again; false when the card would not let it go.
 * The question asks first (X); --shots-delete-now goes straight here. */
bool shots_delete_now(app *a, sysview *v);

/* The shelf in its order, MENU > Sort By's (sort_shelf) */
void shots_sort(app *a, int s);

#endif
