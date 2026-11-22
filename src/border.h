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
 *   Handheld   the handheld's own face around the game (src/handframe.h),
 *              ZyperOS's drawing at the game's scale: the game at its full
 *              Sharp Pixels size, the frame cut where the panel ends
 *   Handheld, Whole
 *              the same one whole step smaller, so the whole lens and some
 *              of the body show
 *              Both only for a system with a frame drawn (the three Game
 *              Boys first) and while the display mode is Sharp Pixels: the
 *              frame is drawn at a whole step, as the mode draws the game
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
	BORDER_HANDHELD,
	BORDER_WHOLE,
	BORDER_KINDS
} border_kind;

/* The word the database keeps ("off", "blur", "own", "handheld", "whole") */
const char *border_id(border_kind k);

/* The kind for a word; Off for one that is none (NULL included) */
border_kind border_parse(const char *word);

/* The row's value for a kind, and the note its list says under it */
str_id border_label_id(border_kind k);
str_id border_note_id(border_kind k);

/* The database key for the system tagged `tag`: "border.GB" */
void border_key(const char *tag, char *out, size_t n);

/* What a system's row can offer besides Off */
typedef struct {
	bool blur;      /* the device draws Blur (border_blur_here) and the mode leaves room */
	bool handheld;  /* a frame is drawn for the system and fits the panel, in any mode */
	bool whole;     /* ...and Handheld, Whole has a placement here (2x or more) */
	bool own;       /* the card has a picture for it and the mode leaves room */
} border_offer;

/* What Border offers: Blur and Your Own only where the system's display mode
 * leaves room around its games (`room`), since in a mode that covers the
 * screen they would change nothing; a handheld's frames (`frame`, `whole`:
 * the system has a frame that fits the panel, and its Whole placement) in
 * every mode, Full Screen, the one a system starts in, too, since choosing
 * a frame turns Display to Sharp Pixels, the size the frame is drawn for */
border_offer border_offer_make(bool room, bool blur_here, bool own, bool frame, bool whole);

/* Whether the Border row shows: where the mode leaves room, or a frame is
 * offered */
bool border_row_shows(border_offer o, bool room);

/* Whether `k` is one of the handheld frames */
bool border_is_frame(border_kind k);

/* The kind a kept `k` reads as at display mode `mode` (a DMODES name): a
 * frame only at Sharp Pixels, Off at any other, and back with Sharp Pixels;
 * the rest as they are */
border_kind border_in_mode(border_kind k, const char *mode);

/* The kinds this system's row offers, in the row's order, into `out` (up to
 * BORDER_KINDS): Off, Blur, Handheld, Handheld Whole, Your Own, each where
 * `o` says. Returns how many. */
int border_offered(border_offer o, border_kind *out);

/* The kind kept, as the row can show it: one the row does not offer now
 * (Your Own with its picture gone, Blur on a device without it, a Handheld
 * frame at a mode that is not Sharp Pixels) reads Off, and is drawn as Off */
border_kind border_shown(border_kind kept, border_offer o);

/* The kind after or before `k` among the offered, round: d is +1 or -1 */
border_kind border_step(border_kind k, int d, border_offer o);

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
