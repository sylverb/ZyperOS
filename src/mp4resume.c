/* SPDX-License-Identifier: GPL-3.0-only */
/* See mp4resume.h. */
#include "mp4resume.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool mp4resume_format(const mp4resume *r, char *out, size_t n)
{
	int k;

	if (!r || !out || !r->track[0] || strpbrk(r->track, "\t\n") || !(r->at >= 0)) return false;
	k = snprintf(out, n, "%s\t%s\t%.1f\t%s", r->playing ? "playing" : "paused",
	             r->queue == MR_ALL ? "all" : "album", r->at, r->track);
	return k > 0 && (size_t)k < n;
}

bool mp4resume_parse(const char *s, mp4resume *out)
{
	const char *f[4], *p = s;
	char *end;
	size_t len;
	int i;

	if (!s || !out) return false;
	for (i = 0; i < 4; i++) {
		f[i] = p;
		if (i == 3) break;
		if (!(p = strchr(p, '\t'))) return false;
		p++;
	}
	memset(out, 0, sizeof *out);
	if (!strncmp(f[0], "playing\t", 8)) out->playing = true;
	else if (strncmp(f[0], "paused\t", 7)) return false;
	if (!strncmp(f[1], "all\t", 4)) out->queue = MR_ALL;
	else if (!strncmp(f[1], "album\t", 6)) out->queue = MR_ALBUM;
	else return false;
	out->at = strtod(f[2], &end);
	if (end == f[2] || *end != '\t' || !(out->at >= 0) || !isfinite(out->at)) return false;
	len = strlen(f[3]);
	if (!len || len >= sizeof out->track || strchr(f[3], '\n')) return false;
	memcpy(out->track, f[3], len + 1);
	return true;
}

bool mp4resume_due(const mp4resume_seen *last, bool heard, bool playing, const char *track,
                   double at, bool now)
{
	if (!heard || !track || !track[0]) return false;
	if (now || !last->written) return true;
	if (playing != last->playing || strcmp(track, last->track)) return true;
	return playing && fabs(at - last->at) >= MP4RESUME_EVERY;
}

void mp4resume_wrote(mp4resume_seen *last, bool playing, const char *track, double at)
{
	last->written = true;
	last->playing = playing;
	last->at = at;
	snprintf(last->track, sizeof last->track, "%s", track ? track : "");
}
