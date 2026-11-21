/* SPDX-License-Identifier: GPL-3.0-only OR MPL-2.0 */
#ifndef ZYPEROS_BOOST_H
#define ZYPEROS_BOOST_H

/* The boost past the top of the volume and the brightness (DESIGN.md 11.6):
 * what a press, a held key and the clock do to it, and what the bar shows of
 * it at any moment.
 *
 * Some films and games are quiet even at the top of the volume. At the top,
 * a level key held BOOST_HOLD_MS more, or pressed BOOST_TAPS more times with
 * no more than BOOST_GAP_MS between two presses, opens cells past the top:
 * each volume cell BOOST_DB_CELL dB of software gain (src/eqdsp.h, with a
 * limiter that keeps every peak under full scale), each brightness cell a
 * lift of the mid-tones in the display's gamma table (src/panelgamma.h),
 * since the top rung is already the backlight's own maximum. While it
 * charges, the bar's end pushes out by one big cell that fills: linearly
 * with a hold, a third for each press, and drains back the same way when the
 * key is let go early or the presses stop. Full, it settles to a cell's size
 * as the others spawn, the rim flashes, and a short rumble is felt. A key
 * still held stops there: each further cell is a press. Going down empties
 * the cells one by one; at the top they wait, empty; one more press goes
 * under the top and folds them away. An empty boost closes with the bar; a
 * filled one stays, and is kept as the level is (my picks in the volume
 * boost lab, 2026-11-19).
 *
 * Pure: no clock of its own, no input, no I/O. The launcher (src/levels.c),
 * Diatom (a copy, diatom/port/boost.h) and portkeys each keep a boost_bar
 * per level, say what their keys do and give it the time, and draw the
 * osdbar_look it gives back; tools/boost-check.c holds it on the host. */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "design.h"
#include "osdbar.h"

#define BOOST_VOL_CELLS    6                 /* volume cells past the top...         */
#define BOOST_DB_CELL      3                 /* ...each this many dB of gain (2 until
                                               * 2026-11-20: "i believe there is more
                                               * room for software audio boost. make
                                               * each step a little higher db boost") */
#define BOOST_BRIGHT_CELLS 3                 /* brightness cells, each a lift step  */
#define BOOST_TAPS         3                 /* presses on the top that open it...  */
#define BOOST_GAP_MS       DS_MS_TAP_GAP     /* ...each within this of the last     */
#define BOOST_HOLD_MS      DS_MS_HOLD_BOOST  /* or a key held on the top this long  */

/* Where the keys are pressed, which decides what the boost may do there:
 * OPEN where it opens and fills (MP4, a game, a port; brightness anywhere),
 * FILL where an open one fills and empties but none opens (MP4's lock
 * screen: a held key in a pocket must not open it), HIDE where the bar is
 * the bar as it was, a press at the top does nothing more and a press under
 * the top folds a kept boost away (the menus, with nothing heard behind
 * them: the launcher takes a menu over MP4 playing, or over a paused game,
 * as OPEN, src/levels.c). */
enum { BOOST_OPEN, BOOST_FILL, BOOST_HIDE };

/* What a press or the clock did, for the host to act on */
enum {
	BOOST_LEVEL  = 1,   /* the level moved: apply it, keep it */
	BOOST_GAIN   = 2,   /* the cells filled moved: apply the gain or the lift, keep it */
	BOOST_OPENED = 4,   /* it opened: the rumble */
};

enum { BOOST_EASE_OUT, BOOST_EASE_IN, BOOST_EASE_LINEAR };

typedef struct {
	float a, b;          /* from, to */
	uint32_t t0, ms;     /* start, length */
	int ease;
} boost_tween;

typedef struct {
	int level, max;      /* the level, 0..max */
	int ncells;          /* boost cells this bar has; 0, no boost */
	int cells;           /* filled, while open */
	bool open;
	int taps;            /* presses on the top in a row, while closed */
	uint32_t tap_at;     /* the last one */
	bool holding;        /* a key going up is held on the top... */
	uint32_t ceil_at;    /* ...since */
	bool stopped;        /* a held key past the opening: its repeats do nothing */
	boost_tween charge;  /* the presses' part of the charge */
	boost_tween hold;    /* the held key's part: 0..1 over BOOST_HOLD_MS */
	boost_tween grow, morph, tick, mark, cut;
	boost_tween fill[OSDBAR_BOOST_MAX];
	uint32_t flash_at, push_at;
	bool flash_on, push_on;
} boost_bar;

/* ---- tweens: retargeted from wherever they are, so nothing queues ------- */

static inline float boost_ease(int e, float t)
{
	t = t < 0 ? 0 : (t > 1 ? 1 : t);
	return e == BOOST_EASE_OUT ? ds_out(t) : e == BOOST_EASE_IN ? ds_in(t) : t;
}

static inline float boost_at(const boost_tween *w, uint32_t now)
{
	int32_t d = (int32_t)(now - w->t0);

	if (d <= 0 || w->ms == 0) return d <= 0 && w->ms ? w->a : w->b;
	if ((uint32_t)d >= w->ms) return w->b;
	return w->a + (w->b - w->a) * boost_ease(w->ease, (float)d / w->ms);
}

static inline void boost_to(boost_tween *w, float b, uint32_t now, uint32_t ms, int ease)
{
	float cur = boost_at(w, now);

	if (w->b == b && cur == b) return;
	w->a = cur;
	w->b = b;
	w->t0 = now;
	w->ms = ms;
	w->ease = ease;
}

static inline void boost_put(boost_tween *w, float v)
{
	w->a = w->b = v;
	w->t0 = 0;
	w->ms = 0;
}

static inline bool boost_still(const boost_tween *w, uint32_t now)
{
	return w->ms == 0 || (int32_t)(now - w->t0) >= (int32_t)w->ms;
}

/* ---- the state -------------------------------------------------------- */

/* Closed at once: nothing moves, nothing charges */
static inline void boost_shut(boost_bar *b)
{
	int i;

	b->open = false;
	b->cells = 0;
	b->taps = 0;
	b->holding = false;
	b->stopped = false;
	boost_put(&b->charge, 0);
	boost_put(&b->hold, 0);
	boost_put(&b->grow, 0);
	boost_put(&b->morph, 0);
	boost_put(&b->tick, 0);
	for (i = 0; i < OSDBAR_BOOST_MAX; i++) boost_put(&b->fill[i], 0);
	b->flash_on = b->push_on = false;
}

/* Open at once with `cells` filled, at the top: a boost kept from before */
static inline void boost_open_at(boost_bar *b, int cells)
{
	int i;

	boost_shut(b);
	if (b->ncells <= 0 || cells <= 0) return;
	if (cells > b->ncells) cells = b->ncells;
	b->level = b->max;
	b->open = true;
	b->cells = cells;
	boost_put(&b->grow, 1);
	boost_put(&b->morph, 1);
	boost_put(&b->tick, 1);
	for (i = 0; i < OSDBAR_BOOST_MAX; i++) boost_put(&b->fill[i], i < cells ? 1 : 0);
	boost_put(&b->mark, 1);
}

/* A bar at `level` of `max` with `ncells` boost cells (0 for none), `kept`
 * of them filled from before - which counts only at the top */
static inline void boost_init(boost_bar *b, int level, int max, int ncells, int kept)
{
	memset(b, 0, sizeof *b);
	b->max = max > 0 ? max : 1;
	b->level = level < 0 ? 0 : (level > b->max ? b->max : level);
	b->ncells = ncells < 0 ? 0 : (ncells > OSDBAR_BOOST_MAX ? OSDBAR_BOOST_MAX : ncells);
	boost_shut(b);
	boost_put(&b->mark, (float)b->level / b->max);
	boost_put(&b->cut, b->level == 0 ? 1 : 0);
	if (kept > 0 && b->level == b->max) boost_open_at(b, kept);
}

/* The level set from elsewhere (a game's level coming back, the jack): a
 * level under the top closes the boost */
static inline void boost_set_level(boost_bar *b, int level)
{
	b->level = level < 0 ? 0 : (level > b->max ? b->max : level);
	if (b->level < b->max && b->open) boost_shut(b);
}

/* The charge as the bar draws it: the presses' part, the hold filling what
 * is left of it */
static inline float boost_charge(const boost_bar *b, uint32_t now)
{
	return boost_at(&b->charge, now) + (1 - b->charge.b) * boost_at(&b->hold, now);
}

static inline void boost_open(boost_bar *b, uint32_t now)
{
	int i;

	b->open = true;
	b->taps = 0;
	b->cells = 1;
	b->holding = false;
	boost_put(&b->charge, 0);
	boost_put(&b->hold, 0);
	/* the full charging cell is the first cell: it settles to a cell's size
	 * while the others come out of the end */
	boost_put(&b->fill[0], 1);
	for (i = 1; i < OSDBAR_BOOST_MAX; i++) boost_put(&b->fill[i], 0);
	boost_put(&b->morph, 0);
	boost_to(&b->morph, 1, now, DS_MS_BOOST_OPEN, BOOST_EASE_OUT);
	boost_put(&b->grow, 0);
	boost_to(&b->grow, 1, now, DS_MS_BOOST_OPEN, BOOST_EASE_OUT);
	boost_to(&b->tick, 1, now, DS_MS_QUICK, BOOST_EASE_OUT);
	b->flash_at = now;
	b->flash_on = true;
}

static inline void boost_fold(boost_bar *b, uint32_t now)
{
	int i;

	b->open = false;
	b->cells = 0;
	b->taps = 0;
	for (i = 0; i < OSDBAR_BOOST_MAX; i++) boost_to(&b->fill[i], 0, now, DS_MS_EXIT, BOOST_EASE_IN);
	boost_to(&b->grow, 0, now, DS_MS_EXIT, BOOST_EASE_IN);
	boost_to(&b->tick, 0, now, DS_MS_EXIT, BOOST_EASE_IN);
	boost_put(&b->charge, 0);
	boost_put(&b->hold, 0);
}

/* The charge drains at the speed and on the curve it filled: linearly, a
 * whole cell in BOOST_HOLD_MS */
static inline void boost_drain(boost_tween *w, uint32_t now)
{
	float v = boost_at(w, now);

	boost_to(w, 0, now, (uint32_t)(v * BOOST_HOLD_MS + 0.5f), BOOST_EASE_LINEAR);
}

/* A press (`repeat` false) or a held key's repeat of a level key, going up
 * (`dir` > 0) or down, `where` the place (BOOST_OPEN...). Returns what it
 * did (BOOST_LEVEL...). A press finding the bar at an end does nothing to it
 * (the host shows the bar anyway, as a press at the top always has). */
static inline int boost_key(boost_bar *b, int dir, bool repeat, int where, uint32_t now)
{
	int did = 0;
	bool may = b->ncells > 0 && where == BOOST_OPEN;

	if (repeat && b->stopped) return 0;
	if (dir > 0) {
		if (b->level < b->max) {
			b->level++;
			b->taps = 0;
			did |= BOOST_LEVEL;
			if (b->level == b->max) { b->holding = true; b->ceil_at = now; }
		} else if (b->open && where != BOOST_HIDE) {
			if (b->cells < b->ncells) {
				boost_to(&b->fill[b->cells], 1, now, DS_MS_QUICK, BOOST_EASE_OUT);
				b->cells++;
				did |= BOOST_GAIN;
			} else {
				b->push_at = now;
				b->push_on = true;
			}
		} else if (may && !b->open) {
			if (!repeat) {
				/* a press on the top: it counts within the gap of the last */
				if ((int32_t)(now - b->tap_at) > BOOST_GAP_MS) b->taps = 0;
				b->taps++;
				b->tap_at = now;
				b->holding = true;
				b->ceil_at = now;
				boost_put(&b->hold, 0);
				if (b->taps >= BOOST_TAPS) {
					boost_open(b, now);
					b->stopped = true;
					did |= BOOST_GAIN | BOOST_OPENED;
				} else {
					boost_to(&b->charge, (float)b->taps / BOOST_TAPS, now, DS_MS_QUICK, BOOST_EASE_OUT);
				}
			} else if (!b->holding) {
				b->holding = true;
				b->ceil_at = now;
			}
		}
	} else {
		if (b->open && b->cells > 0 && where != BOOST_HIDE) {
			b->cells--;
			boost_to(&b->fill[b->cells], 0, now, DS_MS_EXIT, BOOST_EASE_IN);
			did |= BOOST_GAIN;
		} else if (b->level > 0) {
			if (b->open) {
				if (where == BOOST_HIDE) boost_shut(b);
				else boost_fold(b, now);
				did |= BOOST_GAIN;
			}
			boost_to(&b->charge, 0, now, DS_MS_EXIT, BOOST_EASE_IN);
			boost_to(&b->hold, 0, now, DS_MS_EXIT, BOOST_EASE_IN);
			b->taps = 0;
			b->holding = false;
			b->level--;
			did |= BOOST_LEVEL;
		}
	}
	return did;
}

/* The key going up let go */
static inline void boost_release(boost_bar *b, uint32_t now)
{
	b->stopped = false;
	if (b->holding && !b->open && b->hold.b > 0) boost_drain(&b->hold, now);
	b->holding = false;
}

/* The clock, every frame the bar is up or the key is held: `up_held`, the
 * key going up is held now. A hold that has charged for BOOST_HOLD_MS opens
 * it (BOOST_OPENED); presses that stopped drain. */
static inline int boost_tick(boost_bar *b, bool up_held, int where, uint32_t now)
{
	if (b->open || b->ncells <= 0) return 0;
	if (up_held && b->holding && b->level == b->max && where == BOOST_OPEN) {
		float h = (float)(int32_t)(now - b->ceil_at) / BOOST_HOLD_MS;

		boost_put(&b->hold, h < 0 ? 0 : (h > 1 ? 1 : h));
		if (h >= 1) {
			boost_open(b, now);
			b->stopped = true;
			return BOOST_GAIN | BOOST_OPENED;
		}
	} else if (b->holding && !up_held) {
		boost_release(b, now);
	}
	if (!up_held && b->taps > 0 && (int32_t)(now - b->tap_at) > BOOST_GAP_MS) {
		b->taps = 0;
		boost_drain(&b->charge, now);
	}
	return 0;
}

/* The bar went away: an empty boost closes, a filled one stays */
static inline void boost_bar_gone(boost_bar *b)
{
	if (b->open && b->cells == 0) boost_shut(b);
	if (!b->open) {
		b->taps = 0;
		boost_put(&b->charge, 0);
		boost_put(&b->hold, 0);
	}
}

/* Whether any of it still moves: the host draws every frame until it stops */
static inline bool boost_moving(const boost_bar *b, uint32_t now)
{
	int i;

	if (b->holding && !b->open && b->ncells > 0 && b->level == b->max) return true;
	if (!boost_still(&b->charge, now) || !boost_still(&b->hold, now) || !boost_still(&b->grow, now) ||
	    !boost_still(&b->morph, now) || !boost_still(&b->tick, now) || !boost_still(&b->mark, now) ||
	    !boost_still(&b->cut, now))
		return true;
	for (i = 0; i < OSDBAR_BOOST_MAX; i++)
		if (!boost_still(&b->fill[i], now)) return true;
	if (b->flash_on && (int32_t)(now - b->flash_at) < DS_MS_BOOST_FLASH) return true;
	if (b->push_on && (int32_t)(now - b->push_at) < DS_MS_PRESS + DS_MS_EXIT) return true;
	return boost_at(&b->charge, now) > 0 || boost_at(&b->hold, now) > 0;
}

/* The look of the bar for `kind` at `now`, `where` the place (HIDE draws the
 * bar as it was). The mark follows the level in quick: a host calls this
 * after every change, so its tween is retargeted here. */
static inline void boost_look(boost_bar *b, int kind, int where, uint32_t now, osdbar_look *l)
{
	int i;
	float m = (float)b->level / b->max, cut = b->level == 0 ? 1.0f : 0.0f;

	if (b->mark.b != m) boost_to(&b->mark, m, now, DS_MS_QUICK, BOOST_EASE_OUT);
	if (b->cut.b != cut) boost_to(&b->cut, cut, now, DS_MS_QUICK, BOOST_EASE_OUT);
	memset(l, 0, sizeof *l);
	l->kind = kind;
	l->level = b->level;
	l->max = b->max;
	l->mark = boost_at(&b->mark, now);
	l->cut = boost_at(&b->cut, now);
	if (where == BOOST_HIDE || b->ncells <= 0) return;
	l->ncells = b->ncells;
	for (i = 0; i < b->ncells && i < OSDBAR_BOOST_MAX; i++) l->fill[i] = boost_at(&b->fill[i], now);
	l->grow = boost_at(&b->grow, now);
	l->tick = boost_at(&b->tick, now);
	/* the charging cell is big while closed, settles as it opens, and stays
	 * a cell's size while the pill folds away */
	l->morph = b->open ? boost_at(&b->morph, now) : (l->grow > 0.001f ? 1.0f : 0.0f);
	l->charge = b->open ? 0 : boost_charge(b, now);
	if (b->flash_on) {
		int32_t d = (int32_t)(now - b->flash_at);

		l->flash = d >= 0 && d < DS_MS_BOOST_FLASH ? 1 - (float)d / DS_MS_BOOST_FLASH : 0;
	}
	if (b->push_on) {
		int32_t d = (int32_t)(now - b->push_at);

		l->push = d < 0 ? 0 : d < DS_MS_PRESS ? ds_out((float)d / DS_MS_PRESS)
		        : d < DS_MS_PRESS + DS_MS_EXIT ? 1 - ds_in((float)(d - DS_MS_PRESS) / DS_MS_EXIT) : 0;
	}
}

/* The volume boost's gain in dB, and the brightness boost's lift step */
static inline int boost_db(const boost_bar *b) { return b->open ? b->cells * BOOST_DB_CELL : 0; }
static inline int boost_lift(const boost_bar *b) { return b->open ? b->cells : 0; }

#endif
