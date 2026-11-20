/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_MP4ART_H
#define ZYPEROS_MP4ART_H

#include <stdbool.h>
#include <stddef.h>

#include "library.h"

/* Album covers from MusicBrainz and the Cover Art Archive, for an album whose
 * files carry none, or carry one too small to be crisp where MP4 draws it.
 *
 * MusicBrainz is asked which release an album is, from the artist and title
 * its folders are named, and the Cover Art Archive for that release group's
 * front cover at 500px. Neither needs a key. MusicBrainz asks for at most a
 * request a second, and a User-Agent naming the application and a way to reach
 * its author - net.c sends the project's address.
 *
 * THE FIRST HIT IS NOT THE ANSWER. Measured against the first card, 2026-09-19:
 * The Bends and Little Voice each matched a later single of the same name
 * first, and La Strada matched three different bands. What settles it is the
 * track count - the release group one of whose releases has as many tracks as
 * the folder does, or the nearest - which chose all seven right, a five-track
 * rip of a six-track EP among them. mp4art_pick is that rule, checked against
 * the replies MusicBrainz actually gave (tools/mp4art-check.c).
 *
 * Polled from a screen's loop and never waited on: mp4art_step starts or reads
 * one request and returns, on the launcher's single async slot (net.h). */

typedef struct {
	char artist[128], album[128];
	char base[LIB_PATH * 2];   /* where the cover goes, less its extension */
	int  tracks;               /* in the folder: what the rule matches on */
	int  id;                   /* the caller's, handed back when it lands */
} mp4art_job;

typedef struct {
	int  done, n;              /* albums finished, of how many */
	int  found, missing, failed;
	char now[260];             /* "Album - Artist", the one being looked up */
	/* A cover that arrived on the last step: the job's id and the release
	 * group it came from, or -1 and "". Seen once; the next step clears it. */
	int  landed;
	char rg[64];
	char problem[96];          /* why it stopped early, or "" */
} mp4art_status;

/* The release group to take from a MusicBrainz release search reply, for an
 * album of `tracks` tracks: among the releases MusicBrainz scores 90 or more,
 * the one whose track count is nearest, a higher score breaking a tie. False
 * when nothing in the reply is the album. Pure. */
bool mp4art_pick(const char *json, size_t len, int tracks, char *rg, size_t n);

/* The search for an album, as a URL: the release by its title and artist, each
 * a quoted phrase, with the two characters a phrase gives meaning escaped. An
 * empty artist searches the title alone. False when it does not fit. Pure. */
bool mp4art_search_url(const char *artist, const char *album, char *out, size_t n);

/* WHERE TO LOOK, IN ORDER, each asked only when the one before found nothing.
 * The folder names are what a person typed, and MusicBrainz searches them as
 * exact phrases, so a name that says more than the record's own misses it.
 * Measured on the card 2026-09-28, three of ten albums missed that way:
 *
 *   Son Little (Deluxe Edition)    MusicBrainz calls every edition Son Little
 *   Yo-Yo Ma, Stuart Duncan, ...   credited "... Edgar Meyer & Chris Thile",
 *                                  with a different hyphen in Yo-Yo
 *   Trompe Le Monde                a folder of tracks with no artist above it,
 *                                  so its artist was its own title
 *
 * So: the names as they are; then the album without a trailing "(...)" or
 * "[...]" and only the first of several artists; then, for a folder whose
 * artist is its own title, the title alone. Each different from the ones
 * before it, and the track count still decides among what comes back. Fills
 * `artists` and `albums`; returns how many. Pure. */
#define MP4ART_TRIES 3
int mp4art_tries(const char *artist, const char *album,
                  char artists[][128], char albums[][128]);

/* A picture's size from its header, JPEG or PNG, without decoding it. Pure. */
bool mp4art_image_size(const char *path, int *w, int *h);

/* Start on `n` albums, the jobs copied. `reply` is where a search's answer is
 * written while it is read. False when there is nothing to do or no memory. */
bool mp4art_begin(const mp4art_job *jobs, int n, const char *reply);

/* One step at `now_ms`: 1 still working, 0 finished or stopped. */
int  mp4art_step(unsigned now_ms);
void mp4art_status_get(mp4art_status *st);

/* Stop, and give the async slot back. */
void mp4art_cancel(void);

#endif
