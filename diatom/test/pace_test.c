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
