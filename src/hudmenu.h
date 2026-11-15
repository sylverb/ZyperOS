/* SPDX-License-Identifier: GPL-3.0-only */
/* The On-Screen Stats page: what A on the in-game menu's On-Screen Stats
 * row opens (its value, On or Off, stays on the row, and Left and Right
 * there turn it over). Its rows, each an option row (DESIGN.md 9) but Shows:
 *
 *   On-Screen Stats  On, Off
 *   Shows            the items on ("FPS, Speed", "3 Items"); A opens their
 *                    list, each On or Off: FPS, Speed, CPU Temperature,
 *                    CPU Load, CPU Clock, Battery, Time
 *   Corner           Top Left, Top Right, Bottom Left, Bottom Right
 *   Edge Distance    Close, Normal, Far          (8, 24, 48 units)
 *   Background       Pill, Shadow
 *   Opacity          100%, 75%, 50%
 *   Text Color       Theme, White, Green, Yellow
 *   Size             Normal, Small
 *
 * under a band at the page's top that is the screen's strip where the line
 * goes, at its own size, with the line in it as it will be
 * (src/hudmenu_screen.c). The items' list has the band too.
 *
 * Kept per player, for every game, in the device's database: HUD_KEY and a
 * key a part, each a word (hud_*_id, the edge's and the opacity's here; the
 * items "fps,speed"), which `--set` takes as they are. Sent to Diatom whole
 * as SETHUD (hm_line). Pure: no SDL, no database, so tools/hudmenu-check.c
 * reaches all of it. */
#ifndef ZYPEROS_HUDMENU_H
#define ZYPEROS_HUDMENU_H

#include <stdbool.h>
#include <stddef.h>

#include "hud.h"
#include "i18n.h"

typedef enum {
	HM_ON, HM_SHOWS, HM_CORNER, HM_EDGE, HM_BACKGROUND, HM_OPACITY, HM_COLOR, HM_SIZE,
	HM_ROWS
} hm_row;

/* The database's keys: the switch (game_menu.h's HUD_KEY) and the
 * style's parts */
#define HM_KEY_ITEMS      "hud_items"
#define HM_KEY_CORNER     "hud_corner"
#define HM_KEY_EDGE       "hud_edge"
#define HM_KEY_BACKGROUND "hud_background"
#define HM_KEY_OPACITY    "hud_opacity"
#define HM_KEY_COLOR      "hud_color"
#define HM_KEY_SIZE       "hud_size"

/* The edge's words and the opacity's, as kept */
extern const char *const hm_edge_id[HUD_EDGES];        /* close normal far */
extern const char *const hm_opacity_id[HUD_OPACITIES]; /* 100 75 50 */

typedef struct {
	bool on;
	hud_style st;
} hm_state;

/* The database's key for a row of the style (NULL for HM_ON, which is
 * HUD_KEY, for HM_SHOWS, which is HM_KEY_ITEMS, and past the rows), its
 * words and how many */
const char *hm_key(hm_row r);
const char *const *hm_ids(hm_row r, int *n);

/* A row's name, its choices' count and each choice's name, in the
 * language that is on. Shows has no choices of its own (0): its value is
 * hm_shows_value. */
const char *hm_label(hm_row r);
int hm_count(hm_row r);
const char *hm_value(hm_row r, int k);

/* An item's name (HUD_FPS ...), and the Shows row's value for these items:
 * one or two named, "FPS, Speed", or how many */
const char *hm_item_label(int item);
void hm_shows_value(uint8_t items, char *out, size_t n);
/* The items with `item` turned over; the last one on stays on, since a
 * line of nothing is On-Screen Stats off */
uint8_t hm_item_flip(uint8_t items, int item);

/* Which choice a row is on, and the state with row `r` on choice `k`
 * (kept in range: past the ends, the one it was on) */
int  hm_get(const hm_state *s, hm_row r);
void hm_set(hm_state *s, hm_row r, int k);

/* A kept word's choice for row `r`: -1 for one it does not know, which
 * leaves the default's */
int hm_parse(hm_row r, const char *word);

/* What Diatom is told: "SETHUD\ton=1\titems=fps,speed\tcorner=tl\t
 * edge=normal\tbg=pill\topacity=100\tcolor=theme\tsize=normal\tclock24=1",
 * the words as kept, and the clock MENU > Clock says */
void hm_line(const hm_state *s, bool clock24, char *out, size_t n);

#endif
