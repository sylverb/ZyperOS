/* SPDX-License-Identifier: GPL-3.0-only */
/* See hudmenu.h */
#include "hudmenu.h"

#include <stdio.h>
#include <string.h>

const char *const hm_edge_id[HUD_EDGES] = { "close", "normal", "far" };
const char *const hm_opacity_id[HUD_OPACITIES] = { "100", "75", "50" };

static const char *const ON_ID[2] = { "1", "0" };

const char *hm_key(hm_row r)
{
	switch (r) {
	case HM_CORNER:     return HM_KEY_CORNER;
	case HM_EDGE:       return HM_KEY_EDGE;
	case HM_BACKGROUND: return HM_KEY_BACKGROUND;
	case HM_OPACITY:    return HM_KEY_OPACITY;
	case HM_COLOR:      return HM_KEY_COLOR;
	case HM_SIZE:       return HM_KEY_SIZE;
	default:             return NULL;
	}
}

const char *const *hm_ids(hm_row r, int *n)
{
	switch (r) {
	case HM_ON:         *n = 2; return ON_ID;
	case HM_CORNER:     *n = HUD_CORNERS; return hud_corner_id;
	case HM_EDGE:       *n = HUD_EDGES; return hm_edge_id;
	case HM_BACKGROUND: *n = HUD_BACKGROUNDS; return hud_background_id;
	case HM_OPACITY:    *n = HUD_OPACITIES; return hm_opacity_id;
	case HM_COLOR:      *n = HUD_COLORS; return hud_color_id;
	case HM_SIZE:       *n = HUD_SIZES; return hud_size_id;
	default:             *n = 0; return NULL;
	}
}

const char *hm_item_label(int item)
{
	static const str_id L[HUD_ITEMS] = {
		S_GAMEMENU_HUD_FPS, S_GAMEMENU_HUD_SPEED, S_GAMEMENU_HUD_TEMP, S_GAMEMENU_HUD_CPU,
		S_GAMEMENU_HUD_CLOCK, S_GAMEMENU_HUD_BATTERY, S_GAMEMENU_HUD_TIME,
	};
	return item >= 0 && item < HUD_ITEMS ? T(L[item]) : "";
}

void hm_shows_value(uint8_t items, char *out, size_t n)
{
	int on = 0, first = -1, second = -1, i;

	for (i = 0; i < HUD_ITEMS; i++)
		if (items & (1u << i)) {
			if (first < 0) first = i;
			else if (second < 0) second = i;
			on++;
		}
	if (on == 1) snprintf(out, n, "%s", hm_item_label(first));
	else if (on == 2) snprintf(out, n, "%s, %s", hm_item_label(first), hm_item_label(second));
	else snprintf(out, n, TN(S_GAMEMENU_HUD_ITEMS, on), on);
}

uint8_t hm_item_flip(uint8_t items, int item)
{
	uint8_t to;

	if (item < 0 || item >= HUD_ITEMS) return items;
	to = (uint8_t)(items ^ (1u << item));
	return to ? to : items;
}

const char *hm_label(hm_row r)
{
	static const str_id L[HM_ROWS] = {
		S_GAMEMENU_HUD, S_GAMEMENU_HUD_SHOWS, S_GAMEMENU_HUD_CORNER, S_GAMEMENU_HUD_EDGE, S_GAMEMENU_HUD_BACKGROUND,
		S_GAMEMENU_HUD_OPACITY, S_GAMEMENU_HUD_COLOR, S_GAMEMENU_HUD_SIZE,
	};
	return r >= 0 && r < HM_ROWS ? T(L[r]) : "";
}

int hm_count(hm_row r)
{
	int n;

	hm_ids(r, &n);
	return n;
}

const char *hm_value(hm_row r, int k)
{
	static const str_id ON[2] = { S_MENU_ON_TITLE, S_MENU_OFF_TITLE };
	static const str_id CORNER[HUD_CORNERS] = {
		S_GAMEMENU_HUD_TOP_LEFT, S_GAMEMENU_HUD_TOP_RIGHT,
		S_GAMEMENU_HUD_BOTTOM_LEFT, S_GAMEMENU_HUD_BOTTOM_RIGHT,
	};
	static const str_id EDGE[HUD_EDGES] = {
		S_GAMEMENU_HUD_CLOSE, S_GAMEMENU_HUD_NORMAL, S_GAMEMENU_HUD_FAR,
	};
	static const str_id BG[HUD_BACKGROUNDS] = {
		S_GAMEMENU_HUD_PILL, S_GAMEMENU_HUD_SHADOW,
	};
	static const str_id COLOR[HUD_COLORS] = {
		S_GAMEMENU_HUD_THEME, S_GAMEMENU_HUD_WHITE, S_LOOK_ACCENT_GREEN, S_LOOK_ACCENT_YELLOW,
	};
	static const str_id SIZE[HUD_SIZES] = { S_GAMEMENU_HUD_NORMAL, S_GAMEMENU_HUD_SMALL };
	static char pct[8];

	if (k < 0 || k >= hm_count(r)) k = 0;
	switch (r) {
	case HM_ON:         return T(ON[k]);
	case HM_CORNER:     return T(CORNER[k]);
	case HM_EDGE:       return T(EDGE[k]);
	case HM_BACKGROUND: return T(BG[k]);
	case HM_OPACITY:
		snprintf(pct, sizeof pct, "%d%%", hud_opacity_pct[k]);
		return pct;
	case HM_COLOR:      return T(COLOR[k]);
	case HM_SIZE:       return T(SIZE[k]);
	default:             return "";
	}
}

int hm_get(const hm_state *s, hm_row r)
{
	switch (r) {
	case HM_ON:         return s->on ? 0 : 1;
	case HM_CORNER:     return s->st.corner;
	case HM_EDGE:       return s->st.edge;
	case HM_BACKGROUND: return s->st.background;
	case HM_OPACITY:    return s->st.opacity;
	case HM_COLOR:      return s->st.color;
	case HM_SIZE:       return s->st.size;
	default:             return 0;
	}
}

void hm_set(hm_state *s, hm_row r, int k)
{
	if (k < 0 || k >= hm_count(r)) return;
	switch (r) {
	case HM_ON:         s->on = k == 0; break;
	case HM_CORNER:     s->st.corner = (uint8_t)k; break;
	case HM_EDGE:       s->st.edge = (uint8_t)k; break;
	case HM_BACKGROUND: s->st.background = (uint8_t)k; break;
	case HM_OPACITY:    s->st.opacity = (uint8_t)k; break;
	case HM_COLOR:      s->st.color = (uint8_t)k; break;
	case HM_SIZE:       s->st.size = (uint8_t)k; break;
	default:             break;
	}
}

int hm_parse(hm_row r, const char *word)
{
	int n;
	const char *const *ids = hm_ids(r, &n);

	return ids ? hud_find(ids, n, word) : -1;
}

void hm_line(const hm_state *s, bool clock24, char *out, size_t n)
{
	hud_style st = s->st;
	char items[64];

	hud_clamp(&st);
	hud_items_str(st.items, items, sizeof items);
	snprintf(out, n, "SETHUD\ton=%d\titems=%s\tcorner=%s\tedge=%s\tbg=%s\topacity=%s\tcolor=%s\tsize=%s\tclock24=%d",
	         s->on ? 1 : 0, items, hud_corner_id[st.corner], hm_edge_id[st.edge],
	         hud_background_id[st.background], hm_opacity_id[st.opacity],
	         hud_color_id[st.color], hud_size_id[st.size], clock24 ? 1 : 0);
}
