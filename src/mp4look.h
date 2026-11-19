/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_MP4LOOK_H
#define ZYPEROS_MP4LOOK_H

/* MP4's Player Looks (DESIGN.md 11.14, my picks in the Sound lab,
 * 2026-11-19): how Now Playing draws a song. Cover, the album's cover
 * large, is the default and was the only one; the others are drawn by
 * src/nowplaying_looks.c from what this holds, which is the part with no
 * SDL in it, so tools/mp4look-check.c can hold the motion to its numbers.
 *
 * The bars are the sound (src/muse/bands.h): 32 bands from the daemon, 30
 * times a second, each bar the loudest of the bands under it. A bar rises at
 * once and falls on **follow** (tau DS_MS_BAR_FALL); its cap holds where the
 * bar last reached for **peak-hold**, then drops with gravity, from the top
 * to the floor in MP4L_CAP_DROP_MS. Paused, the bars fall to rest, and once
 * every bar and cap is down nothing moves and nothing is drawn. */

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	MP4L_COVER,
	MP4L_CLASSIC,
	MP4L_SPECTRUM,
	MP4L_COUNT
} mp4l_id;

/* The setting's word for a look ("cover", "classic", ...), kept as a word
 * so the list's order can change without a card's choice moving; and back,
 * Cover for one it does not know */
const char *mp4l_key(mp4l_id id);
mp4l_id     mp4l_from_key(const char *key);

/* How many bars a look draws, 0 for none (Cover) */
int  mp4l_bars(mp4l_id id);

/* ---- the motion ------------------------------------------------------------ */

#define MP4L_BANDS    32
#define MP4L_BARS_MAX 32
/* The cap's fall, from the top to the floor, after its hold */
#define MP4L_CAP_DROP_MS 667

typedef struct {
	int   n;                       /* bars, mp4l_bars of the look */
	float lv[MP4L_BARS_MAX];       /* each bar's height, 0 to 1 */
	float cap[MP4L_BARS_MAX];      /* its cap's */
	float hold[MP4L_BARS_MAX];     /* ms the cap still holds */
	float fall[MP4L_BARS_MAX];     /* the cap's speed down, per ms */
} mp4l_bars_t;

/* `n` bars, every one at rest */
void mp4l_bars_reset(mp4l_bars_t *b, int n);

/* One frame, `dt` ms after the last. `bands` the daemon's 32 levels (0-255)
 * while they are fresh, NULL when there are none to follow (paused, or
 * nothing came): every bar then falls to rest. */
void mp4l_bars_step(mp4l_bars_t *b, float dt, const uint8_t *bands);

/* Every bar and every cap down: nothing to draw again */
bool mp4l_bars_rest(const mp4l_bars_t *b);

#endif
