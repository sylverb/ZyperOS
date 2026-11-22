/* SPDX-License-Identifier: GPL-3.0-only */
/* MP4's bars, held (src/mp4d/bands.h): each band lit by a tone at its own
 * middle and not by one an octave off, silence dark, the window taken where
 * the listener is and not where the decoder got to, and the time it takes.
 *
 *   make check-bands
 *   build-native/bands-check --song FILE   where each band sits on a real
 *                                          song (ffmpeg decodes it): the
 *                                          scale in bands.c is set by this
 */
#include "../src/mp4d/bands.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RATE 48000

static int fails;

static void fail(const char *what)
{
	printf("FAIL: %s\n", what);
	fails++;
}

static void tone(float *w, float hz, float db)
{
	float a = powf(10.0f, db / 20.0f);
	int i;

	for (i = 0; i < BANDS_FFT; i++) w[i] = a * sinf(2.0f * (float)M_PI * hz * (float)i / RATE);
}

static float band_mid(int b)
{
	return BANDS_LO * powf(BANDS_HI / BANDS_LO, ((float)b + 0.5f) / BANDS_N);
}

static double now_s(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

/* A song through the bands, a window every 1600 frames (30 a second): each
 * band's median, its 90th percentile and how often it is empty or full */
static int song(const char *path)
{
	char cmd[1200];
	FILE *p;
	static int16_t pcm[BANDS_FFT * 2];
	static float win[BANDS_FFT];
	static uint8_t lv[20000][BANDS_N];
	int n = 0, b, i, skip = 0;

	snprintf(cmd, sizeof cmd, "ffmpeg -v error -i '%s' -f s16le -ac 2 -ar 48000 -", path);
	p = popen(cmd, "r");
	if (!p) { perror("ffmpeg"); return 1; }
	while (n < 20000 && fread(pcm, 4, BANDS_FFT, p) == BANDS_FFT) {
		/* 1024 frames read; the window every 1600: the 576 between skipped */
		for (i = 0; i < BANDS_FFT; i++) win[i] = (pcm[i * 2] + pcm[i * 2 + 1]) * (0.5f / 32768.0f);
		bands_of(win, lv[n++]);
		for (skip = 0; skip < 576; skip++)
			if (fread(pcm, 4, 1, p) != 1) break;
	}
	pclose(p);
	printf("%d windows of %s\nband   Hz     p10  p50  p90  empty%%  full%%\n", n, path);
	for (b = 0; b < BANDS_N; b++) {
		int hist[256] = { 0 }, acc = 0, p10 = -1, p50 = -1, p90 = -1, v;

		for (i = 0; i < n; i++) hist[lv[i][b]]++;
		for (v = 0; v < 256; v++) {
			acc += hist[v];
			if (p10 < 0 && acc * 10 >= n) p10 = v;
			if (p50 < 0 && acc * 2 >= n) p50 = v;
			if (p90 < 0 && acc * 10 >= n * 9) p90 = v;
		}
		printf("%4d %6.0f   %3d  %3d  %3d   %5.1f  %5.1f\n", b, band_mid(b), p10, p50, p90,
		       100.0 * hist[0] / n, 100.0 * hist[255] / n);
	}
	return 0;
}

int main(int argc, char **argv)
{
	static float w[BANDS_FFT];
	uint8_t out[BANDS_N];
	int b, i, k;
	double t0, us;
	char why[200];

	if (argc == 3 && !strcmp(argv[1], "--song")) return song(argv[2]);

	/* Silence: every bar empty */
	memset(w, 0, sizeof w);
	bands_of(w, out);
	for (b = 0; b < BANDS_N; b++)
		if (out[b]) { fail("silence lights a bar"); break; }

	/* A tone at each band's middle, 30 dB down: that band is the brightest,
	 * or within 8 of it - under 200 Hz a band is narrower than a bin, and a
	 * bass note lights its neighbours nearly as much, which is the
	 * transform's resolution and not a fault; and two octaves off is far
	 * darker. */
	for (b = 0; b < BANDS_N; b++) {
		int top = 0;

		tone(w, band_mid(b), -30.0f);
		bands_of(w, out);
		for (k = 0; k < BANDS_N; k++)
			if (out[k] > out[top]) top = k;
		if (out[b] + 8 < out[top]) {
			snprintf(why, sizeof why, "a tone at %.0f Hz lights band %d (%d) over its own %d (%d)",
			         band_mid(b), top, out[top], b, out[b]);
			fail(why);
		}
		if (b + 8 < BANDS_N && out[b + 8] + 60 > out[b]) {
			snprintf(why, sizeof why, "a tone at %.0f Hz lights band %d, two octaves up, to %d of %d",
			         band_mid(b), b + 8, out[b + 8], out[b]);
			fail(why);
		}
		if (out[b] == 0 || out[b] == 255) {
			snprintf(why, sizeof why, "a tone at %.0f Hz, 30 dB down, reads %d", band_mid(b), out[b]);
			fail(why);
		}
	}

	/* Louder is higher, every 6 dB a step up */
	{
		int last = -1;

		for (k = -60; k <= -12; k += 6) {
			tone(w, 1000.0f, (float)k);
			bands_of(w, out);
			for (b = 0; b < BANDS_N && !(band_mid(b) > 900 && band_mid(b) < 1150); b++) { }
			if (out[b] <= last) {
				snprintf(why, sizeof why, "a tone at %d dB reads %d, no higher than 6 dB down", k, out[b]);
				fail(why);
			}
			last = out[b];
		}
	}

	/* Where the listener is: frames fed with a click at a known place, and a
	 * queue behind them; the window ends at the queue's start, so the click
	 * is in it while it is heard and not after the queue has moved past */
	{
		static int16_t chunk[1024 * 2];
		uint8_t lv[BANDS_N];

		memset(chunk, 0, sizeof chunk);
		bands_drop();
		if (bands_now(lv)) fail("levels with nothing fed since the drop");
		for (i = 0; i < 1024; i++) {
			/* a loud 2 kHz tone in this chunk only */
			int16_t s = (int16_t)(16000.0 * sin(2 * M_PI * 2000.0 * i / RATE));

			chunk[i * 2] = chunk[i * 2 + 1] = s;
		}
		bands_feed(chunk, 1024, 0);
		memset(chunk, 0, sizeof chunk);
		/* eight silent chunks after it, and 8192 frames still queued: the
		 * listener is at the tone's end, the window the tone */
		for (k = 0; k < 8; k++) bands_feed(chunk, 1024, k == 7 ? 8192 : 0);
		if (!bands_now(lv)) fail("no levels with a queue behind the window");
		for (b = 0; b < BANDS_N && band_mid(b) < 2000; b++) { }
		if (lv[b] < 100 && lv[b - 1] < 100) {
			snprintf(why, sizeof why, "the tone being heard reads %d/%d: the window is not where the listener is",
			         lv[b - 1], lv[b]);
			fail(why);
		}
		bands_feed(chunk, 1024, 0);    /* nothing queued: the listener at the end, silence */
		if (!bands_now(lv)) fail("no levels after the queue drained");
		for (k = 0; k < BANDS_N; k++)
			if (lv[k]) { fail("silence heard after the tone reads lit"); break; }
	}

	/* The time: bands_now's whole work, the window copied and transformed */
	tone(w, 440.0f, -20.0f);
	t0 = now_s();
	for (i = 0; i < 20000; i++) bands_of(w, out);
	us = (now_s() - t0) / 20000 * 1e6;
	printf("bands: %.1f us a window on this host, %.2f ms a second at 30 a second\n", us, us * 30 / 1000);

	if (fails) {
		printf("bands-check: %d failed\n", fails);
		return 1;
	}
	printf("bands-check: every band lit by its own tone, silence dark, the window where it is heard\n");
	return 0;
}
