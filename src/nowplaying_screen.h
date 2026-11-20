/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_NOWPLAYING_SCREEN_H
#define ZYPEROS_NOWPLAYING_SCREEN_H

/* MP4's Now Playing (DESIGN.md 11.21): the cover and the words, the
 * lock screen, and the video view. The screens are src/nowplaying_screen.c;
 * the player is src/mp4c.c, the lock's rule src/mp4lock.c. */
#include "app.h"
#include "mp4lock.h"
#include "mp4c.h"
#include "platform.h"

#include <stdbool.h>
#include <stddef.h>

/* How far one step of left or right seeks, for the press `b` held in `in` */
double seek_step(const in_state *in, in_button b);
/* `sec` as minutes and seconds */
void mmss(char *out, size_t n, double sec);
/* A chapter as Now Playing names it */
void chapter_label(int i, char *out, size_t n);
/* Now Playing drawn in Cover, the look it always had; when it next wants a
 * frame */
unsigned np_draw(app *a, const mu_now *mn, const char *next);
/* Now Playing drawn in the look that is on (src/nowplaying_looks.h) */
unsigned np_draw_now(app *a, const mu_now *mn, const char *next);

/* MENU's Lock Screen and Shuffle All, asked from MP4's menu; whether a lock
 * can happen here */
void mp4_ask_lock(void);
void mp4_ask_shuffle(void);
/* MENU's Quit (or X there): MP4 closes after its menu */
void mp4_ask_quit(void);
bool lock_allowed(void);
/* An MP4 screen's presses: read, SELECT alone let go, a screen coming up,
 * and the volume, brightness, X and SELECT with START keys (true: lock) */
void mp4_input(app *a);
bool mp4_select(void);
bool mp4_start(void);    /* START alone, let go: the screen turns (11.67) */
void mp4_input_reset(void);
bool mp4_keys(app *a);
/* The lock screen: its panel, and the screen until it is let go */
void lock_panel(app *a, const mp4lock *l);
void mp4_locked(app *a);
/* MP4's menu, from Now Playing and the shelf */
mp4_exit mp4_menu(app *a);

/* A frame of the video player, its controls and the header at k (0 hidden,
 * 1 shown) and the picture `grow` of the way to the whole screen (with the
 * header gone); when it next wants a frame by itself (0 while the title
 * slides). Whether the picture playing grows at all: a 16:9 one does not. */
unsigned np_draw_video(app *a, const mu_now *mn, float k, float grow);
bool np_video_grows(void);
/* Now Playing, opened from the shelf or a list */
mp4_exit mp4_now_screen(app *a);

#endif
