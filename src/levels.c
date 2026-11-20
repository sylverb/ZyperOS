/* SPDX-License-Identifier: GPL-3.0-only */
/* The launcher's level keys and the boost past the top. See src/levels.h. */
#include "levels.h"

#include "boost.h"
#include "db.h"
#include "eq.h"
#include "mp4c.h"
#include "rumble.h"

#include <stdio.h>

/* One bar per level: [0] the volume, [1] the brightness */
static struct {
	bool ready;
	bool frozen;          /* a shot: the level as it was set */
	boost_bar bar[2];
	bool up_was[2];       /* the key going up was down last frame */
	int where;            /* the volume's place, last frame */
	int db_told;          /* the gain last given to MP4 and the file */
} L = { .db_told = -1 };

static const char *const KEEP[2] = { "volume_boost", "brightness_boost" };
static const int CELLS[2] = { BOOST_VOL_CELLS, BOOST_BRIGHT_CELLS };

static int k_of(int kind) { return kind == OSDBAR_VOLUME ? 0 : kind == OSDBAR_BRIGHT ? 1 : -1; }
static int kind_of(int k) { return k == 0 ? OSDBAR_VOLUME : OSDBAR_BRIGHT; }
static int device_level(int k) { return k == 0 ? plat_volume_get() : plat_brightness_get(); }
static int device_max(int k) { return k == 0 ? PLAT_VOL_MAX : PLAT_BRIGHT_MAX; }

/* The boost's effect, wherever it goes: the volume's gain to MP4 and to the
 * file a port's plugin reads (each told only when it changes), the
 * brightness's lift to the display's table */
static void apply(int k)
{
	if (k == 0) {
		const int db = boost_db(&L.bar[0]);

		mp4c_gain(db);
		eq_share_gain(db);
		L.db_told = db;
	} else {
		plat_brightness_lift(boost_lift(&L.bar[1]));
	}
}

static void keep(int k)
{
	db_set_int(db_dev(), KEEP[k], L.bar[k].open ? L.bar[k].cells : 0);
}

/* The level as the device has it now: another path may have moved it (a
 * route change, a game handing it back), and one under the top closes the
 * boost */
static void sync(int k)
{
	const int lv = device_level(k);
	boost_bar *b = &L.bar[k];

	if (L.frozen || lv < 0 || lv == b->level) return;
	if (b->open && lv < b->max) {
		boost_set_level(b, lv);
		apply(k);
		keep(k);
	} else {
		boost_set_level(b, lv);
	}
}

void levels_init(void)
{
	int k;

	for (k = 0; k < 2; k++) {
		int lv = device_level(k);

		boost_init(&L.bar[k], lv < 0 ? device_max(k) : lv, device_max(k), CELLS[k],
		           db_get_int(db_dev(), KEEP[k], 0));
		apply(k);
	}
	L.where = BOOST_HIDE;
	L.ready = true;
}

static void ensure(void)
{
	if (!L.ready) levels_init();
}

/* What a press or the clock did, done: the level by the device's own nudge
 * (which keeps it, shows the bar and says it on the side lights, as every
 * level press always has), the boost's effect, the rumble as it opens */
static void act(int k, int dir, int did, unsigned now)
{
	(void)now;
	if ((did & BOOST_LEVEL) || !(did & BOOST_GAIN)) {
		/* a step, or a press at an end that moved nothing, as before */
		if (k == 0) plat_volume_nudge(dir);
		else plat_brightness_nudge(dir);
	} else {
		plat_osd_show(kind_of(k), L.bar[k].level, L.bar[k].max);
	}
	if (did & BOOST_GAIN) {
		apply(k);
		keep(k);
	}
	if (did & BOOST_OPENED)
		plat_rumble_buzz((float)rumble_level_pct(rumble_menu()) / 100.0f, 40);
}

bool levels_keys(in_state *in, int where)
{
	static const in_button UP[2] = { IN_VOLUP, IN_BRIGHTUP }, DN[2] = { IN_VOLDN, IN_BRIGHTDN };
	const unsigned now = plat_now_ms();
	bool acted = false;
	int k;

	ensure();
	L.where = where;
	for (k = 0; k < 2; k++) {
		boost_bar *b = &L.bar[k];
		const int wh = k == 0 ? where : BOOST_OPEN;
		int did;

		if (k == 1 && where == BOOST_FILL) break;
		sync(k);
		if (in_repeat(in, UP[k])) {
			act(k, +1, boost_key(b, +1, !in->pressed[UP[k]], wh, now), now);
			acted = true;
		}
		if (in_repeat(in, DN[k])) {
			act(k, -1, boost_key(b, -1, !in->pressed[DN[k]], wh, now), now);
			acted = true;
		}
		if (L.up_was[k] && !in->down[UP[k]]) boost_release(b, now);
		L.up_was[k] = in->down[UP[k]];
		did = boost_tick(b, in->down[UP[k]], wh, now);
		if (did) act(k, +1, did & ~BOOST_LEVEL, now);
	}
	/* A port started later reads the gain from the file; MP4, started
	 * again, has to be told again (both only on a change) */
	if (L.db_told != boost_db(&L.bar[0])) apply(0);
	else mp4c_gain(L.db_told);
	return acted;
}

bool levels_look(int kind, osdbar_look *out)
{
	const int k = k_of(kind);

	if (k < 0) return false;
	ensure();
	sync(k);
	boost_look(&L.bar[k], kind, k == 0 ? L.where : BOOST_OPEN, plat_now_ms(), out);
	return true;
}

bool levels_moving(void)
{
	const unsigned now = plat_now_ms();

	if (!L.ready) return false;
	return boost_moving(&L.bar[0], now) || boost_moving(&L.bar[1], now);
}

void levels_bar_gone(void)
{
	int k;

	if (!L.ready) return;
	for (k = 0; k < 2; k++) {
		const bool was = L.bar[k].open;

		boost_bar_gone(&L.bar[k]);
		if (was && !L.bar[k].open) {
			apply(k);
			keep(k);
		}
	}
}

void levels_output_changed(void)
{
	if (!L.ready || !L.bar[0].open) return;
	boost_shut(&L.bar[0]);
	apply(0);
	keep(0);
	fprintf(stderr, "levels: the output changed, so the volume boost is closed\n");
}

void levels_from_game(int kind, int level, int boost)
{
	const int k = k_of(kind);
	boost_bar *b;

	if (k < 0) return;
	ensure();
	b = &L.bar[k];
	if (level < 0) return;
	if (level > b->max) level = b->max;
	if (level == b->max && boost > 0) {
		if (!b->open || b->cells != boost) boost_open_at(b, boost);
	} else {
		boost_set_level(b, level);
		if (b->open) boost_shut(b);
	}
	b->level = level;
	apply(k);
	keep(k);
}

int levels_boost(int kind)
{
	const int k = k_of(kind);

	if (k < 0) return 0;
	ensure();
	return L.bar[k].open ? L.bar[k].cells : 0;
}

void levels_shot(int kind, int level, int cells)
{
	const int k = k_of(kind);

	if (k < 0) return;
	ensure();
	L.frozen = true;
	L.where = BOOST_OPEN;
	boost_init(&L.bar[k], level, L.bar[k].max, CELLS[k], cells);
}
