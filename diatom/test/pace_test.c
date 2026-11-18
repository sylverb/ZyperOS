/* The frame loop's rule for a late turn at 1x (src/pace.h), case by case and
 * then against a model of the port's sound queue.
 *
 * The model is the Pixel 2's path as the port drives it: SDL's queue holding
 * up to 4096 frames, primed to its target of half, refusing what does not fit
 * (what Diatom counts as dropped); SDL's audio thread taking a period of 1024
 * frames from it whenever the device's own buffer of 2048 has room for one,
 * padding with silence what the queue does not hold; the device playing 48 kHz.
 * The game is a DS game's 59.8261 fps, each frame's sound a frame's time as
 * audio.c's rate control trims it (the queue held at its target, 0.5% at
 * most, proportional plus integral), and each frame takes what the schedule
 * says. The loop paces as main.c does at 1x: sleep to the deadline, or past
 * it ask pace_when_late. "Before" is the rule with no queue to judge by
 * (capacity 0), which is the loop as it was: a debt kept up to four frames,
 * then dropped.
 *
 * Offline and instant: it links src/pace.c and nothing else. */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "pace.h"

static int g_fail, g_pass;
static double g_depth;   /* the queue at its target, in us: what the loop gives pace_when_late */

static void ck(int cond, const char *what)
{
	if (cond) { g_pass++; return; }
	g_fail++;
	printf("  FAIL %s\n", what);
}

#define RATE     48000.0
#define CAP      4096
#define PERIOD   1024
#define DEV_BUF  2048
#define FPS      59.8261

/* ---- the port's queue and the device ---- */
typedef struct {
	double t;          /* us */
	double integral;   /* rate control's */
	double q;          /* frames in SDL's queue */
	double dev;        /* frames in the device's buffer */
	double silence;    /* frames the device played as padding */
	double refused;    /* frames the queue would not take: "audio dropped" */
} port;

/* The device plays to `to`; SDL tops its buffer up a period at a time */
static void play_to(port *p, double to)
{
	while (p->t < to) {
		/* when the device next has room for a period */
		double room_at = p->t + (p->dev - (DEV_BUF - PERIOD)) / RATE * 1e6;

		if (room_at > to) {
			p->dev -= (to - p->t) * RATE / 1e6;
			p->t = to;
			break;
		}
		p->dev = DEV_BUF - PERIOD;
		p->t = room_at;
		{
			double take = p->q < PERIOD ? p->q : PERIOD;

			p->q -= take;
			p->silence += PERIOD - take;
			p->dev += PERIOD;
		}
	}
}

static void push(port *p, double n)
{
	double room = CAP - p->q;
	double take = n < room ? n : room;

	p->q += take;
	p->refused += n - take;
}

/* ---- a game ---- */
typedef struct { long from; double us; } cost_at;

typedef struct {
	long   frames;
	double refused, silence, secs;
	long   resyncs, caught_up;
	double dropped_us;   /* debt the rule let go of */
	double q_max;        /* the fullest the queue was after a frame's sound */
} result;

/* `n` frames, each costing what `costs` says from its frame on (the last
 * entry at or before it), paced with the rule judging by `cap` */
static result game(const cost_at *costs, int ncost, long n, double depth_us)
{
	port p;
	result r;
	double frame_us = 1e6 / FPS, next = 0.0, carry = 0.0;
	long f;

	memset(&p, 0, sizeof p);
	memset(&r, 0, sizeof r);
	p.q = CAP / 2;
	p.dev = DEV_BUF;
	for (f = 0; f < n; f++) {
		double us = costs[0].us, sound;
		int i;

		for (i = 0; i < ncost; i++)
			if (f >= costs[i].from) us = costs[i].us;
		play_to(&p, p.t + us);
		/* audio.c's sync_rate, after the turn's sound: a fuller queue
		 * takes more source a frame, so fewer frames of sound */
		{
			double delta = (p.q - CAP / 2.0) / (CAP / 2.0), adjust;

			if (delta < -1.0) delta = -1.0;
			if (delta > 1.0) delta = 1.0;
			p.integral += delta * 0.002;
			if (p.integral < -1.0) p.integral = -1.0;
			if (p.integral > 1.0) p.integral = 1.0;
			adjust = delta + p.integral;
			if (adjust < -1.0) adjust = -1.0;
			if (adjust > 1.0) adjust = 1.0;
			carry += RATE / FPS / (1.0 + adjust * 0.005);
		}
		sound = floor(carry);
		carry -= sound;
		push(&p, sound);
		if (f > 10 && p.q > r.q_max) r.q_max = p.q;

		next += frame_us;
		if (next > p.t) {
			play_to(&p, next);
		} else {
			double late = p.t - next, keep;

			switch (pace_when_late(late, frame_us, depth_us, &keep)) {
			case PACE_RESYNC:    next = p.t;        r.resyncs++;   break;
			case PACE_CAUGHT_UP: next = p.t - keep; r.caught_up++;
			                     r.dropped_us += late - keep;      break;
			case PACE_KEEP:      break;
			}
		}
	}
	r.frames = n;
	r.refused = p.refused;
	r.silence = p.silence;
	r.secs = p.t / 1e6;
	return r;
}

static void show(const char *what, result b, result a)
{
	printf("  %-34s before: %5.0f dropped, %5.0f silent, %2ld resync(s), %6.2f s"
	       "   after: %5.0f dropped, %5.0f silent, %2ld resync(s), %2ld cut (%5.1f ms), %6.2f s, queue max %4.0f/%4.0f\n",
	       what, b.refused, b.silence, b.resyncs, b.secs,
	       a.refused, a.silence, a.resyncs, a.caught_up, a.dropped_us / 1000.0, a.secs, b.q_max, a.q_max);
}


/* ---- a core paced by its sound, handing it over in bursts (audio.c, HOLD) ----
 * PPSSPP's retro_run hands over all the sound its emulation thread made since
 * the last, and that thread runs on ahead while no picture comes: a turn of a
 * frame brings `times` frames of sound. The loop paces each turn by its sound,
 * four frames at most (main.c). With `hold`, a push that does not fit waits
 * for room, a step at a time (pace_room_us, 5 ms at most a step), and a turn
 * that waited makes the next wait until the queue is back at its target. */
typedef struct { long frame, times; } burst_at;
typedef struct {
	double refused, silence, secs, held_us, q_after_burst;
	long held;
} paced_result;

static paced_result paced_game(const burst_at *bursts, int nb, long n, bool hold)
{
	port p;
	paced_result r;
	double frame_us = 1e6 / FPS, next = 0.0, carry = 0.0;
	long f;

	memset(&p, 0, sizeof p);
	memset(&r, 0, sizeof r);
	p.q = CAP / 2;
	p.dev = DEV_BUF;
	for (f = 0; f < n; f++) {
		long times = 1, i;
		double sound, turn_us;
		bool held = false;

		for (i = 0; i < nb; i++)
			if (bursts[i].frame == f) times = bursts[i].times;
		play_to(&p, p.t + 3000.0);   /* the core's retro_run: 3 ms */
		carry += RATE / FPS * (double)times;
		sound = floor(carry);
		carry -= sound;
		turn_us = sound * 1e6 / RATE;
		while (sound > 0) {
			double room = CAP - p.q, now = sound < room ? sound : room;

			p.q += now;
			sound -= now;
			if (sound <= 0) break;
			if (!hold) {
				p.refused += sound;
				break;
			}
			{
				double us = pace_room_us((size_t)p.q, (size_t)sound, CAP, RATE);

				if (us < 1000) us = 1000;
				if (us > 5000) us = 5000;
				play_to(&p, p.t + us);
				r.held_us += us;
				held = true;
			}
		}
		if (held) r.held++;
		next += turn_us < frame_us * 4.0 ? turn_us : frame_us * 4.0;
		if (held) {
			double back = p.t + (p.q > CAP / 2.0 ? (p.q - CAP / 2.0) * 1e6 / RATE : 0);

			if (next < back) next = back;
		}
		if (next > p.t) {
			play_to(&p, next);
			if (times > 1) r.q_after_burst = p.q > r.q_after_burst ? p.q : r.q_after_burst;
		} else if (p.t - next > frame_us * 4.0) {
			next = p.t;
		}
	}
	r.refused = p.refused;
	r.silence = p.silence;
	r.secs = p.t / 1e6;
	return r;
}

/* ---- a game locked to the display, or not ----
 * The panel refreshes every `refresh_us`; a turn starts on the loop's
 * schedule (a millisecond or so of scheduling noise, the same series every
 * run), runs the core for `cost_us`, and hands its frame over; the frame is
 * flipped at the first refresh at least `draw_us` after that. Two frames at
 * one refresh: the first is replaced unseen. A refresh with no new frame:
 * the last one shown again. Locked: the schedule at the refresh period and
 * each turn's start steered to `target` after a refresh (pace_phase_step). */
typedef struct { long shown, lost, repeats; double secs; } shown_result;

static shown_result shown_game(double core_fps, double refresh_us, bool lock, long n)
{
	const double cost_us = 7600, draw_us = 2000;
	double frame_us = lock ? refresh_us : 1e6 / core_fps, next = 1000, t = 0;
	long last_slot = -1, f;
	unsigned seed = 12345;
	shown_result r;

	memset(&r, 0, sizeof r);
	for (f = 0; f < n; f++) {
		double start, ready, jitter;
		long slot;

		seed = seed * 1103515245u + 12345u;
		jitter = ((seed >> 8) % 2001) - 1000.0;   /* -1 to +1 ms */
		start = (next > t ? next : t) + (jitter > 0 ? jitter : 0) * 0.6;
		ready = start + cost_us + draw_us;
		slot = (long)ceil(ready / refresh_us);
		if (slot == last_slot) r.lost++;
		else if (last_slot >= 0) r.repeats += slot - last_slot - 1;
		if (slot != last_slot) r.shown++;
		last_slot = slot;
		t = start + cost_us;
		next += frame_us;
		if (lock) next += pace_phase_step(start, 0, refresh_us, 1500, PACE_PHASE_GAIN,
		                                  refresh_us * PACE_PHASE_MAX_FRAC);
	}
	r.secs = t / 1e6;
	return r;
}

int main(void)
{
	const double F = 1e6 / FPS;
	char what[128];
	result b, a;
	double worst_before = 0.0, keep;
	int ms;

	/* ---- the rule, case by case ---- */
	g_depth = pace_depth_frames(CAP) * 1e6 / RATE;   /* 21.3 ms */
	ck(pace_depth_frames(CAP) == PERIOD, "the depth: the target less SDL's period, 1024 frames");
	ck(pace_when_late(0.0, F, g_depth, &keep) == PACE_KEEP && keep == 0.0,
	   "on time: nothing to do");
	ck(pace_when_late(15000.0, F, g_depth, &keep) == PACE_KEEP && keep == 15000.0,
	   "15 ms behind, inside the queue's depth: keep it all");
	ck(pace_when_late(g_depth, F, g_depth, &keep) == PACE_KEEP && keep == g_depth,
	   "exactly the queue's depth behind: keep it all");
	ck(pace_when_late(53000.0, F, g_depth, &keep) == PACE_CAUGHT_UP && keep == g_depth,
	   "53 ms behind (a 70 ms frame): keep the queue's depth, drop the rest");
	ck(pace_when_late(F * 4.0, F, g_depth, &keep) == PACE_CAUGHT_UP && keep == g_depth,
	   "four frames behind: keep the queue's depth");
	ck(pace_when_late(F * 4.0 + 1.0, F, g_depth, &keep) == PACE_RESYNC && keep == 0.0,
	   "past four frames: resync");
	ck(pace_when_late(53000.0, F, 0.0, &keep) == PACE_KEEP && keep == 53000.0,
	   "no sound device: no depth, keep it all (the loop as it was)");

	/* ---- a minute of a game against the queue ---- */
	{
		static const cost_at steady[] = { { 0, 3000.0 } };

		b = game(steady, 1, 3600, 0);
		a = game(steady, 1, 3600, g_depth);
		show("3 ms frames, no stall", b, a);
		ck(b.refused == 0 && a.refused == 0, "no stall: nothing dropped either way");
		ck(a.caught_up == 0 && a.resyncs == 0, "no stall: the rule never acts");
		ck(fabs(a.secs - 3600 / FPS) < 0.02, "no stall: the game at its own speed");
	}

	/* One frame of `ms`, among frames of 3 ms (a light title screen after a
	 * load). The cut needs a stall that empties the queue (over ~43 ms) and
	 * stays under the resync (~83 ms with the frame's own period). */
	for (ms = 30; ms <= 110; ms += 5) {
		const cost_at one[] = { { 0, 3000.0 }, { 300, ms * 1000.0 }, { 301, 3000.0 } };

		b = game(one, 3, 1200, 0);
		a = game(one, 3, 1200, g_depth);
		snprintf(what, sizeof what, "one frame of %d ms", ms);
		show(what, b, a);
		if (b.refused > worst_before) worst_before = b.refused;
		snprintf(what, sizeof what, "one frame of %d ms: nothing dropped with the rule", ms);
		ck(a.refused == 0, what);
		snprintf(what, sizeof what, "one frame of %d ms: no more silence (the stall's own gap)", ms);
		ck(a.silence <= b.silence + PERIOD, what);
		snprintf(what, sizeof what, "one frame of %d ms: lets go of no more than the silence heard and a period", ms);
		ck(a.dropped_us <= (a.silence + PERIOD) * 1e6 / RATE, what);
		snprintf(what, sizeof what, "one frame of %d ms: the queue never full", ms);
		ck(a.q_max < CAP, what);
	}
	ck(worst_before > 300, "before the rule the model drops sound after a stall (it sees the fault)");

	/* A frame the queue covers: its debt is repaid whole, as before */
	{
		static const cost_at covered[] = { { 0, 10000.0 }, { 300, 30000.0 }, { 301, 10000.0 } };

		b = game(covered, 3, 1200, 0);
		a = game(covered, 3, 1200, g_depth);
		show("one frame of 30 ms among 10 ms", b, a);
		ck(a.dropped_us < PERIOD * 1e6 / RATE, "a stall the queue covered: repaid, to within a period");
		ck(fabs(a.secs - b.secs) < PERIOD / RATE, "a stall the queue covered: the same speed");
	}

	/* A DS opening: loads of 55, 70 and 62 ms among 5 to 14 ms frames */
	{
		static const cost_at opening[] = {
			{ 0, 9000.0 }, { 200, 55000.0 }, { 201, 5000.0 }, { 500, 70000.0 },
			{ 501, 14000.0 }, { 800, 62000.0 }, { 801, 5000.0 },
		};

		b = game(opening, 7, 1800, 0);
		a = game(opening, 7, 1800, g_depth);
		show("a DS opening's three loads", b, a);
		ck(b.refused > 0, "a DS opening: sound dropped before the rule");
		ck(a.refused == 0, "a DS opening: nothing dropped with it");
		/* Before, each load's silence stayed in the queue as latency, a
		 * deeper queue that then covered the next load: no more silence is
		 * not the measure here, where it is after one stall (above) */
		ck(b.q_max >= CAP, "a DS opening: before, the queue full");
		ck(a.q_max < CAP, "a DS opening: after, the queue never full");
		ck(a.dropped_us <= (a.silence + 3 * PERIOD) * 1e6 / RATE,
		   "a DS opening: lets go of no more than the silence heard");
	}

	/* ---- a core paced by its sound, in bursts ---- */
	ck(pace_room_us(1000, 1000, CAP, RATE) == 0.0, "room: fits, no wait");
	ck(fabs(pace_room_us(3072, 2048, CAP, RATE) - 1024 * 1e6 / RATE) < 0.01,
	   "room: 1024 frames over, 21.3 ms at 48 kHz");
	ck(pace_room_us(4096, 2048, 0, RATE) == 0.0, "room: no queue, no wait");
	{
		static const burst_at none[] = { { -1, 1 } };
		static const burst_at bursts[] = { { 200, 5 }, { 400, 3 }, { 600, 10 }, { 800, 20 } };
		paced_result b0 = paced_game(none, 1, 1100, false), h0 = paced_game(none, 1, 1100, true);
		paced_result b = paced_game(bursts, 4, 1100, false), h = paced_game(bursts, 4, 1100, true);
		/* the sound the bursts bring past a frame's each: 4 + 2 + 9 + 19 frames */
		const double extra_s = 34 / FPS;

		printf("  sound-paced, no burst: refused %.0f/%.0f, %.2f/%.2f s\n", b0.refused, h0.refused, b0.secs, h0.secs);
		printf("  sound-paced, bursts of 5, 3, 10 and 20 frames: refused %.0f, %.2f s; held: refused %.0f, "
		       "%ld pushes waited %.0f ms, %.2f s, silent %.0f, the queue after a burst %.0f at most\n",
		       b.refused, b.secs, h.refused, h.held, h.held_us / 1000, h.secs, h.silence, h.q_after_burst);
		ck(b0.refused == 0 && h0.refused == 0 && h0.held == 0, "no burst: nothing refused, nothing waits");
		ck(b.refused > 0, "bursts, no hold: sound refused (the model sees the fault)");
		ck(h.refused == 0, "bursts, held: nothing refused");
		ck(h.held <= 4, "  ...only the bursts' turns wait");
		ck(fabs((h.secs - h0.secs) - extra_s) < 0.1, "  ...the game held back by the bursts' extra sound, no more");
		ck(h.silence <= h0.silence + PERIOD, "  ...no more silence than with no burst");
		ck(h.q_after_burst <= CAP / 2 + RATE / FPS + PERIOD, "  ...the queue back near its target after each");
	}

	/* ---- display lock ---- */
	ck(fabs(pace_lock_ratio(60.08, 59.7275) - 1.005902) < 0.00001, "lock: a GBA on the Pixel 2's 60.08 Hz, 0.59% fast");
	ck(fabs(pace_lock_ratio(60.08, 59.8261) - 1.004245) < 0.00001, "lock: a DS, 0.42%");
	ck(fabs(pace_lock_ratio(60.08, 60.0988) - 0.999687) < 0.00001, "lock: an NES, a hair slow");
	ck(pace_lock_ratio(60.08, 50.0070) == 0, "lock: a PAL game, 50 Hz: not locked");
	ck(pace_lock_ratio(0, 59.7275) == 0 && pace_lock_ratio(60.08, 0) == 0, "lock: no refresh or no core rate: not locked");
	{
		const double P = 1e6 / 60.08;
		double s1 = pace_phase_step(1500, 0, P, 1500, 0.1, P / 16);
		double s2 = pace_phase_step(9000, 0, P, 1500, 0.1, P / 16);
		double s3 = pace_phase_step(P - 500, 0, P, 1500, 0.1, P / 16);
		double s4 = pace_phase_step(10 * P + 1600, 3 * P, P, 1500, 0.1, P / 16);

		ck(fabs(s1) < 0.001, "phase: on target, no step");
		ck(s2 < 0 && s2 >= -P / 16, "phase: late in the refresh, the next start moves earlier, at most a sixteenth");
		ck(fabs(s3 - 200) < 0.01, "phase: just before a refresh, the short way round: 2 ms behind target, 0.2 ms later");
		ck(fabs(s4 + 10) < 0.01, "phase: any refresh as the reference, ten refreshes on");
	}
	{
		const double P = 1e6 / 60.08;
		shown_result free_ = shown_game(59.7275, P, false, 3584), locked = shown_game(59.7275, P, true, 3584);

		printf("  a GBA on a 60.08 Hz panel, a minute: core-paced %ld new frames shown, %ld lost, %ld refreshes "
		       "repeated; locked %ld, %ld, %ld\n", free_.shown, free_.lost, free_.repeats, locked.shown,
		       locked.lost, locked.repeats);
		ck(free_.lost > 10 && free_.repeats > 10, "core-paced: frames lost and refreshes repeated (the model sees the judder)");
		ck(locked.lost == 0 && locked.repeats == 0, "locked: one new frame every refresh, none lost or repeated");
		ck(fabs(locked.secs - 3584 * P / 1e6) < 0.05, "locked: at the panel's rate");
	}

	/* A game slow all the time (17.2 ms a frame against 16.7): the same speed */
	{
		static const cost_at slow[] = { { 0, 17200.0 } };

		b = game(slow, 1, 3600, 0);
		a = game(slow, 1, 3600, g_depth);
		show("17.2 ms frames", b, a);
		ck(a.refused == 0 && b.refused == 0, "always slow: nothing dropped either way");
		ck(fabs(a.secs - b.secs) / b.secs < 0.005, "always slow: the same speed");
		ck(fabs(a.silence - b.silence) <= 4 * PERIOD, "always slow: the same silence");
	}

	printf("%d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
