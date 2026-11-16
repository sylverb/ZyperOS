/* SPDX-License-Identifier: GPL-3.0-only */
/* A choice's picture beside its list (DESIGN.md 11.3 and section 15): a
 * column at an option list's left, where the Controls tab's handheld
 * stands, holding the handheld's screen in small - framed, its corners the
 * focus fill's beside it - with what the choice under the cursor does to
 * the game on it, and under that the choice's note in meta. The list
 * itself is menu_draw's rows (the runner's side column, menu_style.side), so
 * its cursor, tick, sounds and keys are every list's.
 *
 * Laid out by src/ds/flex.h, one column: the screen a 4:3 leaf as wide as
 * the column less DS_PREVIEW_GAP, then DS_PREVIEW_NOTE_GAP, then the note's
 * room, as many meta lines (DS_LINE_META) as the tallest note of the set
 * takes, up to DS_PREVIEW_NOTE_LINES, so nothing moves as the cursor does.
 * The column is DS_PREVIEW_COL_W, narrower where the choices run long, to
 * DS_PREVIEW_COL_MIN, so every choice keeps its words. Part of the design
 * system (src/ds/): no app.h, no panel.h. */
#ifndef ZYPEROS_DS_PREVIEW_H
#define ZYPEROS_DS_PREVIEW_H

#include <SDL.h>

/* Four lines in the widest column; six where the column narrows for a
 * language's long choices (Spanish, Portuguese), so a note is never cut */
#define DS_PREVIEW_NOTE_LINES 6

typedef struct {
	SDL_Rect screen;   /* the small screen, its frame included */
	SDL_Rect note;     /* the note's room */
} ds_preview_frame;

/* The column's width beside rows whose widest is `rows_w`, in a panel's
 * content `content_w` wide: DS_PREVIEW_COL_W, or what is left down to
 * DS_PREVIEW_COL_MIN */
int ds_preview_col_w(int content_w, int rows_w);

/* The lines `text` takes as a note `w` wide, up to DS_PREVIEW_NOTE_LINES:
 * the tallest of a set is the room ds_preview_layout keeps */
int ds_preview_note_lines(const char *text, int w);

/* Its parts in the column at (x, y), `col_w` wide, the note's room
 * `note_lines` lines */
ds_preview_frame ds_preview_layout(int x, int y, int col_w, int note_lines);

/* The small screen's ground and frame: **ink** inside, the **edge**'s
 * DS_FRAME line round it, corners DS_RADIUS_ROW; returns the inside, where
 * the picture goes (a caller clips to it). Then ds_preview_screen_end rounds
 * the inside's corners over what was drawn. */
SDL_Rect ds_preview_screen_begin(SDL_Renderer *r, const SDL_Rect *screen);
void ds_preview_screen_end(SDL_Renderer *r, const SDL_Rect *screen);

/* The note in its room: meta, **text-soft**, left, DS_LINE_META apart */
void ds_preview_note(SDL_Renderer *r, const SDL_Rect *note, const char *text);

#endif
