/* SPDX-License-Identifier: GPL-3.0-only */
/* The equalizer's arithmetic (src/eqdsp.h), measured on test tones:
 *
 *     make check-eqdsp
 *
 * Flat touches nothing; each band at +6 dB lifts its own tone by about 6 dB
 * and leaves a tone two octaves away within 1.5 dB; a step of a band moves
 * the sound over 20 ms with no jump bigger than the tone's own; the knee
 * keeps a boosted loud tone under full scale; the shared file reads back.
 * And what it costs: the time for a second of 48 kHz stereo through five
 * bands, against the second it lasts (a share of one core of this computer;
 * the Pixel 2's Cortex-A35 is slower, so the device's number is its own). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/eqdsp.h"

#define RATE 48000

static int fails;

static void report(int ok, const char *name, double got)
{
	printf("  %s %s (%.3f)\n", ok ? "PASS" : "FAIL", name, got);
	if (!ok) fails++;
}

static void tone(int16_t *buf, int n, float hz, float amp)
{
	int i;

	for (i = 0; i < n; i++) {
		const int16_t v = (int16_t)lrintf(amp * 32767.0f * sinf(2.0f * 3.14159265f * hz * (float)i / RATE));

		buf[i * 2] = buf[i * 2 + 1] = v;
	}
}

/* The level of the second half of `buf` (the filters settled), dB re `amp` */
static double level_db(const int16_t *buf, int n, float amp)
{
	double sq = 0;
	int i;

	for (i = n / 2; i < n; i++) sq += (double)buf[i * 2] * buf[i * 2];
	return 20.0 * log10(sqrt(sq / (n - n / 2)) / (amp * 32767.0 / sqrt(2.0)));
}

static double gain_at(int band, int db, float hz)
{
	static int16_t buf[RATE * 2];
	int8_t g[EQ_BANDS] = { 0 };
	eq_state e;

	eq_init(&e, RATE);
	g[band] = (int8_t)db;
	eq_jump(&e, g);
	tone(buf, RATE, hz, 0.1f);
	eq_run(&e, buf, RATE);
	return level_db(buf, RATE, 0.1f);
}

int main(void)
{
	static int16_t a[RATE * 2], b[RATE * 2];
	int8_t g[EQ_BANDS] = { 0 }, back[EQ_BANDS];
	eq_state e;
	int i, k, same;
	double worst, d;

	printf("eqdsp: the five bands\n");
	/* Flat: nothing at all */
	eq_init(&e, RATE);
	tone(a, RATE, 440.0f, 0.5f);
	memcpy(b, a, sizeof a);
	eq_run(&e, b, RATE);
	report(!memcmp(a, b, sizeof a) && !eq_active(&e), "flat_touches_nothing", 0);
	eq_jump(&e, g);
	eq_run(&e, b, RATE);
	report(!memcmp(a, b, sizeof a), "all_zero_touches_nothing", 0);

	/* Each band at +6 lifts its own tone, and not one two octaves away */
	for (k = 0; k < EQ_BANDS; k++) {
		const float f = EQ_FREQ[k];
		const float far = k < 2 ? f * 4.0f : f / 4.0f;
		char name[64];

		/* A shelf's own tone is at its middle, half its gain; a tone past
		 * it gets all of it */
		d = gain_at(k, 6, k == 0 ? f / 4.0f : k == EQ_BANDS - 1 ? 18000.0f : f);
		snprintf(name, sizeof name, "band_%d_plus6_lifts_its_tone", (int)f);
		report(d > 5.0 && d < 6.6, name, d);
		d = gain_at(k, 6, far);
		snprintf(name, sizeof name, "band_%d_plus6_leaves_%d_hz", (int)f, (int)far);
		report(fabs(d) < 1.5 + (k == 0 || k == EQ_BANDS - 1 ? 1.5 : 0), name, d);
		d = gain_at(k, -6, k == 0 ? f / 4.0f : k == EQ_BANDS - 1 ? 18000.0f : f);
		snprintf(name, sizeof name, "band_%d_minus6_cuts_its_tone", (int)f);
		report(d < -5.0 && d > -6.6, name, d);
	}

	/* A step mid-tone: no jump bigger than the tone's own steepest */
	eq_init(&e, RATE);
	tone(a, RATE, 1000.0f, 0.3f);
	eq_run(&e, a, RATE / 2);
	g[2] = 6;
	eq_set(&e, g);
	eq_run(&e, a + RATE, RATE / 2);
	worst = 0;
	for (i = 1; i < RATE; i++) {
		d = fabs((double)a[i * 2] - a[(i - 1) * 2]);
		if (d > worst) worst = d;
	}
	/* 0.3 at 1 kHz moves at most 2 pi 1000 / 48000 * 0.3 * 32767 a sample,
	 * and twice that after +6 dB */
	report(worst < 2.0 * 2.0 * 3.14159 * 1000.0 / RATE * 0.3 * 32767.0 * 1.05, "a_step_has_no_click", worst);
	report(eq_active(&e) && e.steps == 0 && e.now[2] == 6.0f, "the_move_ends_in_20_ms", e.now[2]);

	/* Back to flat: at rest it stops touching the samples again */
	memset(g, 0, sizeof g);
	eq_set(&e, g);
	eq_run(&e, a, 48 * EQ_RAMP_MS + 48);
	report(!eq_active(&e), "back_to_flat_rests", 0);

	/* A boosted loud tone stays under full scale, bent rather than cut */
	eq_init(&e, RATE);
	g[0] = 6;
	eq_jump(&e, g);
	tone(a, RATE, 40.0f, 0.95f);
	eq_run(&e, a, RATE);
	k = 0;
	for (i = 0; i < RATE * 2; i++) if (a[i] == 32767 || a[i] == -32768) k++;
	report(k == 0, "the_knee_never_cuts_flat", k);

	/* The volume boost: a quiet tone made louder by the gain, a loud one held
	 * under -1 dB of full scale by the limiter, no gain touching nothing */
	{
		int16_t peak = 0;

		eq_init(&e, RATE);
		eq_gain_jump(&e, 0);
		report(!eq_active(&e), "no_gain_touches_nothing", 0);
		eq_gain_jump(&e, 12);
		tone(a, RATE, 440.0f, 0.1f);
		eq_run(&e, a, RATE);
		for (i = RATE; i < RATE * 2; i++) if (abs(a[i]) > peak) peak = (int16_t)abs(a[i]);
		report(peak > 0.1f * 32767 * 3.8f && peak < 0.1f * 32767 * 4.1f, "twelve_db_is_four_times_a_quiet_tone", peak / 3276.7);
		eq_init(&e, RATE);
		eq_gain_jump(&e, 12);
		tone(a, RATE, 440.0f, 0.9f);
		eq_run(&e, a, RATE);
		peak = 0;
		for (i = 0; i < RATE * 2; i++) if (abs(a[i]) > peak) peak = (int16_t)abs(a[i]);
		report(peak <= (int)EQ_LIM_T + 1, "the_limiter_holds_a_loud_tone_under_minus_1_db", peak / 32767.0);
		eq_init(&e, RATE);
		g[0] = 6; g[1] = 6; g[2] = 6; g[3] = 6; g[4] = 6;
		eq_jump(&e, g);
		eq_gain_jump(&e, 12);
		tone(a, RATE, 60.0f, 0.95f);
		eq_run(&e, a, RATE);
		peak = 0;
		for (i = 0; i < RATE * 2; i++) if (abs(a[i]) > peak) peak = (int16_t)abs(a[i]);
		report(peak <= (int)EQ_LIM_T + 1, "and_with_every_band_boosted_too", peak / 32767.0);
		eq_init(&e, RATE);
		eq_gain_jump(&e, 6);
		eq_gain(&e, 0);
		tone(a, RATE, 440.0f, 0.1f);
		eq_run(&e, a, RATE);
		report(!eq_active(&e), "back_to_no_gain_in_20_ms_rests", e.gain_now);
	}

	/* The shared file, and its gain */
	{
		eq_share s = { EQ_SHARE_MAGIC, 2, { 5, 2, 0, 0, 1 }, 8, { 0 } };
		int8_t gain = -1;

		same = eq_share_read(&s, back, &gain) && back[0] == 5 && back[4] == 1 && gain == 8;
		s.seq = 3;
		report(same && !eq_share_read(&s, back, NULL), "the_shared_file_reads_whole_or_not", 0);
		s.seq = 2;
		eq_share_put(&s, NULL, 4);
		report(s.seq == 4 && s.gain == 4 && s.db[0] == 5, "the_gain_written_alone_keeps_the_bands", s.gain);
	}

	/* What it costs: a second of stereo through five boosted bands */
	{
		const int reps = 20;
		struct timespec t0, t1;
		double s;

		eq_init(&e, RATE);
		g[0] = 5; g[1] = 2; g[2] = -2; g[3] = 3; g[4] = 1;
		eq_jump(&e, g);
		tone(a, RATE, 440.0f, 0.3f);
		clock_gettime(CLOCK_MONOTONIC, &t0);
		for (k = 0; k < reps; k++) eq_run(&e, a, RATE);
		clock_gettime(CLOCK_MONOTONIC, &t1);
		s = ((double)(t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9) / reps;
		printf("  cost: %.3f ms for a second of 48 kHz stereo, five bands: %.3f%% of one core here\n",
		       s * 1000.0, s * 100.0);
		report(s < 0.02, "costs_under_2_percent_of_a_core_here", s * 100.0);
	}

	if (fails) {
		printf("\n%d failure%s\n", fails, fails == 1 ? "" : "s");
		return 1;
	}
	printf("ok\n");
	return 0;
}
