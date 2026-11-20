/* SPDX-License-Identifier: GPL-3.0-only */
/* MP4's Player Looks' motion (src/mp4look.h) held to its numbers: a bar up
 * at once and down on follow's tau, the same at any frame rate; its cap held
 * for peak-hold and then down with gravity in MP4L_CAP_DROP_MS; every bar at
 * rest soon after the sound stops; the bands shared out over a look's bars;
 * the setting's words both ways.
 *
 *   make check-mp4look
 */
#include "../src/design.h"
#include "../src/mp4look.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fails;

static void fail(const char *what, double got, double want)
{
	printf("FAIL: %s: %.4f, not %.4f\n", what, got, want);
	fails++;
}

static void near(const char *what, double got, double want, double tol)
{
	if (fabs(got - want) > tol) fail(what, got, want);
}

/* `ms` exactly, in frames `dt` apart and the rest in one, with `bands` or
 * none */
static void run(mp4l_bars_t *b, float ms, float dt, const uint8_t *bands)
{
	int n = (int)(ms / dt), i;

	for (i = 0; i < n; i++) mp4l_bars_step(b, dt, bands);
	if (ms - (float)n * dt > 0.0f) mp4l_bars_step(b, ms - (float)n * dt, bands);
}

int main(void)
{
	static uint8_t full[MP4L_BANDS], one[MP4L_BANDS];
	mp4l_bars_t b;
	int i, look;

	memset(full, 255, sizeof full);

	/* The words: each look's, round; one nobody knows is Cover */
	for (look = 0; look < MP4L_COUNT; look++)
		if ((int)mp4l_from_key(mp4l_key((mp4l_id)look)) != look) fail("a look's word back", look, look);
	if (mp4l_from_key("tape-deck") != MP4L_COVER || mp4l_from_key(NULL) != MP4L_COVER)
		fail("an unknown word is Cover", 1, 0);
	if (mp4l_bars(MP4L_COVER) != 0) fail("Cover has bars", mp4l_bars(MP4L_COVER), 0);
	if (mp4l_bars(MP4L_SPECTRUM) != 32) fail("Spectrum's bars", mp4l_bars(MP4L_SPECTRUM), 32);

	/* Up at once */
	mp4l_bars_reset(&b, 32);
	mp4l_bars_step(&b, 16.7f, full);
	near("a bar after one frame of full sound", b.lv[0], 1.0, 1e-6);
	near("its cap with it", b.cap[0], 1.0, 1e-6);

	/* Down on follow: after one tau, e^-1 of the way left, at 60 and at 30
	 * frames a second alike */
	run(&b, (float)DS_MS_BAR_FALL, 1000.0f / 60, NULL);
	near("a bar one tau after the sound stops, at 60", b.lv[0], exp(-1.0), 0.002);
	mp4l_bars_reset(&b, 32);
	mp4l_bars_step(&b, 0, full);
	run(&b, (float)DS_MS_BAR_FALL, 1000.0f / 30, NULL);
	near("...and at 30", b.lv[0], exp(-1.0), 0.002);

	/* The cap: held for peak-hold, then down with gravity: at the floor
	 * MP4L_CAP_DROP_MS after (the bar under it long gone) */
	mp4l_bars_reset(&b, 32);
	mp4l_bars_step(&b, 0, full);
	run(&b, (float)DS_MS_PEAK_HOLD - 10, 10, NULL);
	near("the cap just before peak-hold ends", b.cap[0], 1.0, 1e-6);
	run(&b, 210, 10, NULL);
	near("200 ms into its fall", b.cap[0], 1.0 - 0.5 * (2.0 / (MP4L_CAP_DROP_MS * (double)MP4L_CAP_DROP_MS)) * 200 * 200, 0.002);
	mp4l_bars_reset(&b, 32);
	mp4l_bars_step(&b, 0, full);
	run(&b, (float)DS_MS_PEAK_HOLD + MP4L_CAP_DROP_MS - 30, 1000.0f / 60, NULL);
	if (b.cap[0] <= 0.0f) fail("the cap on the floor early", b.cap[0], 0.01);
	run(&b, 60, 1000.0f / 60, NULL);
	near("the cap on the floor after its hold and its drop", b.cap[0], 0.0, 1e-6);

	/* A bar coming up under a falling cap catches it: the cap rides on it */
	mp4l_bars_reset(&b, 32);
	mp4l_bars_step(&b, 0, full);
	run(&b, 600, 10, NULL);
	memset(one, 230, sizeof one);
	mp4l_bars_step(&b, 10, one);
	near("a cap a bar has come up to", b.cap[0], 230.0 / 255, 1e-6);

	/* At rest: a second after the sound stops nothing moves */
	mp4l_bars_reset(&b, 32);
	mp4l_bars_step(&b, 0, full);
	if (mp4l_bars_rest(&b)) fail("at rest with full sound", 1, 0);
	run(&b, (float)DS_MS_PEAK_HOLD + MP4L_CAP_DROP_MS + 400, 1000.0f / 60, NULL);
	if (!mp4l_bars_rest(&b)) fail("not at rest after the cap's hold and drop", b.lv[0] + b.cap[0], 0);

	/* The bands over 18 bars: band 31 alone lights the last bar alone, and
	 * band 0 the first */
	mp4l_bars_reset(&b, 18);
	memset(one, 0, sizeof one);
	one[31] = 255;
	mp4l_bars_step(&b, 0, one);
	for (i = 0; i < 17; i++)
		if (b.lv[i] != 0.0f) fail("band 31 lights a bar before the last", i, 17);
	near("band 31 on the last bar", b.lv[17], 1.0, 1e-6);
	mp4l_bars_reset(&b, 18);
	memset(one, 0, sizeof one);
	one[0] = 128;
	mp4l_bars_step(&b, 0, one);
	near("band 0 on the first bar", b.lv[0], 128.0 / 255, 1e-6);
	for (i = 1; i < 18; i++)
		if (b.lv[i] != 0.0f) fail("band 0 lights a bar after the first", i, 0);

	if (fails) {
		printf("mp4look-check: %d failed\n", fails);
		return 1;
	}
	printf("mp4look-check: bars up at once and down on follow at any rate, caps held %d ms and down in %d,"
	       " at rest after, bands over the bars, the setting's words\n",
	       DS_MS_PEAK_HOLD, MP4L_CAP_DROP_MS);
	return 0;
}
