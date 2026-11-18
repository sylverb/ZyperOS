/* SPDX-License-Identifier: GPL-3.0-only */
/* What an option list's cursor shows (DESIGN.md 11.3, 1.0.6 rc.2: Theme's
 * lists): the choice under it, before A takes it.
 *
 * - show(k): choice k on the screen as it would be once chosen - the accent
 *   on every surface, a ground of either theme. In memory only: a show
 *   writes no setting and no file, so B, a close or POWER leave the card as
 *   it was.
 * - undo(kept): what is kept, back; `kept` the list's kept choice (-1: none
 *   on as a whole). Safe at any time, shown or not.
 *
 * A list runs it through ds_hover_run: show once each time the cursor comes
 * to another choice (never every frame: a show repaints the whole screen),
 * undo when the cursor is back on the kept one, and undo when the list
 * closes, whichever way - before the choice A took is applied, so the row's
 * own Left and Right step it from the kept one as they always did
 * (src/menu.h's menu_choice) and its screen saves it.
 *
 * Part of the design system (src/ds/): no app.h, no panel.h; `ctx` is the
 * caller's (the app). */
#ifndef ZYPEROS_DS_HOVER_H
#define ZYPEROS_DS_HOVER_H

#include <stdbool.h>

typedef struct ds_hover {
	void (*show)(void *ctx, int k);
	void (*undo)(void *ctx, int kept);
} ds_hover;

typedef struct ds_hover_run {
	const ds_hover *h;
	void *ctx;
	int kept;          /* the choice that is on; -1 none is on as a whole */
	int at;            /* the cursor's choice */
	int shown;         /* the choice shown; -1 none: what is kept is on */
} ds_hover_run;

/* A list opens with `kept` on (-1: none on as a whole) and the cursor on
 * `at`. Nothing is shown yet: the first ds_hover_at shows `at` if it is not
 * the kept one. */
void ds_hover_begin(ds_hover_run *r, const ds_hover *h, void *ctx, int kept, int at);

/* Each pass of the list, the cursor on `k`: shows k once (once per choice
 * it comes to), or puts the kept one back when k is the kept one. NULL h
 * or no show: nothing. */
void ds_hover_at(ds_hover_run *r, int k);

/* The list has closed, whichever way: undo if anything was shown, before
 * the choice taken is applied. The run is spent (no hover). */
void ds_hover_end(ds_hover_run *r);

/* A list is showing a choice that is not kept (a show not yet undone):
 * whatever keeps what is on screen - the boot screen's words, the theme
 * file Auto's hour writes - waits until it is undone, so a show is never
 * written down by something else */
bool ds_hover_showing(void);

#endif
