/* The boost past the top of the volume and the brightness (src/boost.h):
 * what presses, held keys and the clock do to it, held without a handheld.
 *
 * The rules are my picks in the volume boost lab (2026-11-19): at the top, a
 * key held one more second, or three more presses each within a second of
 * the last, opens it; the charge drains back at the speed it filled; the key
 * still held stops at the first cell; going down empties the cells, waits
 * at the top, then folds them; an empty boost closes with the bar; nothing
 * opens on MP4's lock screen, and the menus show the bar as it was.
 *
 *   boost-check     checks
 *
 * Pure C: links nothing but libm (src/osdbar.h's types, no drawing). */
#include "../src/boost.h"

#include <math.h>
#include <stdio.h>

static int fails;

static void ck(int cond, const char *what)
{
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

static bool near(float a, float b) { return fabsf(a - b) < 0.02f; }

/* A key held from `t` for `ms`, repeating as src/keyrepeat.h does (300 ms,
 * then every 90 ms), the clock ticking every 16 ms; returns the time after */
static uint32_t hold(boost_bar *b, int dir, uint32_t t, uint32_t ms, int where, int *did)
{
	uint32_t next = t + 300, end = t + ms;

	*did |= boost_key(b, dir, false, where, t);
	for (; t < end; t += 16) {
		if (t >= next) { *did |= boost_key(b, dir, true, where, t); next += 90; }
		*did |= boost_tick(b, dir > 0, where, t);
	}
	boost_release(b, t);
	*did |= boost_tick(b, false, where, t);
	return t;
}

static int tap(boost_bar *b, int dir, uint32_t t, int where)
{
	int did = boost_key(b, dir, false, where, t);

	boost_release(b, t + 40);
	return did | boost_tick(b, false, where, t + 40);
}

int main(void)
{
	boost_bar b;
	osdbar_look l;
	int did = 0;
	uint32_t t;

	/* A hold from 15: the top at 0.57 s, open a second later */
	boost_init(&b, 15, 20, BOOST_VOL_CELLS, 0);
	t = hold(&b, +1, 1000, 1500, BOOST_OPEN, &did);
	ck(b.level == 20 && !b.open, "a hold of 1.5 s from 15 reaches the top and has not opened yet");
	did = 0;
	boost_init(&b, 15, 20, BOOST_VOL_CELLS, 0);
	t = hold(&b, +1, 1000, 1650, BOOST_OPEN, &did);
	ck(b.open && b.cells == 1, "held 1 s on the top (0.57 s to get there), it opens with one cell filled");
	ck(did & BOOST_OPENED, "and says it opened, for the rumble");
	ck(boost_db(&b) == BOOST_DB_CELL, "one cell is 2 dB");

	/* the key still held stops at the first cell */
	did = 0;
	boost_init(&b, 15, 20, BOOST_VOL_CELLS, 0);
	t = hold(&b, +1, 1000, 4000, BOOST_OPEN, &did);
	ck(b.open && b.cells == 1, "held on past the opening, it stays at one cell");
	tap(&b, +1, t + 100, BOOST_OPEN);
	tap(&b, +1, t + 200, BOOST_OPEN);
	ck(b.cells == 3, "each press after that fills one more");
	for (int i = 0; i < 6; i++) tap(&b, +1, t + 300 + i * 100, BOOST_OPEN);
	ck(b.cells == BOOST_VOL_CELLS && boost_db(&b) == 12, "it stops at six cells, +12 dB");
	boost_look(&b, OSDBAR_VOLUME, BOOST_OPEN, t + 900, &l);
	ck(l.push > 0, "and a press on the full boost pushes the end");

	/* three presses on the top, each within the gap */
	boost_init(&b, 19, 20, BOOST_VOL_CELLS, 0);
	tap(&b, +1, 1000, BOOST_OPEN);
	ck(b.level == 20 && b.taps == 0, "the press that reaches the top is not one on it");
	tap(&b, +1, 1500, BOOST_OPEN);
	ck(b.taps == 1 && near(boost_charge(&b, 1500 + 200), 1.0f / 3), "a press on the top charges a third");
	tap(&b, +1, 2300, BOOST_OPEN);
	ck(b.taps == 2 && !b.open, "a second within 1 s, two thirds");
	did = tap(&b, +1, 3200, BOOST_OPEN);
	ck(b.open && b.cells == 1 && (did & BOOST_OPENED), "the third within 1 s opens it");

	/* presses too far apart start again */
	boost_init(&b, 20, 20, BOOST_VOL_CELLS, 0);
	tap(&b, +1, 1000, BOOST_OPEN);
	tap(&b, +1, 2100, BOOST_OPEN);
	tap(&b, +1, 3200, BOOST_OPEN);
	tap(&b, +1, 4300, BOOST_OPEN);
	ck(!b.open && b.taps == 1, "presses 1.1 s apart never open it");

	/* the presses' charge drains after the gap, linearly, as a hold fills */
	boost_init(&b, 20, 20, BOOST_VOL_CELLS, 0);
	tap(&b, +1, 1000, BOOST_OPEN);
	tap(&b, +1, 1500, BOOST_OPEN);
	ck(near(boost_charge(&b, 2400), 2.0f / 3), "two presses hold two thirds until the gap");
	boost_tick(&b, false, BOOST_OPEN, 2501);
	ck(near(boost_charge(&b, 2501 + 333), 1.0f / 3), "then it drains a third in a third of a second");
	ck(near(boost_charge(&b, 2501 + 667), 0), "and is empty after two thirds of one");

	/* a hold let go early drains at the speed it filled */
	boost_init(&b, 20, 20, BOOST_VOL_CELLS, 0);
	boost_key(&b, +1, false, BOOST_OPEN, 1000);
	boost_tick(&b, true, BOOST_OPEN, 1000);
	ck(near(boost_charge(&b, 1000 + DS_MS_QUICK), 1.0f / 3), "a held press on the top is a press first");
	boost_tick(&b, true, BOOST_OPEN, 1600);
	ck(near(boost_charge(&b, 1600), 1.0f / 3 + 2.0f / 3 * 0.6f), "then the hold fills the rest over a second");
	boost_release(&b, 1600);
	ck(near(boost_charge(&b, 1900), 1.0f / 3 + 2.0f / 3 * 0.3f), "let go, it drains as fast as it filled");
	ck(near(boost_charge(&b, 2200), 1.0f / 3), "back to the press's third");
	ck(!b.open, "and nothing opened");

	/* going down: the cells empty, wait at the top, then fold */
	boost_init(&b, 20, 20, BOOST_VOL_CELLS, 3);
	ck(b.open && b.cells == 3, "a boost kept from before opens at once");
	tap(&b, -1, 1000, BOOST_OPEN);
	tap(&b, -1, 1100, BOOST_OPEN);
	tap(&b, -1, 1200, BOOST_OPEN);
	ck(b.open && b.cells == 0 && b.level == 20, "three presses down empty it; it waits at the top");
	did = tap(&b, -1, 1300, BOOST_OPEN);
	ck(!b.open && b.level == 19 && (did & BOOST_LEVEL), "the next goes under the top and folds it");
	boost_look(&b, OSDBAR_VOLUME, BOOST_OPEN, 1300 + 40, &l);
	ck(l.grow > 0 && l.morph == 1, "folding, the cells go back in at a cell's size");
	boost_look(&b, OSDBAR_VOLUME, BOOST_OPEN, 1300 + 400, &l);
	ck(l.grow == 0 && l.ncells == BOOST_VOL_CELLS, "folded, the pill is back to its own width");

	/* an empty boost closes with the bar, a filled one stays */
	boost_init(&b, 20, 20, BOOST_VOL_CELLS, 1);
	tap(&b, -1, 1000, BOOST_OPEN);
	boost_bar_gone(&b);
	ck(!b.open, "an empty boost closes with the bar");
	boost_init(&b, 20, 20, BOOST_VOL_CELLS, 2);
	boost_bar_gone(&b);
	ck(b.open && b.cells == 2, "a filled one stays");

	/* MP4's lock screen: never opens, but an open one fills and empties */
	boost_init(&b, 20, 20, BOOST_VOL_CELLS, 0);
	did = 0;
	hold(&b, +1, 1000, 3000, BOOST_FILL, &did);
	for (int i = 0; i < 3; i++) tap(&b, +1, 5000 + i * 200, BOOST_FILL);
	ck(!b.open, "on the lock screen neither a hold nor presses open it");
	boost_init(&b, 20, 20, BOOST_VOL_CELLS, 2);
	tap(&b, +1, 1000, BOOST_FILL);
	ck(b.cells == 3, "an open one still fills there");

	/* the menus: the bar as it was; a press under the top folds a kept boost */
	boost_init(&b, 20, 20, BOOST_VOL_CELLS, 2);
	tap(&b, +1, 1000, BOOST_HIDE);
	ck(b.cells == 2, "in the menus a press at the top changes nothing");
	boost_look(&b, OSDBAR_VOLUME, BOOST_HIDE, 1100, &l);
	ck(l.ncells == 0 && l.level == 20, "and the bar shows no boost cells");
	did = tap(&b, -1, 1200, BOOST_HIDE);
	ck(!b.open && b.level == 19 && (did & BOOST_GAIN), "a press down goes under the top and closes the kept boost");
	boost_init(&b, 20, 20, BOOST_VOL_CELLS, 0);
	hold(&b, +1, 1000, 3000, BOOST_HIDE, &did);
	ck(!b.open, "and nothing opens there");

	/* no boost for this bar: the top is the top */
	boost_init(&b, 20, 20, 0, 0);
	hold(&b, +1, 1000, 3000, BOOST_OPEN, &did);
	ck(!b.open, "a bar with no boost cells never opens");

	/* a level from elsewhere under the top closes it */
	boost_init(&b, 20, 20, BOOST_VOL_CELLS, 4);
	boost_set_level(&b, 12);
	ck(!b.open && b.level == 12, "a level set under the top closes the boost");

	/* the look while it charges and opens */
	boost_init(&b, 20, 20, BOOST_VOL_CELLS, 0);
	boost_key(&b, +1, false, BOOST_OPEN, 1000);
	boost_tick(&b, true, BOOST_OPEN, 1300);
	boost_look(&b, OSDBAR_VOLUME, BOOST_OPEN, 1300, &l);
	ck(l.morph == 0 && l.charge > 0.5f && l.grow == 0, "charging: the big cell fills, nothing grown yet");
	boost_tick(&b, true, BOOST_OPEN, 2000);
	boost_look(&b, OSDBAR_VOLUME, BOOST_OPEN, 2000 + DS_MS_BOOST_OPEN / 2, &l);
	ck(l.morph > 0 && l.morph < 1 && l.grow > 0 && l.flash > 0 && l.charge == 0,
	   "opening: it settles, the pill grows, the rim flashes");
	boost_look(&b, OSDBAR_VOLUME, BOOST_OPEN, 2000 + 1000, &l);
	ck(l.morph == 1 && l.grow == 1 && l.flash == 0 && l.fill[0] == 1, "open: one cell, at rest");
	ck(!boost_moving(&b, 2000 + 1000) || b.holding, "and nothing moves");

	/* the mark follows the level in quick */
	boost_init(&b, 10, 20, BOOST_VOL_CELLS, 0);
	boost_look(&b, OSDBAR_VOLUME, BOOST_OPEN, 1000, &l);
	tap(&b, +1, 1000, BOOST_OPEN);
	boost_look(&b, OSDBAR_VOLUME, BOOST_OPEN, 1000, &l);
	ck(near(l.mark, 0.5f), "the mark starts where it was");
	boost_look(&b, OSDBAR_VOLUME, BOOST_OPEN, 1000 + DS_MS_QUICK, &l);
	ck(near(l.mark, 11.0f / 20), "and is at the new level after quick");

	/* brightness: three lift steps */
	boost_init(&b, 11, 11, BOOST_BRIGHT_CELLS, 0);
	for (int i = 0; i < 3; i++) tap(&b, +1, 1000 + i * 300, BOOST_OPEN);
	for (int i = 0; i < 5; i++) tap(&b, +1, 2000 + i * 300, BOOST_OPEN);
	ck(b.open && boost_lift(&b) == BOOST_BRIGHT_CELLS, "brightness opens the same way and stops at three steps");

	if (fails) { printf("boost: %d FAILED\n", fails); return 1; }
	printf("boost: ok\n");
	return 0;
}
