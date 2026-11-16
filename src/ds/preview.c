/* SPDX-License-Identifier: GPL-3.0-only */
/* See preview.h */
#include "ds/preview.h"

#include "design.h"
#include "ds/flex.h"
#include "ds/layout.h"
#include "linebreak.h"
#include "theme.h"
#include "ui.h"

#include <math.h>

int ds_preview_col_w(int content_w, int rows_w)
{
	int w = content_w - rows_w;

	if (w > DS_PREVIEW_COL_W) w = DS_PREVIEW_COL_W;
	if (w < DS_PREVIEW_COL_MIN) w = DS_PREVIEW_COL_MIN;
	return w;
}

int ds_preview_note_lines(const char *text, int w)
{
	char lines[DS_PREVIEW_NOTE_LINES][LB_LINE];

	if (!text || !*text) return 0;
	return ui_balance_lines(ui_font(UI_F_META), text, w, DS_PREVIEW_NOTE_LINES, lines);
}

ds_preview_frame ds_preview_layout(int x, int y, int col_w, int note_lines)
{
	static ds_flex L;
	const int inner = col_w - DS_PREVIEW_GAP;
	ds_preview_frame f;
	int screen, note;

	ds_flex_begin(&L, inner, inner * 3 / 4 + DS_PREVIEW_NOTE_GAP + note_lines * DS_LINE_META,
	              &(ds_box){ .dir = DS_COL, .gap = DS_PREVIEW_NOTE_GAP, .align = DS_A_STRETCH });
	screen = ds_flex_add(&L, 0, &(ds_box){ .aspect_w = 4, .aspect_h = 3, .w = inner, .h = inner * 3 / 4 });
	note = ds_flex_add(&L, 0, &(ds_box){ .h = note_lines * DS_LINE_META });
	ds_flex_solve(&L);
	f.screen = ds_flex_rect(&L, screen);
	f.note = ds_flex_rect(&L, note);
	f.screen.x += x; f.screen.y += y;
	f.note.x += x;   f.note.y += y;
	return f;
}

SDL_Rect ds_preview_screen_begin(SDL_Renderer *r, const SDL_Rect *screen)
{
	const SDL_Rect in = { screen->x + DS_FRAME, screen->y + DS_FRAME,
	                      screen->w - 2 * DS_FRAME, screen->h - 2 * DS_FRAME };

	ui_round_rect(r, screen, DS_RADIUS_ROW, ui_col(theme()->edge, 255));
	ui_round_rect(r, &in, DS_RADIUS_ROW - DS_FRAME, ui_col(theme()->ink, 255));
	return in;
}

void ds_preview_screen_end(SDL_Renderer *r, const SDL_Rect *screen)
{
	/* The inside's corners, which a picture drawn to its rect squared off,
	 * back to the frame's: the edge's color where the round leaves off */
	const SDL_Rect in = { screen->x + DS_FRAME, screen->y + DS_FRAME,
	                      screen->w - 2 * DS_FRAME, screen->h - 2 * DS_FRAME };
	const int rr = DS_RADIUS_ROW - DS_FRAME;
	const ds_rgb e = theme()->edge;
	int dy;

	SDL_SetRenderDrawColor(r, e.r, e.g, e.b, 255);
	for (dy = 0; dy < rr; dy++) {
		const float cy = rr - dy - 0.5f;
		const int span = (int)lroundf(rr - sqrtf(rr * rr - cy * cy));

		if (span <= 0) continue;
		SDL_RenderFillRect(r, &(SDL_Rect){ in.x, in.y + dy, span, 1 });
		SDL_RenderFillRect(r, &(SDL_Rect){ in.x + in.w - span, in.y + dy, span, 1 });
		SDL_RenderFillRect(r, &(SDL_Rect){ in.x, in.y + in.h - 1 - dy, span, 1 });
		SDL_RenderFillRect(r, &(SDL_Rect){ in.x + in.w - span, in.y + in.h - 1 - dy, span, 1 });
	}
}

void ds_preview_note(SDL_Renderer *r, const SDL_Rect *note, const char *text)
{
	char lines[DS_PREVIEW_NOTE_LINES][LB_LINE];
	TTF_Font *f = ui_font(UI_F_META);
	int n, i;

	if (!text || !*text) return;
	n = ui_balance_lines(f, text, note->w, DS_PREVIEW_NOTE_LINES, lines);
	for (i = 0; i < n; i++)
		ui_text(r, f, lines[i], note->x, ui_line_y(UI_F_META, note->y + i * DS_LINE_META), -1,
		        UI_TEXT_SOFT);
}
