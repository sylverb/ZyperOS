/* SPDX-License-Identifier: GPL-3.0-only */
/* See mp4hand.h. */
#include "mp4hand.h"
#include "mp4queue.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char *const STATES[] = { "stopped", "playing", "paused" };

void mp4hand_free(mp4hand *h)
{
	int i;

	if (!h) return;
	for (i = 0; h->track && i < h->n; i++) free(h->track[i]);
	free(h->track);
	free(h->order);
	memset(h, 0, sizeof *h);
}

/* A value that goes on one line, as it will be read back */
static bool one_line(const char *s)
{
	return s && !strpbrk(s, "\t\n\r");
}

/* Whether order[0..n) holds each of 0..n-1 once */
static bool permutation(const int *order, int n)
{
	unsigned char *seen;
	bool ok = true;
	int i;

	if (n == 0) return true;
	if (!order || !(seen = calloc((size_t)n, 1))) return false;
	for (i = 0; i < n && ok; i++) {
		if (order[i] < 0 || order[i] >= n || seen[order[i]]) ok = false;
		else seen[order[i]] = 1;
	}
	free(seen);
	return ok;
}

static bool fits(const mp4hand *h)
{
	int i;

	if (h->n < 0 || h->n > MP4HAND_MAX || (h->n && (h->pos < 0 || h->pos >= h->n))) return false;
	if (h->state < MH_STOPPED || h->state > MH_PAUSED) return false;
	if (h->mode < 0 || h->mode >= MUQ_MODES) return false;
	if (!(h->at >= 0) || !isfinite(h->at) || !(h->speed > 0) || !isfinite(h->speed)) return false;
	if (!one_line(h->root) || !one_line(h->artist) || !one_line(h->album)) return false;
	if (h->n && (!h->track || !permutation(h->order, h->n))) return false;
	for (i = 0; i < h->n; i++)
		if (!h->track[i] || !h->track[i][0] || !one_line(h->track[i])) return false;
	return true;
}

bool mp4hand_write(const char *path, const mp4hand *h)
{
	char tmp[600];
	struct timespec now;
	FILE *f;
	bool ok;
	int i;

	if (!path || !h || !fits(h)) return false;
	/* Its own name for each writer, so two never share a half-written one */
	snprintf(tmp, sizeof tmp, "%s.%d", path, (int)getpid());
	if (!(f = fopen(tmp, "w"))) return false;
	/* Each write its own id, so a reader that compares the bytes (the
	 * follower, after the in-game menu) sees a new write as one, even of
	 * the same queue at the same place */
	clock_gettime(CLOCK_REALTIME, &now);
	fprintf(f, "%s\nid\t%d.%lld.%09ld\n", MP4HAND_MAGIC, (int)getpid(), (long long)now.tv_sec, now.tv_nsec);
	fprintf(f, "state\t%s\nat\t%.1f\nmode\t%d\nseed\t%u\nbook\t%d\nvideos\t%d\n"
	        "names\t%d\nall\t%d\nspeed\t%.2f\nroot\t%s\nartist\t%s\nalbum\t%s\nn\t%d\npos\t%d\n",
	        STATES[h->state], h->at, h->mode, h->seed, h->book, h->videos,
	        h->names_each, h->all, h->speed, h->root, h->artist, h->album, h->n, h->n ? h->pos : 0);
	fputs("order\t", f);
	for (i = 0; i < h->n; i++) fprintf(f, i ? " %d" : "%d", h->order[i]);
	fputc('\n', f);
	for (i = 0; i < h->n; i++) fprintf(f, "track\t%s\n", h->track[i]);
	ok = !ferror(f);
	if (fclose(f) != 0) ok = false;
	if (!ok || rename(tmp, path) != 0) {
		unlink(tmp);
		return false;
	}
	return true;
}

/* The integer `s` says, whole, into `*out` */
static bool int_of(const char *s, long lo, long hi, long *out)
{
	char *end;
	long v;

	errno = 0;
	v = strtol(s, &end, 10);
	if (end == s || *end || errno || v < lo || v > hi) return false;
	*out = v;
	return true;
}

static bool num_of(const char *s, double *out)
{
	char *end;
	double v = strtod(s, &end);

	if (end == s || *end || !isfinite(v)) return false;
	*out = v;
	return true;
}

static bool copy(char *dst, size_t n, const char *s)
{
	size_t len = strlen(s);

	if (len >= n) return false;
	memcpy(dst, s, len + 1);
	return true;
}

/* The order's line: n numbers apart by one space */
static bool order_of(const char *s, mp4hand *h)
{
	long v;
	int i;

	for (i = 0; i < h->n; i++) {
		char num[16];
		size_t len = strcspn(s, " ");

		if (!len || len >= sizeof num) return false;
		memcpy(num, s, len);
		num[len] = '\0';
		if (!int_of(num, 0, h->n - 1, &v)) return false;
		h->order[i] = (int)v;
		s += len;
		/* One space between two, none after the last */
		if (*s == ' ' && i < h->n - 1) s++;
		else if (*s || i < h->n - 1) return false;
	}
	return !*s;
}

bool mp4hand_read(const char *path, mp4hand *h)
{
	char *line = NULL;
	size_t cap = 0;
	ssize_t len;
	FILE *f;
	bool ok = true, magic = false, have_n = false, have_order = false;
	int tracks = 0;
	long v;

	if (!h) return false;
	memset(h, 0, sizeof *h);
	h->speed = 1.0;
	if (!path || !(f = fopen(path, "r"))) return false;
	while (ok && (len = getline(&line, &cap, f)) >= 0) {
		char *val;

		if (len && line[len - 1] == '\n') line[--len] = '\0';
		if (!magic) {
			ok = magic = !strcmp(line, MP4HAND_MAGIC);
			continue;
		}
		if (!(val = strchr(line, '\t'))) { ok = false; break; }
		*val++ = '\0';
		if (!strcmp(line, "id")) {
			/* each write's own (mp4hand_write): nothing to take */
			ok = val[0] && one_line(val);
		} else if (!strcmp(line, "state")) {
			int k;

			for (k = 0; k < 3 && strcmp(val, STATES[k]); k++) ;
			ok = k < 3;
			h->state = (mh_state)k;
		} else if (!strcmp(line, "at")) {
			ok = num_of(val, &h->at) && h->at >= 0;
		} else if (!strcmp(line, "mode")) {
			ok = int_of(val, 0, MUQ_MODES - 1, &v);
			h->mode = (int)v;
		} else if (!strcmp(line, "seed")) {
			char *end;
			unsigned long s;

			errno = 0;
			s = strtoul(val, &end, 10);
			ok = end != val && !*end && !errno && s <= 0xffffffffUL;
			h->seed = (unsigned)s;
		} else if (!strcmp(line, "book") || !strcmp(line, "videos") || !strcmp(line, "names") ||
		           !strcmp(line, "all")) {
			ok = int_of(val, 0, 1, &v);
			if (line[0] == 'b') h->book = v;
			else if (line[0] == 'v') h->videos = v;
			else if (line[0] == 'n') h->names_each = v;
			else h->all = v;
		} else if (!strcmp(line, "speed")) {
			ok = num_of(val, &h->speed) && h->speed > 0;
		} else if (!strcmp(line, "root")) {
			ok = copy(h->root, sizeof h->root, val);
		} else if (!strcmp(line, "artist")) {
			ok = copy(h->artist, sizeof h->artist, val);
		} else if (!strcmp(line, "album")) {
			ok = copy(h->album, sizeof h->album, val);
		} else if (!strcmp(line, "n")) {
			ok = !have_n && int_of(val, 0, MP4HAND_MAX, &v);
			have_n = true;
			h->n = (int)v;
			if (ok && h->n) {
				h->order = calloc((size_t)h->n, sizeof *h->order);
				h->track = calloc((size_t)h->n, sizeof *h->track);
				ok = h->order && h->track;
			}
		} else if (!strcmp(line, "pos")) {
			ok = have_n && int_of(val, 0, h->n ? h->n - 1 : 0, &v);
			h->pos = (int)v;
		} else if (!strcmp(line, "order")) {
			ok = have_n && !have_order && order_of(val, h);
			have_order = true;
		} else if (!strcmp(line, "track")) {
			ok = have_n && tracks < h->n && val[0] && (h->track[tracks] = strdup(val)) != NULL;
			tracks++;
		} else {
			ok = false;
		}
	}
	free(line);
	fclose(f);
	/* The tracks as many as it said, in an order of them all */
	if (ok) ok = magic && have_n && (have_order || !h->n) && tracks == h->n && fits(h);
	if (!ok) {
		/* What was taken so far, counted as `n` for the free */
		mp4hand_free(h);
		return false;
	}
	return true;
}
