/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_MP4_VIDTRY_H
#define ZYPEROS_MP4_VIDTRY_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../mp4frame.h"

/* WHETHER A VIDEO IS TOO BIG FOR THIS CPU (mpf_head.trouble's MPF_TOO_BIG,
 * which MP4 says in words; 2026-11-19: "If a size cannot keep up, MP4 must
 * say so plainly"). src/mp4d/vid.c notes every picture it shows or drops
 * while playing, and this judges them in stretches of VIDTRY_STRETCH:
 *
 *   bad   more than a quarter of a stretch's pictures dropped
 *   good  fewer than a tenth dropped
 *
 * Two bad stretches running say it is too big; two good ones running take
 * it back; one in between breaks either run. The first stretch after a
 * restart (an open, a seek, a pause, the screen coming back) is not judged:
 * the decoder fills its threads then and drops as it catches up, even on a
 * file it carries. Measured on the Pixel 2 with dav1d, 2026-11-19:
 *
 *   1280x720  60 fps   77 dropped in the first 2 s, none after: fine
 *   1920x1080 30 fps   37 dropped in the first 2 s, then 0 to 4 of 30: fine
 *   1920x1080 60 fps   4 a second shown, 53 to 91 dropped: too big, said 6 s
 *                      after it starts
 *
 * Pure (the caller's clock), for tools/vidtry-check.c. */

#define VIDTRY_STRETCH 2.0

typedef struct {
	double since;        /* the stretch's start; 0 for none yet */
	long   shown, dropped;
	int    bad, good;    /* stretches of each running */
	bool   warm;         /* this stretch is the first after a restart */
} vidtry;

/* A file opened: nothing known against it */
static inline void vidtry_reset(vidtry *t) { memset(t, 0, sizeof *t); }

/* A stretch from the next picture on, not judged: after a seek, a pause, the
 * screen coming back. The runs of bad and good stretches carry on. */
static inline void vidtry_restart(vidtry *t)
{
	t->since = 0;
	t->shown = t->dropped = 0;
}

/* A picture shown or dropped at `now` (seconds), with `trouble` what is said
 * now: what is to be said after it, MPF_TOO_BIG or MPF_FINE, or `trouble`
 * itself while a stretch runs (MPF_NO_PICTURE is never this one's to
 * change, and no picture is noted under it anyway) */
static inline uint32_t vidtry_note(vidtry *t, double now, bool shown, uint32_t trouble)
{
	long all;

	if (t->since == 0) {
		t->since = now;
		t->warm = true;
	}
	if (shown) t->shown++;
	else       t->dropped++;
	if (now - t->since < VIDTRY_STRETCH) return trouble;
	all = t->shown + t->dropped;
	if (t->warm) {
		/* the start, as the decoder catches up: not judged */
	} else if (t->dropped * 4 > all) {
		t->good = 0;
		if (++t->bad >= 2 && trouble == MPF_FINE) trouble = MPF_TOO_BIG;
	} else if (t->dropped * 10 < all) {
		t->bad = 0;
		if (++t->good >= 2 && trouble == MPF_TOO_BIG) trouble = MPF_FINE;
	} else {
		t->bad = t->good = 0;
	}
	t->warm = false;
	t->since = now;
	t->shown = t->dropped = 0;
	return trouble;
}

#endif
