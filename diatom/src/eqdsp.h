/* SPDX-License-Identifier: GPL-3.0-only OR MPL-2.0 */
/* Vendored from ZyperOS src/eqdsp.h, which is where it is changed: MP4, the
 * menu music, Diatom and the ports' plugin shape the sound with the same
 * five bands. Copy it back here whole; tools/check-copies.py compares. */
#ifndef ZYPEROS_EQDSP_H
#define ZYPEROS_EQDSP_H

/* The equalizer's sound: five bands, one filter each, the same everywhere
 * ZyperOS makes or passes on sound - MP4 (src/mp4d/mp4d.c), the menu music
 * (src/bgm_out.c), a game (Diatom's src/audio.c, a copy of this file) and a
 * port (plastron's zyperos-eq LADSPA plugin, a copy too). The settings and
 * the presets are the launcher's (src/eq.h); this is the arithmetic alone,
 * header-only and pure C (libm), so each of those builds takes it as it is.
 *
 *   60 Hz    a low shelf      250 Hz, 1 kHz, 4 kHz   peaks, Q 1.1
 *   12 kHz   a high shelf     each -6 to +6 dB, in whole steps
 *
 * The filters are RBJ's cookbook biquads (Robert Bristow-Johnson's "Audio
 * EQ Cookbook"), the shelves at slope 1, in direct form I: a form whose
 * coefficients can move under a playing signal without a click, which is
 * how a change is made. Every gain moves to its new value over EQ_RAMP_MS
 * (20 ms), the coefficients worked out again each millisecond on the way;
 * so a step on the Equalizer page is heard as a turn, never a tick.
 *
 * Flat costs nothing: with every band at 0 and no move under way, eq_run
 * returns at once and the samples are not touched. At 0 dB each filter is
 * exactly the identity (its numerator and denominator are the same), so
 * the step from flat to a curve, and back, is seamless.
 *
 * A boost can take a loud song over full scale. Past 90% of it the samples
 * bend smoothly toward full scale (a soft knee) rather than being cut flat,
 * and only while a band is boosted: a cut never needs it. The level is not
 * pulled down for a boost, so Bass is more bass, not less of the rest. */

#include <math.h>
#include <stdint.h>
#include <string.h>

#define EQ_BANDS    5
#define EQ_DB_MIN   (-6)
#define EQ_DB_MAX   6
#define EQ_RAMP_MS  20

static const float EQ_FREQ[EQ_BANDS] = { 60.0f, 250.0f, 1000.0f, 4000.0f, 12000.0f };
#define EQ_PEAK_Q   1.1f

/* One filter's coefficients, divided by its a0 */
typedef struct { float b0, b1, b2, a1, a2; } eq_coef;

typedef struct {
	int     rate;                   /* samples a second, a channel */
	int     tick;                   /* samples a millisecond: one ramp step */
	float   now[EQ_BANDS];          /* each gain as it is, dB */
	float   want[EQ_BANDS];         /* and as it is going to be */
	float   step[EQ_BANDS];         /* dB a step while it moves */
	int     steps;                  /* steps left in the move, 0 at rest */
	int     until;                  /* samples left in this step */
	eq_coef c[EQ_BANDS];
	float   cw[EQ_BANDS], sw[EQ_BANDS];   /* each centre's cosine and sine */
	/* Each filter's last two inputs and outputs, a channel each */
	float   x1[EQ_BANDS][2], x2[EQ_BANDS][2], y1[EQ_BANDS][2], y2[EQ_BANDS][2];
	int     boost;                  /* a band above 0 now or wanted: the knee */
} eq_state;

/* Band `band`'s filter at `db`, its centre's cosine and sine at the rate
 * given (eq_init works them out once) */
static inline void eq_coef_at(eq_coef *c, int band, float db, float cw, float sw)
{
	const float A = powf(10.0f, db / 40.0f);
	float b0, b1, b2, a0, a1, a2;

	if (band == 0 || band == EQ_BANDS - 1) {
		/* A shelf at slope 1: alpha = sin(w0)/2 * sqrt(2) */
		const float alpha = sw * 0.70710678f, sa = 2.0f * sqrtf(A) * alpha;

		if (band == 0) {
			b0 = A * ((A + 1) - (A - 1) * cw + sa);
			b1 = 2 * A * ((A - 1) - (A + 1) * cw);
			b2 = A * ((A + 1) - (A - 1) * cw - sa);
			a0 = (A + 1) + (A - 1) * cw + sa;
			a1 = -2 * ((A - 1) + (A + 1) * cw);
			a2 = (A + 1) + (A - 1) * cw - sa;
		} else {
			b0 = A * ((A + 1) + (A - 1) * cw + sa);
			b1 = -2 * A * ((A - 1) + (A + 1) * cw);
			b2 = A * ((A + 1) + (A - 1) * cw - sa);
			a0 = (A + 1) - (A - 1) * cw + sa;
			a1 = 2 * ((A - 1) - (A + 1) * cw);
			a2 = (A + 1) - (A - 1) * cw - sa;
		}
	} else {
		const float alpha = sw / (2.0f * EQ_PEAK_Q);

		b0 = 1 + alpha * A;
		b1 = -2 * cw;
		b2 = 1 - alpha * A;
		a0 = 1 + alpha / A;
		a1 = -2 * cw;
		a2 = 1 - alpha / A;
	}
	c->b0 = b0 / a0;
	c->b1 = b1 / a0;
	c->b2 = b2 / a0;
	c->a1 = a1 / a0;
	c->a2 = a2 / a0;
}

static inline void eq_coef_of(eq_coef *c, int band, float db, int rate)
{
	const float w0 = 2.0f * 3.14159265358979f * EQ_FREQ[band] / (float)rate;

	eq_coef_at(c, band, db, cosf(w0), sinf(w0));
}

/* Flat, at `rate` (48000 for MP4, the menu music and the dmix) */
static inline void eq_init(eq_state *e, int rate)
{
	int b;

	memset(e, 0, sizeof *e);
	e->rate = rate > 0 ? rate : 48000;
	e->tick = e->rate / 1000 > 0 ? e->rate / 1000 : 1;
	for (b = 0; b < EQ_BANDS; b++) {
		const float w0 = 2.0f * 3.14159265358979f * EQ_FREQ[b] / (float)e->rate;

		e->cw[b] = cosf(w0);
		e->sw[b] = sinf(w0);
		eq_coef_at(&e->c[b], b, 0.0f, e->cw[b], e->sw[b]);
	}
}

/* Whether eq_run would touch the samples: a band off 0, or a move */
static inline int eq_active(const eq_state *e)
{
	int b;

	if (e->steps > 0) return 1;
	for (b = 0; b < EQ_BANDS; b++)
		if (e->now[b] != 0.0f) return 1;
	return 0;
}

static inline void eq_boost_of(eq_state *e)
{
	int b;

	e->boost = 0;
	for (b = 0; b < EQ_BANDS; b++)
		if (e->now[b] > 0.0f || e->want[b] > 0.0f) e->boost = 1;
}

/* The bands to go to, dB, each held to -6..+6: from where they are now,
 * over EQ_RAMP_MS. The same again changes nothing. */
static inline void eq_set(eq_state *e, const int8_t db[EQ_BANDS])
{
	int b, same = 1;

	for (b = 0; b < EQ_BANDS; b++) {
		const int d = db[b] < EQ_DB_MIN ? EQ_DB_MIN : db[b] > EQ_DB_MAX ? EQ_DB_MAX : db[b];

		if ((float)d != e->want[b]) same = 0;
		e->want[b] = (float)d;
	}
	if (same) return;
	for (b = 0; b < EQ_BANDS; b++) e->step[b] = (e->want[b] - e->now[b]) / (float)EQ_RAMP_MS;
	e->steps = EQ_RAMP_MS;
	e->until = 0;
	eq_boost_of(e);
}

/* The same at once, with no move: for a stream that is starting */
static inline void eq_jump(eq_state *e, const int8_t db[EQ_BANDS])
{
	int b;

	eq_set(e, db);
	for (b = 0; b < EQ_BANDS; b++) {
		e->now[b] = e->want[b];
		eq_coef_at(&e->c[b], b, e->now[b], e->cw[b], e->sw[b]);
	}
	e->steps = 0;
	eq_boost_of(e);
}

/* Past 90% of full scale, bent smoothly toward it: f(0.9) = 0.9 with a
 * slope of 1, f reaching 1 as x goes on. Only above the knee. */
static inline float eq_knee(float v)
{
	const float k = 0.9f * 32768.0f, room = 32767.0f - k;
	const float a = v < 0 ? -v : v;
	float over, out;

	if (a <= k) return v;
	over = (a - k) / room;
	out = k + room * (over / (1.0f + over));
	return v < 0 ? -out : out;
}

static inline int16_t eq_s16(float v)
{
	return v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : (int16_t)lrintf(v);
}

/* One band over `n` frames of `f` (interleaved stereo, float), in place,
 * its coefficients fixed: the two channels' recursions side by side, the
 * state in locals, so the loop is the filter's arithmetic alone */
static inline void eq_band_run(eq_state *e, int b, float *f, int n)
{
	const eq_coef c = e->c[b];
	float lx1 = e->x1[b][0], lx2 = e->x2[b][0], ly1 = e->y1[b][0], ly2 = e->y2[b][0];
	float rx1 = e->x1[b][1], rx2 = e->x2[b][1], ry1 = e->y1[b][1], ry2 = e->y2[b][1];
	int i;

	for (i = 0; i < n; i++) {
		const float l = f[i * 2], r = f[i * 2 + 1];
		const float yl = c.b0 * l + c.b1 * lx1 + c.b2 * lx2 - c.a1 * ly1 - c.a2 * ly2;
		const float yr = c.b0 * r + c.b1 * rx1 + c.b2 * rx2 - c.a1 * ry1 - c.a2 * ry2;

		lx2 = lx1; lx1 = l; ly2 = ly1; ly1 = yl;
		rx2 = rx1; rx1 = r; ry2 = ry1; ry1 = yr;
		f[i * 2] = yl;
		f[i * 2 + 1] = yr;
	}
	/* Denormals: a filter ringing down to nothing would run its state into
	 * numbers the FPU takes many cycles over */
	if (fabsf(ly1) < 1e-15f) ly1 = 0.0f;
	if (fabsf(ly2) < 1e-15f) ly2 = 0.0f;
	if (fabsf(ry1) < 1e-15f) ry1 = 0.0f;
	if (fabsf(ry2) < 1e-15f) ry2 = 0.0f;
	e->x1[b][0] = lx1; e->x2[b][0] = lx2; e->y1[b][0] = ly1; e->y2[b][0] = ly2;
	e->x1[b][1] = rx1; e->x2[b][1] = rx2; e->y1[b][1] = ry1; e->y2[b][1] = ry2;
}

/* The float copy eq_run works through, in frames */
#define EQ_SEG 256

/* `frames` interleaved stereo frames of `f`, float at 16-bit's scale (full
 * scale 32768), in place: in runs over which the coefficients hold still (a
 * whole buffer at rest, a millisecond while a gain moves), each band over
 * the run in turn, then the knee while a band is boosted. A port's plugin
 * calls it on its own floats; eq_run on 16-bit samples. */
static inline void eq_run_f(eq_state *e, float *f, int frames)
{
	int at = 0, i, b;

	if (!eq_active(e)) return;
	while (at < frames) {
		int n = frames - at;

		if (e->steps > 0 && e->until <= 0) {
			/* The next step of a move: the gains and their filters */
			e->steps--;
			for (b = 0; b < EQ_BANDS; b++) {
				e->now[b] = e->steps ? e->now[b] + e->step[b] : e->want[b];
				eq_coef_at(&e->c[b], b, e->now[b], e->cw[b], e->sw[b]);
			}
			if (!e->steps) eq_boost_of(e);
			e->until = e->tick;
		}
		if (e->steps > 0 && n > e->until) n = e->until;
		for (b = 0; b < EQ_BANDS; b++) eq_band_run(e, b, f + at * 2, n);
		if (e->boost)
			for (i = at * 2; i < (at + n) * 2; i++) f[i] = eq_knee(f[i]);
		if (e->steps > 0) e->until -= n;
		at += n;
	}
}

/* `frames` interleaved stereo frames of `buf`, in place, through a float
 * copy a run at a time */
static inline void eq_run(eq_state *e, int16_t *buf, int frames)
{
	float f[EQ_SEG * 2];
	int at, i;

	if (!eq_active(e)) return;
	for (at = 0; at < frames; ) {
		const int n = frames - at < EQ_SEG ? frames - at : EQ_SEG;

		for (i = 0; i < n * 2; i++) f[i] = (float)buf[at * 2 + i];
		eq_run_f(e, f, n);
		for (i = 0; i < n * 2; i++) buf[at * 2 + i] = eq_s16(f[i]);
		at += n;
	}
}

/* ---- the gains a port's plugin reads, from the launcher ------------------- */

/* The file the launcher keeps the bands in force for the output in use, for
 * the processes that cannot be told over a socket: a port's ALSA plugin
 * (plastron's zyperos-eq) maps it and reads it once a period. Fixed size,
 * written in place: `seq` is odd while the launcher writes and even after,
 * so a reader that sees it change, or odd, reads again. All bands 0 is
 * flat. $ZYPEROS_EQ_FILE stands for the path on a computer. */
#define EQ_SHARE_PATH  "/run/zyperos/eq"
#define EQ_SHARE_MAGIC 0x3151455aU    /* "ZEQ1" */

typedef struct {
	uint32_t magic;
	uint32_t seq;
	int8_t   db[EQ_BANDS];
	uint8_t  pad[3];
} eq_share;

/* `s`'s bands into `db`, as a reader takes them: 0 (and `db` untouched)
 * while the launcher is writing, or for a file that is not one */
static inline int eq_share_read(const volatile eq_share *s, int8_t db[EQ_BANDS])
{
	uint32_t a, z;
	int8_t got[EQ_BANDS];
	int b;

	if (!s || s->magic != EQ_SHARE_MAGIC) return 0;
	a = __atomic_load_n(&s->seq, __ATOMIC_ACQUIRE);
	if (a & 1) return 0;
	for (b = 0; b < EQ_BANDS; b++) got[b] = s->db[b];
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	z = __atomic_load_n(&s->seq, __ATOMIC_RELAXED);
	if (z != a) return 0;
	memcpy(db, got, sizeof got);
	return 1;
}

#endif
