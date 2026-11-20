/* SPDX-License-Identifier: GPL-3.0-only */
/* See p8draw.h: your PICO-8's screen on the panel, mode by mode */
#include "p8draw.h"

#include <string.h>

const char *p8_draw_mode(const char *display, bool neg_ok)
{
	static const char *const KNOWN[] = { "stretch", "aspect", "square", "integer", "integer-vertical",
	                                     "overscale", "fill", "native" };
	size_t i;

	for (i = 0; display && i < sizeof KNOWN / sizeof KNOWN[0] && strcmp(display, KNOWN[i]); i++) ;
	if (!display || i == sizeof KNOWN / sizeof KNOWN[0]) return "aspect";
	if (!neg_ok && !strcmp(display, "overscale")) return "integer";
	if (!neg_ok && !strcmp(display, "fill")) return "aspect";
	return display;
}

bool p8_draw_rect(const char *display, int w, int h, bool neg_ok, dmode_rect *r)
{
	/* PICO-8's pixels are square: no shape of its own (aspect 0) */
	*r = dmode_fit(p8_draw_mode(display, neg_ok), P8_SCREEN, P8_SCREEN, 0.0, w, h);
	return r->w > 0 && r->h > 0 && r->w % P8_SCREEN == 0 && r->h % P8_SCREEN == 0;
}
