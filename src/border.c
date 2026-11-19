/* SPDX-License-Identifier: GPL-3.0-only */
/* Border, a display row. See src/border.h. */
#include "border.h"

#include <stdio.h>
#include <string.h>

#include "dmodefit.h"

static const char *const IDS[BORDER_KINDS] = {
	[BORDER_OFF]  = "off",
	[BORDER_BLUR] = "blur",
	[BORDER_OWN]  = "own",
	[BORDER_HANDHELD] = "handheld",
	[BORDER_WHOLE] = "whole",
};

const char *border_id(border_kind k)
{
	return k >= 0 && k < BORDER_KINDS ? IDS[k] : IDS[BORDER_OFF];
}

border_kind border_parse(const char *word)
{
	int k;

	if (!word) return BORDER_OFF;
	for (k = 0; k < BORDER_KINDS; k++)
		if (!strcmp(word, IDS[k])) return (border_kind)k;
	return BORDER_OFF;
}

str_id border_label_id(border_kind k)
{
	return k == BORDER_BLUR ? S_GAMEMENU_BORDER_BLUR : k == BORDER_OWN ? S_GAMEMENU_BORDER_OWN
	     : k == BORDER_HANDHELD ? S_GAMEMENU_BORDER_HANDHELD : k == BORDER_WHOLE ? S_GAMEMENU_BORDER_WHOLE
	     : S_MENU_OFF_TITLE;
}

str_id border_note_id(border_kind k)
{
	return k == BORDER_BLUR ? S_GAMEMENU_BORDER_BLUR_NOTE : k == BORDER_OWN ? S_GAMEMENU_BORDER_OWN_NOTE
	     : k == BORDER_HANDHELD ? S_GAMEMENU_BORDER_HANDHELD_NOTE
	     : k == BORDER_WHOLE ? S_GAMEMENU_BORDER_WHOLE_NOTE : S_GAMEMENU_BORDER_OFF_NOTE;
}

void border_key(const char *tag, char *out, size_t n)
{
	snprintf(out, n, "%s%s", BORDER_SETTING_PREFIX, tag ? tag : "");
}

int border_offered(border_offer o, border_kind *out)
{
	int n = 0;

	out[n++] = BORDER_OFF;
	if (o.blur) out[n++] = BORDER_BLUR;
	if (o.handheld) out[n++] = BORDER_HANDHELD;
	if (o.handheld && o.whole) out[n++] = BORDER_WHOLE;
	if (o.own) out[n++] = BORDER_OWN;
	return n;
}

border_kind border_shown(border_kind kept, border_offer o)
{
	if (kept == BORDER_BLUR && !o.blur) return BORDER_OFF;
	if (kept == BORDER_OWN && !o.own) return BORDER_OFF;
	if ((kept == BORDER_HANDHELD || kept == BORDER_WHOLE) && !o.handheld) return BORDER_OFF;
	if (kept == BORDER_WHOLE && !o.whole) return BORDER_OFF;
	return kept >= 0 && kept < BORDER_KINDS ? kept : BORDER_OFF;
}

border_kind border_step(border_kind k, int d, border_offer o)
{
	border_kind list[BORDER_KINDS];
	int n = border_offered(o, list), i, at = 0;

	k = border_shown(k, o);
	for (i = 0; i < n; i++)
		if (list[i] == k) at = i;
	return list[((at + (d < 0 ? -1 : 1)) % n + n) % n];
}

bool border_room(const char *mode, int src_w, int src_h, double aspect, int surf_w, int surf_h)
{
	dmode_rect r;

	if (!mode || surf_w <= 0 || surf_h <= 0) return false;
	if (src_w <= 0 || src_h <= 0)
		return strcmp(mode, "stretch") != 0 && strcmp(mode, "fill") != 0;
	r = dmode_fit(mode, src_w, src_h, aspect, surf_w, surf_h);
	/* A rect reaching past the surface covers it that way (fill, overscale) */
	return r.x > 0 || r.y > 0 || r.x + r.w < surf_w || r.y + r.h < surf_h;
}

const char *border_nextui_tag(const char *tag)
{
	static const char *const MAP[][2] = { { "NES", "FC" }, { "PS1", "PS" } };
	size_t i;

	for (i = 0; tag && i < sizeof MAP / sizeof MAP[0]; i++)
		if (!strcmp(tag, MAP[i][0])) return MAP[i][1];
	return NULL;
}
