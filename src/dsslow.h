/* SPDX-License-Identifier: GPL-3.0-only */
/* When Slow: what a Nintendo DS game's frames do when the game cannot keep
 * up, a row of the Nintendo DS shelf's menu, under Game Language.
 *
 * A DS game on DSperate is sent with frameskip=late (src/config.c,
 * cfg_run_keys): a frame that starts late is run with its drawing left out,
 * two in a row at most, so the game keeps its speed and its sound. Where the
 * game is slow for a stretch, the frames then reach the panel 16.7 and 33.3
 * ms apart in turn: Mario Kart DS in a race on the Pixel 2 at a median 41 a
 * second, uneven (2026-11-16). I find that fine, and wanted the other way
 * as a choice:
 *
 *   Keep Speed  frameskip=late, the default and what it always was
 *   Steady 30   frameskip=steady: while frames start late, exactly every
 *               second one is drawn, an even 30 a second, and every one again
 *               once frames have started on time for half a second (Diatom's
 *               frame loop, STEADY_HOLD)
 *
 * The game's speed is the same either way on DSperate: a frame left undrawn
 * costs its emulation thread as much as a drawn one, the drawing being on
 * threads of their own. Only the frames the panel shows differ.
 *
 * One setting for the handheld (ds_when_slow in the device's database), as
 * Game Language is: only the DS's games leave late frames out, so the row is
 * only on the DS shelf, and only while DSperate runs it.
 *
 * Plain C: tools/dsslow-check.c holds the choices, their words and stepping. */
#ifndef ZYPEROS_DSSLOW_H
#define ZYPEROS_DSSLOW_H

#include "i18n_ids.h"

/* The setting's key in the device's database */
#define DSSLOW_SETTING "ds_when_slow"

typedef enum {
	DSS_KEEP,      /* Keep Speed: frameskip=late */
	DSS_STEADY,    /* Steady 30: frameskip=steady */
	DSS_CHOICES
} dss_choice;

/* The word the database keeps ("keep", "steady") and the choice for a word;
 * a word that is none, NULL included, is Keep Speed */
const char *dsslow_id(dss_choice c);
dss_choice dsslow_parse(const char *word);

/* The choice after or before `c`, round the two: d is +1 or -1 */
dss_choice dsslow_step(dss_choice c, int d);

/* What RUN's frameskip= says for a choice: "late" or "steady" */
const char *dsslow_frameskip(dss_choice c);

/* The row's value for a choice. Two choices: A and Left/Right flip the row,
 * with no list, so no line saying what each does; the guide does. */
str_id dsslow_label_id(dss_choice c);

#endif
