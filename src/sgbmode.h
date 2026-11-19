/* SPDX-License-Identifier: GPL-3.0-only */
/* Super Game Boy, a row of the Game Boy and Game Boy Color shelves' menus,
 * under Core, on mGBA (the shelf's core; Gambatte has no Super Game Boy).
 *
 *   Off          the default, as before: a Game Boy game in the shelf's
 *                own colors, with no border
 *   Its Games    a game made for the Super Game Boy starts as one: its own
 *                colors and its own border, which the game sends itself, so
 *                no BIOS file is needed. Other games as with Off.
 *   Every Game   every game starts as one, in its frame; a Game Boy Color
 *                only cartridge cannot run on one and starts as with Off
 *
 * The picture is 256x224 with the border: exactly 2x on the GKD Pixel 2's
 * 640x480 (512x448) and 3x on the TrimUI Brick's 1024x768 (768x672) at Sharp
 * Pixels. The choice is the system's own, kept in the device's database as
 * sgb.TAG ("sgb.GB"), and sent with RUN as sgb=games or sgb=every; Diatom
 * reads the game's header and sets mGBA's model and border for that game
 * alone (diatom/src/sgb.h). Off sends nothing, so a player who never turns
 * it on starts every game as before.
 *
 * Plain C: tools/sgbmode-check.c holds the choices, their words and
 * stepping. */
#ifndef ZYPEROS_SGBMODE_H
#define ZYPEROS_SGBMODE_H

#include <stdbool.h>
#include <stddef.h>

#include "i18n_ids.h"

/* The setting's key in the device's database is this and the system's tag */
#define SGBM_SETTING_PREFIX "sgb."

typedef enum {
	SGBM_OFF,      /* nothing sent */
	SGBM_GAMES,    /* sgb=games */
	SGBM_EVERY,    /* sgb=every */
	SGBM_CHOICES
} sgbm_choice;

/* The word the database keeps and RUN's sgb= sends ("off", "games",
 * "every"); RUN sends nothing for off */
const char *sgbm_id(sgbm_choice c);

/* The choice for a word; Off for one that is none (NULL included) */
sgbm_choice sgbm_parse(const char *word);

/* The choice after or before `c`, round the three: d is +1 or -1 */
sgbm_choice sgbm_step(sgbm_choice c, int d);

/* The row's value for a choice, and the note its list says under it */
str_id sgbm_label_id(sgbm_choice c);
str_id sgbm_note_id(sgbm_choice c);

/* The database key for the system tagged `tag`: "sgb.GB" */
void sgbm_key(const char *tag, char *out, size_t n);

/* Whether a shelf has the row: the Game Boy's or the Game Boy Color's (tag
 * GB or GBC), on mGBA */
bool sgbm_shelf(const char *tag, const char *core);

#endif
