/* SPDX-License-Identifier: GPL-3.0-only */
/* Badges, switch marks and the progress bar (src/ds/mark.h). The badge and
 * the switch were the Scraper page's scp_badge and scp_mark, and the bar its
 * scp_bar and Flash Drive's tile's, moved from src/main.c as they were. */
#include "ds/mark.h"

#include "ds/layout.h"
#include "ui.h"

void ds_badge(SDL_Renderer *r, int cx, int cy, int d, ui_glyph g, bool lime, Uint8 al)
{
	const ds_theme *t = theme();
	const int e = DS_BADGE_RING;

	ui_round_rect(r, &(SDL_Rect){ cx - d / 2 - e, cy - d / 2 - e, d + 2 * e, d + 2 * e }, d / 2 + e,
	              ui_col(t->edge, al));
	ui_round_rect(r, &(SDL_Rect){ cx - d / 2, cy - d / 2, d, d }, d / 2, ui_col(lime ? t->accent : t->surface, al));
	ui_glyph_draw(r, g, cx, cy, d * 64 / 100, ui_col(lime ? t->on_accent : t->accent_ink, al));
}

void ds_switch(SDL_Renderer *r, int cx, int cy, int d, ui_glyph g, bool on)
{
	const ds_theme *t = theme();
	const int e = DS_BADGE_RING;

	if (on) { ds_badge(r, cx, cy, d, g, true, 255); return; }
	ui_round_rect(r, &(SDL_Rect){ cx - d / 2, cy - d / 2, d, d }, d / 2, ui_col(t->edge, 255));
	ui_round_rect(r, &(SDL_Rect){ cx - d / 2 + e, cy - d / 2 + e, d - 2 * e, d - 2 * e }, d / 2 - e,
	              ui_col(t->raised, 255));
	ui_glyph_draw(r, g, cx, cy, d * 60 / 100, UI_TEXT_DIM);
}

void ds_play_disc(SDL_Renderer *r, int x, int y, int d, Uint8 al)
{
	ui_round_rect(r, &(SDL_Rect){ x, y, d, d }, d / 2, ui_col(theme()->accent, al));
	/* On the disc's middle: the glyph centers its triangle by its centroid */
	ui_glyph_draw(r, UI_GLYPH_PLAY, x + d / 2, y + d / 2, d / 2, ui_col(theme()->on_accent, al));
}

void ds_more(SDL_Renderer *r, float cx, float top, bool up, SDL_Color c)
{
	const float hw = DS_MORE_W / 2.0f, h = DS_MORE_H;
	SDL_Vertex v[3] = {
		{ { cx - hw, up ? top + h : top }, c, { 0, 0 } },
		{ { cx + hw, up ? top + h : top }, c, { 0, 0 } },
		{ { cx, up ? top : top + h }, c, { 0, 0 } },
	};

	SDL_RenderGeometry(r, NULL, v, 3, NULL, 0);
	ui_fit_mark(&(SDL_Rect){ (int)(cx - hw), (int)top, DS_MORE_W, DS_MORE_H });
}

void ds_bar_in(SDL_Renderer *r, const SDL_Rect *track, int fill, SDL_Color track_c, SDL_Color fill_c)
{
	ui_round_rect(r, track, track->h / 2, track_c);
	if (fill > 0 && fill < track->h) fill = track->h;
	if (fill > 0)
		ui_round_rect(r, &(SDL_Rect){ track->x, track->y, fill, track->h }, track->h / 2, fill_c);
}

void ds_bar(SDL_Renderer *r, int x, int y, int w, int fill, Uint8 al)
{
	ds_bar_in(r, &(SDL_Rect){ x, y, w, UI_BAR_H }, fill, ui_col(theme()->track, al), ui_col(theme()->accent_ink, al));
}
