/* SPDX-License-Identifier: GPL-3.0-only */
/* Border: what fills the screen around a game where its display mode leaves
 * room (DESIGN.md 11.59). A row of a game's Display tab, under Display, and
 * of a system's menu, under Display, kept per system (border.TAG). It shows
 * only while the mode leaves room: the GKD Pixel 2's panel is 4:3, so a 4:3
 * game at Full Screen or Sharp Pixels at 2x fills it, and the room is around
 * the handhelds (the Game Boys, the Game Gear, the Neo Geo Pocket and the
 * rest) and a Sharp Pixels picture smaller than the panel.
 *
 *   Off        black, as always: the default
 *   Blur       the game's own picture, small, blurred and dimmed, made again
 *              a few times a second (Diatom, diatom/src/border.h). Only where
 *              the port draws it smoothly (the Pixel 2; the Brick when its
 *              GPU path draws every frame)
 *   Your Own   a picture on the card: the first PNG, by name, in
 *              Borders/<the system's folder>/, else in NextUI's
 *              Overlays/<its tag>/, so a NextUI card's overlays work as they
 *              are. Shown only when there is one.
 *
 * Drawn once, under the game and under the volume bar, On-Screen Stats and
 * the low battery's dot, which stay on top: the launcher makes the picture
 * the panel's size once (src/border_pic.h) and Diatom uploads it once.
 *
 * Plain C: tools/border-check.c holds the choices, their words, stepping and
 * the room rule. */
#ifndef ZYPEROS_BORDER_H
#define ZYPEROS_BORDER_H

#include <stdbool.h>
#include <stddef.h>

#include "i18n_ids.h"

#define BORDER_SETTING_PREFIX "border."

typedef enum {
	BORDER_OFF,
	BORDER_BLUR,
	BORDER_OWN,
	BORDER_KINDS
} border_kind;

/* The word the database keeps ("off", "blur", "own") */
const char *border_id(border_kind k);

/* The kind for a word; Off for one that is none (NULL included) */
border_kind border_parse(const char *word);

/* The row's value for a kind, and the note its list says under it */
str_id border_label_id(border_kind k);
str_id border_note_id(border_kind k);

/* The database key for the system tagged `tag`: "border.GB" */
void border_key(const char *tag, char *out, size_t n);

/* The kinds this system's row offers, in the row's order, into `out` (up to
 * BORDER_KINDS): Off, then Blur where the device draws it (`blur`), then
 * Your Own where the card has a picture for it (`own`). Returns how many. */
int border_offered(bool blur, bool own, border_kind *out);

/* The kind kept, as the row can show it: one the row does not offer now
 * (Your Own with its picture gone, Blur on a device without it) reads Off,
 * and is drawn as Off */
border_kind border_shown(border_kind kept, bool blur, bool own);

/* The kind after or before `k` among the offered, round: d is +1 or -1 */
border_kind border_step(border_kind k, int d, bool blur, bool own);

/* Whether display mode `mode` (a DMODES name) leaves room around a picture
 * `src_w` x `src_h` with the shape `aspect` (0 for square pixels) on a
 * surface `surf_w` x `surf_h`: its rect smaller than the surface either way.
 * An unknown picture (0 x 0, a system never played): room unless the mode
 * always covers the surface (Full Screen, Fill, Sharp Pixels, Zoomed). */
bool border_room(const char *mode, int src_w, int src_h, double aspect, int surf_w, int surf_h);

/* NextUI's tag for a ZyperOS system's, where the two differ (its Overlays
 * folders are by its own tags: FC for the NES, PS for the PlayStation; the
 * SNES is SFC in both), else NULL */
const char *border_nextui_tag(const char *tag);

#endif
