/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_NOWPLAYING_LOOKS_H
#define ZYPEROS_NOWPLAYING_LOOKS_H

/* MP4's Player Looks drawn (DESIGN.md 11.64): Now Playing as the look MENU >
 * Player Look has on, for music. The looks and their motion are
 * src/mp4look.h's; Cover, the default, is src/nowplaying_screen.c's np_draw
 * as it always was. */
#include "app.h"
#include "keyhints.h"
#include "mp4look.h"
#include "mp4c.h"

#include <stdbool.h>

/* The look that is on (the setting mp4.look, Cover until it is changed),
 * and setting it */
mp4l_id np_look(void);
void    np_look_set(mp4l_id id);
/* A shot's look, never saved (--np-look); MP4L_COUNT for none */
void    np_look_shot(mp4l_id id);

/* The look for what plays: the one that is on for music, Cover for a book
 * and a video, which have their own screens */
mp4l_id np_look_for(void);

/* Now Playing in `look` (any but Cover). Returns how long until the
 * picture would change by itself, in ms: 0 while it moves, which is always
 * while music plays */
unsigned npl_draw(app *a, mp4l_id look, const mu_now *mn);

/* Under the lock nothing moves (DESIGN.md 11.64): the bars rest and the
 * daemon stops measuring, until it is let go */
void npl_still(bool still);

/* MENU > Player Look's picture of `look` in the box (x, y, w, h): Now
 * Playing in small, playing - the song that plays, or the shelf's album's
 * first played in show, with bars to match */
void npl_preview(app *a, mp4l_id look, int x, int y, int w, int h);

/* For --np-bench N: N frames of the look that is on with a song's bars
 * moving, each frame's drawing timed; the mean, the 95th percentile and the
 * worst said on stderr */
void npl_bench(app *a, const mu_now *mn, int frames);

/* For --shot: bars and caps as a song has them, held still */
void npl_shot_bars(void);
/* The song a shot or the list's show plays, with no queue to say it: its
 * album (a library index) and its place in it; -1 none */
void npl_song(int album, int place);

/* Now Playing's buttons (src/nowplaying_screen.c), the looks' foot too */
int np_hint_list(key_hint *h, bool playing, int kind, int count, bool video);

#endif
