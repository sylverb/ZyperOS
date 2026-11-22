/* SPDX-License-Identifier: GPL-3.0-only */
/* Whether a video is too big for the CPU (src/mp4d/vidtry.h), driven with
 * the pictures shown and dropped that the Pixel 2 measured with dav1d on
 * 2026-11-19, a second at a time: what MP4 is told, and when.
 *
 *   720p60 AV1: 77 dropped in the first 2 s, then none     never told
 *   1080p30 AV1: 37 dropped at the start, then 0 to 4 a s  never told
 *   1080p60 AV1: 4 shown and 53 to 91 dropped a second     told by 6 s
 *   too big, then the same file light again (a still scene) taken back
 *   a seek or a pause: its catch-up not counted, the runs kept
 *   no picture at all (MPF_NO_PICTURE): never changed by this
 *
 * And mpf_head's new fields after the slots, where a daemon or launcher
 * older than them leaves them alone. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/mp4d/vidtry.h"

static int fails;

#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

/* `secs` seconds of `fps` pictures a second, `drop(i)` of each second's
 * dropped, from `*now`: the trouble after, and in `*at` when it first became
 * `want` (-1 for never) */
typedef int (*dropfn)(int sec);

static uint32_t run(vidtry *t, double *now, int secs, int fps, dropfn drop, uint32_t trouble,
                    uint32_t want, double *at)
{
	int s, i;

	if (at) *at = -1;
	for (s = 0; s < secs; s++) {
		int d = drop(s), n = fps;

		for (i = 0; i < n; i++) {
			uint32_t was = trouble;

			trouble = vidtry_note(t, *now, i >= d, trouble);
			if (at && *at < 0 && trouble == want && was != want) *at = *now;
			*now += 1.0 / fps;
		}
	}
	return trouble;
}

static int none(int s) { (void)s; return 0; }
/* 720p60: 69 then 8 dropped in its first two seconds, none after */
static int p720_60(int s) { return s == 0 ? 69 : s == 1 ? 8 : 0; }
/* 1080p30 with film grain: 37 in the first two, then 0 to 4 a second */
static int p1080_30(int s) { static const int D[] = { 1, 36, 4, 0, 3, 0, 1, 0, 2, 1, 1, 0, 1, 0, 2 }; return D[s % 15]; }
/* 1080p60: of 60 a second, 4 or so shown */
static int p1080_60(int s) { return s == 0 ? 58 : 56 - (s % 3); }
/* a heavy scene, then a light one: half dropped, then none */
static int half(int s) { (void)s; return 30; }
/* between a tenth and a quarter: a sixth */
static int sixth(int s) { (void)s; return 10; }

int main(void)
{
	vidtry t;
	double now = 100, at;
	uint32_t tr;

	/* The fields MP4 reads, after the slots, inside the head's page */
	CHECK(offsetof(mpf_head, trouble) > offsetof(mpf_head, slot), "trouble is after the slots");
	CHECK(sizeof(mpf_head) <= MPF_HEAD, "the head fits its page: %zu", sizeof(mpf_head));
	CHECK(MPF_FINE == 0, "a daemon older than the field (0) is fine");

	vidtry_reset(&t);
	tr = run(&t, &now, 60, 60, p720_60, MPF_FINE, MPF_TOO_BIG, &at);
	CHECK(tr == MPF_FINE && at < 0, "720p60 AV1 is never too big (told at %.1f)", at - 100);

	vidtry_reset(&t);
	now = 100;
	tr = run(&t, &now, 60, 30, p1080_30, MPF_FINE, MPF_TOO_BIG, &at);
	CHECK(tr == MPF_FINE && at < 0, "1080p30 AV1 is never too big (told at %.1f)", at - 100);

	vidtry_reset(&t);
	now = 100;
	tr = run(&t, &now, 20, 60, p1080_60, MPF_FINE, MPF_TOO_BIG, &at);
	CHECK(tr == MPF_TOO_BIG, "1080p60 AV1 is too big");
	CHECK(at >= 100 + 2 * VIDTRY_STRETCH && at <= 100 + 3 * VIDTRY_STRETCH + 0.1,
	      "1080p60 AV1 is told 4 to 6 s after it starts, not %.2f", at - 100);
	printf("1080p60 AV1: told %.1f s after it starts\n", at - 100);

	/* Light again: taken back after two good stretches, the first of them
	 * not the stretch it turned in */
	tr = run(&t, &now, 10, 60, none, tr, MPF_FINE, &at);
	CHECK(tr == MPF_FINE, "too big, then light again: taken back");
	printf("taken back %.1f s after it turned light\n", at - (now - 10));

	/* Between a tenth and a quarter (a sixth): neither said nor taken back */
	vidtry_reset(&t);
	now = 100;
	tr = run(&t, &now, 30, 60, sixth, MPF_FINE, MPF_TOO_BIG, &at);
	CHECK(tr == MPF_FINE, "a sixth dropped is not too big");

	/* A seek: its catch-up not counted, even when it drops all */
	vidtry_reset(&t);
	now = 100;
	tr = run(&t, &now, 10, 30, none, MPF_FINE, MPF_TOO_BIG, NULL);
	vidtry_restart(&t);
	tr = run(&t, &now, 2, 30, p1080_60, tr, MPF_TOO_BIG, &at);
	tr = run(&t, &now, 10, 30, none, tr, MPF_TOO_BIG, &at);
	CHECK(tr == MPF_FINE, "a seek's catch-up is not too big");

	/* Bad stretches broken by a pause run on: one before, one after */
	vidtry_reset(&t);
	now = 100;
	tr = run(&t, &now, 5, 30, half, MPF_FINE, MPF_TOO_BIG, NULL);   /* warm, bad */
	CHECK(tr == MPF_FINE, "one bad stretch is not too big");
	vidtry_restart(&t);
	now += 30;
	tr = run(&t, &now, 5, 30, half, tr, MPF_TOO_BIG, NULL);         /* warm, bad */
	CHECK(tr == MPF_TOO_BIG, "bad, a pause, bad again: too big");

	/* No picture is never this rule's */
	vidtry_reset(&t);
	now = 100;
	tr = run(&t, &now, 20, 60, p1080_60, MPF_NO_PICTURE, MPF_TOO_BIG, NULL);
	CHECK(tr == MPF_NO_PICTURE, "no picture stays no picture");
	tr = run(&t, &now, 20, 60, none, MPF_NO_PICTURE, MPF_FINE, NULL);
	CHECK(tr == MPF_NO_PICTURE, "no picture is not taken back");

	if (fails) return 1;
	printf("ok: vidtry\n");
	return 0;
}
