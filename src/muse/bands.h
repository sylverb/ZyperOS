/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MUSE_BANDS_H
#define MUSE_BANDS_H

/* The sound as 32 bands, for the bars of MP4's Player Looks (DESIGN.md
 * 11.14): what is HEARD at this moment, not what the decoder has reached.
 *
 * The player thread hands over every chunk it writes, with how much was
 * still queued in the device after the write (bands_feed); the socket thread
 * asks for the levels about thirty times a second while anyone wants them
 * (bands_now). Between the two, the frames of the last 170 ms, one channel
 * (the two averaged), and the moment of the last write, so bands_now can
 * take the 1024 frames that end where the listener is: the queue is 200 ms
 * deep, and bars read off the decoder ran a fifth of a second ahead of the
 * drums.
 *
 * A 1024-point FFT under a Hann window, 46.9 Hz a bin at 48 kHz, read in 32
 * bands spaced evenly in pitch from 50 Hz to 12 kHz (a quarter of an octave
 * each): a band is its loudest bin, as a spectrum analyser's bar is, and a
 * band narrower than a bin is the curve between the two bins around its
 * middle. Not above 12 kHz: most MP3s stop at 16, and the bars over 12 were
 * dark on every song tried. 1024 and not the 512 the plan
 * said: at 512 a bin is 94 Hz, and the nine bands under 200 Hz would have
 * shared two bins, nine bars moving as two. The cost is still nothing: about
 * 60 us a time on the Pixel 2's core, 2 ms a second (tools/bands-check.c).
 *
 * Each band is its level in dB, from BANDS_FLOOR (0) to BANDS_CEIL (255),
 * tilted BANDS_TILT dB an octave about 1 kHz: up above it, where a song's
 * power falls off faster than the ear's interest in it (the hi-hat, the
 * voice's s), and down below, where the bass would otherwise hold its bars
 * full. The scale is set against real music, not a test tone:
 * tools/bands-check.c --song plays a file through it and says where each
 * band sits. */

#include <stdbool.h>
#include <stdint.h>

#define BANDS_N    32
#define BANDS_FFT  1024
#define BANDS_LO   50.0f
#define BANDS_HI   12000.0f

/* Player thread: `n` frames just written (DEC_CHANNELS interleaved), and the
 * frames the device still held queued once the write returned */
void bands_feed(const int16_t *frames, int n, long queued_after);

/* Player thread: the queue thrown away (a pause, a seek, a new file), so
 * nothing written before it is heard any more */
void bands_drop(void);

/* Socket thread: the 32 levels of what is heard now, 0-255. False when
 * nothing has been written since the last drop, or not a window's worth:
 * there is nothing to show. */
bool bands_now(uint8_t out[BANDS_N]);

/* The same, for the check: the levels of `win` frames of one channel,
 * BANDS_FFT of them, as bands_now computes them */
void bands_of(const float *win, uint8_t out[BANDS_N]);

#endif
