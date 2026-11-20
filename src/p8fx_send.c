/* SPDX-License-Identifier: GPL-3.0-only */
/* Your PICO-8's look file from the shelf's rows. See src/p8fx_send.h. */
#include "p8fx_send.h"

#include <stdio.h>
#include <string.h>

#include "border.h"
#include "border_pic.h"
#include "dmodepreview.h"
#include "p8draw.h"
#include "p8fx.h"
#include "platform.h"
#include "strength.h"

bool p8fx_room(const app *a, int sys)
{
	const int k = a->view[sys].dmode;
	dmode_rect r;
	int w, h;

	dmode_surface(&w, &h);
	p8_draw_rect(DMODES[k >= 0 && k < DMODE_COUNT ? k : 0].name, w, h, P8_NEG_OK, &r);
	return !p8fx_covers(r.x, r.y, r.w, r.h, w, h);
}

bool p8fx_send(app *a, int sys)
{
	const system_cfg *s = &a->sys.systems[sys];
	const sysview *v = &a->view[sys];
	const char *path = plat_flag(P8FX_FILE);
	border_kind k = border_load_room(a, sys, p8fx_room(a, sys));
	p8fx_look l;
	bool ok;

	p8fx_look_default(&l);
	snprintf(l.shader, sizeof l.shader, "%s", SHADERS[v->shader >= 0 && v->shader < SHADER_COUNT ? v->shader : 0].name);
	l.strength = strength_clamp(v->strength);
	if (k == BORDER_BLUR) l.border = P8FX_BORDER_BLUR;
	/* Your Own: the picture the panel's size, as Diatom is handed it
	 * (border_send) */
	if (k == BORDER_OWN) {
		char png[CFG_STR * 3] = "";
		const char *file = plat_flag(P8FX_BORDER_FILE);
		SDL_Surface *pic = NULL;
		int w, h;

		dmode_surface(&w, &h);
		if (border_own_path(s, png, sizeof png) && (pic = border_own_surface(png, w, h)) &&
		    border_write_dtov(pic, file)) {
			l.border = P8FX_BORDER_PICTURE;
			snprintf(l.border_file, sizeof l.border_file, "%s", file);
		} else {
			fprintf(stderr, "p8fx: %s: cannot use %s; no border\n", s->name, png[0] ? png : "(no picture)");
		}
		if (pic) SDL_FreeSurface(pic);
	}
	ok = p8fx_look_write(path, &l);
	fprintf(stderr, "p8fx: %s: shader %s at %d%%, border %s%s\n", s->name, l.shader, l.strength,
	        p8fx_border_word(l.border), ok ? "" : ", not written");
	return ok;
}
