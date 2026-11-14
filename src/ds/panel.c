/* SPDX-License-Identifier: GPL-3.0-only */
/* A panel's slab, rule and legend (src/ds/panel.h): what the menus, the
 * tiles, the list and Player 2's panel each drew with the same lines. */
#include "ds/panel.h"

#include "design.h"
#include "keyhints.h"
#include "keyhints_draw.h"
#include "ui.h"

void ds_panel_slab(SDL_Renderer *r, const SDL_Rect *panel, unsigned lift_rgb, bool lift)
{
	if (lift) ui_lift(r, panel, lift_rgb, UI_LIFT_PANEL);
	ui_panel(r, panel, DS_RADIUS_PANEL);
}

bool ds_panel_open(SDL_Renderer *r, const SDL_Rect *panel, unsigned lift_rgb, mm_pose pose,
                   const ds_pass *pass)
{
	bool posed = pose.scale != 1.0f || pose.alpha != 1.0f;

	/* Moving, the lift goes straight on the screen at the panel's size and
	 * its strength, and the panel into the pass: a shadow or a glow drawn
	 * into a clear texture and copied out with its alpha would be faded
	 * twice over. */
	if (posed) {
		SDL_FRect f = pass->scaled(panel, pose.scale);
		SDL_Rect q = { (int)(f.x + 0.5f), (int)(f.y + 0.5f), (int)(f.w + 0.5f), (int)(f.h + 0.5f) };

		ui_lift_k(r, &q, lift_rgb, UI_LIFT_PANEL, pose.alpha);
		posed = pass->begin(pass->ctx);
		ui_panel(r, panel, DS_RADIUS_PANEL);
		return posed;
	}
	ds_panel_slab(r, panel, lift_rgb, true);
	return false;
}

void ds_panel_close(const SDL_Rect *panel, mm_pose pose, bool posed, const ds_pass *pass)
{
	if (posed) pass->end(pass->ctx, panel, pose);
}

void ds_panel_rule(SDL_Renderer *r, int x, int y, int w, SDL_Color c)
{
	SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
	SDL_RenderFillRect(r, &(SDL_Rect){ x, y, w, DS_RULE });
}

void ds_panel_legend(SDL_Renderer *r, const char *legend, int x0, int x1, int cy)
{
	if (!legend) return;
	if (keyhints_is(legend))
		keyhints_draw_packed(r, legend, x0, x1, cy, 255);
	else
		ui_text(r, ui_font(UI_F_META), legend, (x0 + x1) / 2, ui_cap_y(ui_font(UI_F_META), cy), 0, UI_TEXT_DIM);
}
