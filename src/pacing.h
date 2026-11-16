/* SPDX-License-Identifier: GPL-3.0-only */
/* Timing: how a system's games are paced against the panel, a row of the
 * system's menu, under Core (and When Slow on the Nintendo DS shelf).
 *
 * The GKD Pixel 2's panel refreshes 60.08 times a second, and a GBA makes
 * 59.73 frames a second, a DS 59.83 and a Genesis 59.92. Two ways to show
 * them, each a fair choice:
 *
 *   Smooth  one game frame each refresh (Diatom's display lock, RUN
 *           pace=smooth, diatom/src/pace.h): every frame shown and evenly,
 *           as RetroArch's vsync shows them, the game up to 0.59% fast and
 *           its sound resampled to match
 *   Exact   the game at its own rate (pace=exact, and what every system
 *           without the row gets): its speed is the console's, and about
 *           one refresh in 170 on a GBA shows the frame before again
 *
 * A player who timed Pokemon Pinball against a GBA micro found our mGBA
 * under 0.5 s slow over 10 minutes (Exact), where another firmware's
 * Smooth-paced core was 2.5 s fast (2026-11-16). Smooth would be 3.5 s fast
 * here. Whoever runs a timer or plays to the console's beat wants Exact; for
 * everyone else Smooth is the even picture.
 *
 * A system has the row when config/systems.cfg gives it a pace line, whose
 * word is the default (Smooth for the GBA, GB, GBC, DS and Genesis). The
 * choice is the system's own, kept in the device's database as pace.TAG
 * ("pace.GBA"), as each system's shader is its own.
 *
 * Plain C: tools/pacing-check.c holds the choices, their words and stepping. */
#ifndef ZYPEROS_PACING_H
#define ZYPEROS_PACING_H

#include <stddef.h>

#include "i18n_ids.h"

/* The setting's key in the device's database is this and the system's tag */
#define PACING_SETTING_PREFIX "pace."

typedef enum {
	PACING_SMOOTH,    /* one frame a refresh: pace=smooth */
	PACING_EXACT,     /* the game's own rate: pace=exact */
	PACING_CHOICES
} pacing_choice;

/* The word the database keeps and RUN's pace= sends ("smooth", "exact") */
const char *pacing_id(pacing_choice c);

/* The choice for a word, `fallback` for one that is none (NULL included) */
pacing_choice pacing_parse(const char *word, pacing_choice fallback);

/* The choice after or before `c`, round the two: d is +1 or -1 */
pacing_choice pacing_step(pacing_choice c, int d);

/* The row's value for a choice. Two choices: A and Left/Right flip the row */
str_id pacing_label_id(pacing_choice c);

/* The database key for the system tagged `tag`: "pace.GBA" */
void pacing_key(const char *tag, char *out, size_t n);

#endif
