/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_MP4VIDEO_H
#define ZYPEROS_MP4VIDEO_H

#include <stdbool.h>
#include <stddef.h>

#include <SDL.h>

/* MP4's video, the launcher's half: the newest picture mp4d put in
 * the shared frames (src/mp4frame.h, written by src/mp4d/vid.c), as a YUV
 * texture the GPU turns into color as it draws it.
 *
 * Polled from the player's loop, never waited on. The daemon decides when
 * each picture is due - it holds the sound's clock - so all this does is take
 * the newest one whenever there is a newer one, which is also why it needs no
 * clock of its own. */

/* A picture newer than the last one taken, uploaded: true when the texture
 * changed and the screen wants presenting. */
bool mp4v_update(SDL_Renderer *r);

/* The picture to draw, or NULL when there is none yet for the file playing -
 * the daemon has opened it and not decoded its first frame, or it is another
 * file's picture still sitting in the frames. In `src` (which may be NULL)
 * the part of the texture the picture is in, to draw from: the texture is as
 * wide as the frames' rows, a multiple of 128 (src/mp4frame.h, mpf_stride),
 * and the picture is at its left. */
SDL_Texture *mp4v_texture(SDL_Rect *src);

/* The largest rectangle inside `box` with the picture's shape - its pixels'
 * own shape counted, for the anamorphic DVD rips that store 720x480 for a
 * 16:9 picture - centered in it. `box` itself when there is no picture. */
SDL_Rect mp4v_fit(SDL_Rect box);

/* What the daemon says became of the picture of the file playing
 * (src/mp4frame.h's trouble): MPF_FINE, MPF_TOO_BIG while this CPU drops
 * more than a quarter of its pictures, or MPF_NO_PICTURE for a format
 * nothing here decodes; with the format's short name, FFmpeg's ("av1"), into
 * `format` and its frames a second x 1000 (0 unknown) into `rate_milli`
 * when they are not NULL. MPF_FINE with no daemon, or an older one. */
int mp4v_trouble(char *format, size_t n, unsigned *rate_milli);

/* A trouble in place of the daemon's, for a host shot (--video-trouble);
 * -1 for the daemon's again */
void mp4v_test_trouble(int trouble, const char *format, unsigned rate_milli);

/* A test picture in place of the daemon's, `w` x `h`: color bars over a
 * gradient, for a host shot of the player (--video), which has no daemon. */
void mp4v_test_picture(SDL_Renderer *r, int w, int h);

#endif
