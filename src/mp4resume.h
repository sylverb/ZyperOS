/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_MP4RESUME_H
#define ZYPEROS_MP4RESUME_H

/* MP4 picks up where it was after a power-off (DESIGN.md 11.67): what was
 * playing when the power went - a power cut, an empty battery, a forced off,
 * or the launcher's own power-off - is there at the next start, paused at its
 * second, on Now Playing. Mine, 2026-11-19: "make sure that if powered off
 * while playing, when powered on it turns on with the media paused where it
 * was".
 *
 * One row in the handheld's database, MP4RESUME_KEY, beside a book's and a
 * video's places: whether it was playing or paused, the queue (an album's, a
 * book's or a series' files, or Shuffle All's every song), the second, and
 * the file by its path on the card, so a file added later moves nothing.
 *
 * Written as a book's place is (src/main.c's book_keep): when a queue starts,
 * when its file changes, when it pauses or plays, every thirty seconds of
 * playing, and at once when the launcher powers off or restarts. Nothing
 * while paused or stopped. A start resumes it only when it says playing:
 * paused, it was not heard when the power went.
 *
 * This file is the row and the rule, with no SDL and no database, so
 * tools/mp4resume-check.c can hold them to their word. */

#include <stdbool.h>
#include <stddef.h>

#define MP4RESUME_KEY   "mp4.resume"
/* Seconds of playing between two writes: a Brick that loses its battery
 * goes back half a minute at most, as a book does */
#define MP4RESUME_EVERY 30.0
#define MP4RESUME_PATH  512

typedef enum { MR_ALBUM, MR_ALL } mr_queue;

typedef struct {
	bool     playing;               /* false: paused */
	mr_queue queue;
	double   at;                    /* seconds into the file */
	char     track[MP4RESUME_PATH]; /* the file, relative to the card */
} mp4resume;

/* The row's value for `r` into `out`: "playing\talbum\t83.4\tMusic/...".
 * False when it does not fit or the path holds a tab or a newline. */
bool mp4resume_format(const mp4resume *r, char *out, size_t n);

/* The row read back. False for anything that is not one: a word it does not
 * know, a second that is not a number or is less than 0, no file. */
bool mp4resume_parse(const char *s, mp4resume *out);

/* What the last write said, for the rule below */
typedef struct {
	bool   written;                 /* a row stands for this queue */
	bool   playing;
	double at;
	char   track[MP4RESUME_PATH];
} mp4resume_seen;

/* Whether the player's state now asks for a write: `heard` it is playing or
 * paused (false: nothing to keep), and `now` a write whatever changed (the
 * launcher powering off, a new queue about to replace this one). */
bool mp4resume_due(const mp4resume_seen *last, bool heard, bool playing, const char *track,
                   double at, bool now);

/* The write done: `last` says it */
void mp4resume_wrote(mp4resume_seen *last, bool playing, const char *track, double at);

#endif
