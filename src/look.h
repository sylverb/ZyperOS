/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_LOOK_H
#define ZYPEROS_LOOK_H

/* The player's look (DESIGN.md 3.5): MENU > Display > Theme. My picks for
 * the first version (2026-11-12): the accent, one of DS_ACCENT's ten and the
 * same in both themes; a background tone for each theme, one of DS_TONE's
 * three; corners rounded or square; and Zyper on or off. And since 1.0.6
 * rc.3 the folders' color: each system's own, or one of the accents for
 * every folder (Tarik: "can we customize them in theme to be per system or a
 * custom color for all?"). Each starts as the themes ship, which is
 * LOOK_DEFAULT.
 *
 * From an accent and a tone this works out the whole theme - its surfaces,
 * its text, the accent's ink and the text on the accent - and holds every
 * text to 4.5:1 and every mark to 3:1 on what it is drawn on, as
 * tools/designdef.py holds the tokens (tools/look-check.c tries every
 * combination). The defaults give back the tokens exactly.
 *
 * The look that is on is the tables theme_at() returns (src/theme.h), so
 * every screen that draws from a theme draws from it, worked out once when
 * a setting changes and never per frame. Plain C, no SDL and no database:
 * the caller reads the saved words and hands them over (look_parse), as
 * theme.c's caller does the appearance. */

#include <stdbool.h>

#include "design.h"

typedef struct {
	int  accent;               /* DS_ACCENT's */
	int  tone[DS_THEMES];      /* DS_TONE[theme]'s */
	bool square;               /* Corners: Square */
	bool zyper_off;            /* Zyper: Off */
	int  folder;               /* Folder Color: 0 System Colors, each system's
	                            * own; k DS_ACCENT[k - 1] for every folder */
} look;

/* Folder Color's choices: System Colors, then the accents */
#define LOOK_FOLDERS (DS_ACCENTS + 1)

#define LOOK_DEFAULT ((look){ 0, { 0, 0 }, false, false, 0 })

/* The look that is on as one number, 0 to LOOK_IDS - 1: its accent, both
 * tones, its corners and its folders' color, what a card is painted with
 * (src/ui.c's stamp on a card's texture). Not a count of changes: a list's
 * cursor going through looks and back leaves a card of the kept look
 * current and every other one stale, however many it went through. */
#define LOOK_IDS (DS_ACCENTS * DS_TONES * DS_TONES * 2 * LOOK_FOLDERS)
int look_id(void);

/* The colors as they ship: the first accent and both first tones (corners
 * and Zyper are not colors) */
bool look_is_default(const look *l);

/* The theme `base` (DS_LIGHT or DS_DARK) with `ground` for its ground and
 * `accent` for its accent, everything else worked out from them, into
 * `out`. The base's own ground and accent give its table back unchanged. */
void look_theme(int base, ds_rgb ground, ds_rgb accent, ds_theme *out);

/* WCAG 2's contrast of two colors, 1 to 21 */
float look_ratio(ds_rgb a, ds_rgb b);

/* The look that is on: set, the two themes worked out for theme_at() at
 * once; and read */
void look_set(const look *l);
const look *look_now(void);

/* How many times a look has been set: a cache of something drawn in a
 * theme's colors keeps this beside the theme it was drawn in, and draws
 * again when either changes */
unsigned look_gen(void);

/* Corners and Zyper, as the drawing asks: square corners on, Zyper off */
bool look_square(void);
bool look_zyper_off(void);

/* A folder's color as Folder Color has it: `system_rgb`, its system's own
 * (0xRRGGBB, or CFG_NO_COLOR, which the drawing gives its text-dim), with
 * System Colors on; the one color every folder takes with any other. The
 * shelf's folders of games and the File Transfer page's console folders. */
unsigned look_folder_rgb(unsigned system_rgb);

/* Folder Color's word as the database keeps it (look.folders): "system", or
 * an accent's id (DS_ACCENT_ID); NULL or a word from another release reads
 * as System Colors */
int look_folder_parse(const char *word);
const char *look_folder_word(int folder);

/* The words the database keeps (look.accent, look.light, look.dark: an id
 * of DS_ACCENT_ID or DS_TONE_ID; look.corners: "rounded" or "square";
 * look.zyper: "on" or "off"), any of them NULL or a word from another
 * release reading as it ships */
look look_parse(const char *accent, const char *light, const char *dark,
                const char *corners, const char *zyper);

#endif
