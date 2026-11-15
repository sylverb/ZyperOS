/* SPDX-License-Identifier: GPL-3.0-only */
/* See powerhold.h. */
#include "powerhold.h"

static bool g_hold;

void powerhold_set_mode(bool hold) { g_hold = hold; }
bool powerhold_mode(void) { return g_hold; }

bool powerhold_key(powerhold *p, bool down, unsigned now)
{
	if (!down) {
		p->down = false;
		p->fired = false;
		return false;
	}
	if (p->down) return false;
	p->down = true;
	p->since = now;
	p->fired = !g_hold;           /* Press: this is the turn-off, once */
	return !g_hold;
}

bool powerhold_due(powerhold *p, unsigned now)
{
	if (!g_hold || !p->down || p->fired) return false;
	if (now - p->since < POWERHOLD_MS) return false;
	p->fired = true;
	return true;
}

int powerhold_permille(const powerhold *p, unsigned now)
{
	unsigned t;

	if (!g_hold || !p->down || p->fired) return -1;
	t = now - p->since;
	return t >= POWERHOLD_MS ? 1000 : (int)(t * 1000u / POWERHOLD_MS);
}

int powerhold_wait_ms(const powerhold *p, unsigned now)
{
	unsigned t;

	if (!g_hold || !p->down || p->fired) return -1;
	t = now - p->since;
	return t >= POWERHOLD_MS ? 0 : (int)(POWERHOLD_MS - t);
}

int powerhold_next_ms(const powerhold *p, unsigned now, int steps)
{
	int left = powerhold_wait_ms(p, now);
	unsigned step = steps > 0 ? POWERHOLD_MS / (unsigned)steps : POWERHOLD_MS;

	if (left <= 0 || !step) return left;
	/* The time since the start is POWERHOLD_MS - left, and the next step is
	 * where that next reaches a multiple of `step` */
	{
		unsigned to = step - (POWERHOLD_MS - (unsigned)left) % step;

		return (int)(to < (unsigned)left ? to : (unsigned)left);
	}
}

void powerhold_consume(powerhold *p)
{
	if (p->down) p->fired = true;
}
