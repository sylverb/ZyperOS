/* SPDX-License-Identifier: GPL-3.0-only */
/* The launcher's level keys (src/levels.c) as the screens call them, the
 * device, the database, MP4 and the port's file stubbed: what the volume
 * keys do to the boost past the top on each kind of screen, what the bar
 * shows of it, and the gain MP4 is told.
 *
 * The case it holds: music or a video's sound from MP4 heard behind the home
 * (or a shelf, a menu, the in-game menu: every launcher screen but MP4's own
 * says BOOST_HIDE). There the boost is MP4's, as on its own screens - the
 * bar shows the cells, a hold or three presses on the top open them, VOL-
 * empties one before the level moves, and each cell reaches MP4 - where the
 * home showed the bar with no cells and a press down closed the whole boost
 * and took a step too (2026-11-20, on 1.1.0-rc.3: "if i'm playing something
 * that has the software audio boost, i should be able to still control that
 * audio boost from home"). With nothing heard, the menus' own: the bar as it
 * was, nothing opens, a press under the top folds a kept boost. MP4's lock
 * screen never opens one, heard or not.
 *
 * And the two beside it (the same day): the in-game menu over a paused game
 * is the game's place, so VOL- there empties one cell, as in the game, where
 * it threw the game's whole boost away in one press; and MP4 behind a
 * running game hears each step of the game's boost at once
 * (levels_game_boost, from Diatom's LEVEL lines: tools/resident-check.c),
 * not when the game pauses or ends.
 *
 *   levels-check     checks
 *
 * SDL's headers for platform.h's types; nothing of SDL is called. */
#include "../src/levels.h"

#include "../src/db.h"
#include "../src/eq.h"
#include "../src/keyrepeat.h"
#include "../src/mp4c.h"
#include "../src/rumble.h"

#include <stdio.h>
#include <string.h>

static int fails;

static void ck(int cond, const char *what)
{
	printf("  %s %s\n", cond ? "ok  " : "FAIL", what);
	if (!cond) fails++;
}

/* ---- the neighbours, stubbed ------------------------------------------- */

static unsigned clock_ms = 1000;
static int vol = PLAT_VOL_MAX, bri = PLAT_BRIGHT_MAX;
static int kept_boost[2];          /* volume_boost, brightness_boost */
static int mp4_gain = -1, port_gain = -1, lift = -1, buzzes;
static bool heard;                 /* MP4 playing, or asked to */

unsigned plat_now_ms(void) { return clock_ms; }

/* platform.c's, on this clock */
bool in_repeat(in_state *st, in_button b)
{
	if (st->pressed[b]) return true;
	if (!st->down[b]) return false;
	if (clock_ms - st->down_since[b] < REPEAT_DELAY_MS) return false;
	if (clock_ms - st->last_repeat[b] < REPEAT_RATE_MS) return false;
	st->last_repeat[b] = clock_ms;
	return true;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int  plat_volume_get(void) { return vol; }
int  plat_brightness_get(void) { return bri; }
void plat_volume_nudge(int delta) { vol = clampi(vol + delta, 0, PLAT_VOL_MAX); }
void plat_brightness_nudge(int delta) { bri = clampi(bri + delta, 0, PLAT_BRIGHT_MAX); }
void plat_osd_show(int kind, int val, int max) { (void)kind; (void)val; (void)max; }
void plat_brightness_lift(int l) { lift = l; }
bool plat_rumble_buzz(float scale, int ms) { (void)scale; (void)ms; buzzes++; return true; }

static int db_one;
db  *db_dev(void) { return (db *)&db_one; }
int  db_get_int(db *d, const char *key, int fallback)
{
	(void)d;
	return !strcmp(key, "volume_boost") ? kept_boost[0] : !strcmp(key, "brightness_boost") ? kept_boost[1]
	     : fallback;
}
bool db_set_int(db *d, const char *key, int val)
{
	(void)d;
	if (!strcmp(key, "volume_boost")) kept_boost[0] = val;
	else if (!strcmp(key, "brightness_boost")) kept_boost[1] = val;
	return true;
}

bool eq_share_gain(int db) { port_gain = db; return true; }
void mp4c_gain(int db) { mp4_gain = db; }
bool mp4c_heard(void) { return heard; }
rumble_level rumble_menu(void) { return RUMBLE_MEDIUM; }
int rumble_level_pct(rumble_level l) { return l == RUMBLE_OFF ? 0 : 60; }

/* ---- a screen's frames -------------------------------------------------- */

static in_state in;

/* One pass of a screen's loop at `where`: the keys as they are now, then
 * 16 ms on, this frame's presses gone */
static void frame(int where)
{
	levels_keys(&in, where);
	memset(in.pressed, 0, sizeof in.pressed);
	clock_ms += 16;
}

static void idle(int where, unsigned ms)
{
	for (unsigned t = 0; t < ms; t += 16) frame(where);
}

static void down(in_button b)
{
	in.down[b] = in.pressed[b] = true;
	in.down_since[b] = clock_ms;
	in.last_repeat[b] = 0;
}

/* A press and its let-go, then 200 ms at that screen */
static void press(in_button b, int where)
{
	down(b);
	frame(where);
	in.down[b] = false;
	idle(where, 200);
}

/* Held `ms`, repeating as the launcher's keys do, then let go */
static void hold(in_button b, unsigned ms, int where)
{
	down(b);
	idle(where, ms);
	in.down[b] = false;
	idle(where, 200);
}

static osdbar_look look(void)
{
	osdbar_look l;

	levels_look(OSDBAR_VOLUME, &l);
	return l;
}

static int cells_shown(const osdbar_look *l)
{
	int n = 0;

	for (int i = 0; i < l->ncells; i++) n += l->fill[i] > 0.99f;
	return n;
}

/* A fresh start: the volume at `level`, `kept` cells kept from before */
static void start(int level, int kept)
{
	memset(&in, 0, sizeof in);
	clock_ms += 5000;
	vol = level;
	kept_boost[0] = kept;
	mp4_gain = port_gain = -1;
	buzzes = 0;
	levels_game_paused(false);
	levels_init();
	idle(BOOST_HIDE, 100);
}

int main(void)
{
	osdbar_look l;

	/* MP4 heard behind the home, a boost of three cells set in the player */
	heard = true;
	start(PLAT_VOL_MAX, 3);
	ck(mp4_gain == 6 && port_gain == 6, "MP4 heard on the home: the boost set while playing stays, +6 dB told");
	l = look();
	ck(l.ncells == BOOST_VOL_CELLS && cells_shown(&l) == 3,
	   "the home's bar shows the boost cells, three filled");
	press(IN_VOLDN, BOOST_HIDE);
	ck(vol == PLAT_VOL_MAX && levels_boost(OSDBAR_VOLUME) == 2,
	   "VOL- on the home empties one cell, the level stays at the top");
	ck(mp4_gain == 4 && port_gain == 4 && kept_boost[0] == 2, "and MP4 is told +4 dB, the two kept");
	press(IN_VOLUP, BOOST_HIDE);
	press(IN_VOLUP, BOOST_HIDE);
	ck(levels_boost(OSDBAR_VOLUME) == 4 && mp4_gain == 8, "VOL+ on the home fills the next cells, +8 dB to MP4");
	for (int i = 0; i < 4; i++) press(IN_VOLDN, BOOST_HIDE);
	ck(vol == PLAT_VOL_MAX && levels_boost(OSDBAR_VOLUME) == 0 && mp4_gain == 0,
	   "four presses down empty it; it waits at the top, MP4 back to 0 dB");
	press(IN_VOLDN, BOOST_HIDE);
	ck(vol == PLAT_VOL_MAX - 1, "the next press goes under the top");

	/* opened on the home, the player's way */
	start(PLAT_VOL_MAX, 0);
	for (int i = 0; i < BOOST_TAPS; i++) press(IN_VOLUP, BOOST_HIDE);
	ck(levels_boost(OSDBAR_VOLUME) == 1 && mp4_gain == 2 && buzzes == 1,
	   "three presses on the top of the home open it, +2 dB to MP4, the rumble felt");
	start(PLAT_VOL_MAX, 0);
	hold(IN_VOLUP, BOOST_HOLD_MS + 100, BOOST_HIDE);
	ck(levels_boost(OSDBAR_VOLUME) == 1 && mp4_gain == 2, "VOL+ held a second on the top of the home opens it");
	press(IN_VOLUP, BOOST_HIDE);
	ck(kept_boost[0] == 2, "a cell filled on the home is kept like the level");

	/* from the player to the home with the bar still up: the same cells */
	start(PLAT_VOL_MAX, 2);
	press(IN_VOLUP, BOOST_OPEN);
	l = look();
	ck(cells_shown(&l) == 3, "Now Playing's press fills a third cell");
	frame(BOOST_HIDE);
	l = look();
	ck(cells_shown(&l) == 3, "back on the home, the bar keeps them");

	/* MP4's lock screen never opens one, heard or not */
	start(PLAT_VOL_MAX, 0);
	hold(IN_VOLUP, 3000, BOOST_FILL);
	for (int i = 0; i < BOOST_TAPS; i++) press(IN_VOLUP, BOOST_FILL);
	ck(levels_boost(OSDBAR_VOLUME) == 0 && mp4_gain == 0, "MP4's lock screen still never opens one");

	/* nothing heard (MP4 paused or stopped: the menu music's again): the
	 * menus' own, as before */
	heard = false;
	start(PLAT_VOL_MAX, 3);
	l = look();
	ck(l.ncells == 0, "nothing heard: the home's bar is the bar as it was");
	press(IN_VOLUP, BOOST_HIDE);
	ck(levels_boost(OSDBAR_VOLUME) == 3, "a press at the top changes nothing");
	press(IN_VOLDN, BOOST_HIDE);
	ck(vol == PLAT_VOL_MAX - 1 && levels_boost(OSDBAR_VOLUME) == 0 && mp4_gain == 0 && kept_boost[0] == 0,
	   "a press down goes under the top and folds the kept boost");
	start(PLAT_VOL_MAX, 0);
	hold(IN_VOLUP, 3000, BOOST_HIDE);
	for (int i = 0; i < BOOST_TAPS; i++) press(IN_VOLUP, BOOST_HIDE);
	ck(levels_boost(OSDBAR_VOLUME) == 0, "and nothing opens there");

	/* MP4's own screens open it heard or not, as before */
	start(PLAT_VOL_MAX, 0);
	for (int i = 0; i < BOOST_TAPS; i++) press(IN_VOLUP, BOOST_OPEN);
	ck(levels_boost(OSDBAR_VOLUME) == 1 && mp4_gain == 2, "MP4's own screens open it with nothing heard");

	/* the in-game menu over a paused game, nothing heard: the game's place */
	heard = false;
	start(PLAT_VOL_MAX, 0);
	levels_from_game(OSDBAR_VOLUME, PLAT_VOL_MAX, 3);   /* PAUSED: the game's three cells */
	levels_game_paused(true);
	idle(BOOST_HIDE, 100);
	l = look();
	ck(cells_shown(&l) == 3, "the in-game menu's bar shows the game's three cells");
	press(IN_VOLDN, BOOST_HIDE);
	ck(vol == PLAT_VOL_MAX && levels_boost(OSDBAR_VOLUME) == 2,
	   "VOL- in the in-game menu empties one cell, as in the game; the level stays at the top");
	ck(kept_boost[0] == 2 && mp4_gain == 4, "  ...two kept, the two RESUME hands the game (levels_boost)");
	press(IN_VOLUP, BOOST_HIDE);
	ck(levels_boost(OSDBAR_VOLUME) == 3, "VOL+ there fills one again");
	start(PLAT_VOL_MAX, 0);
	levels_game_paused(true);
	for (int i = 0; i < BOOST_TAPS; i++) press(IN_VOLUP, BOOST_HIDE);
	ck(levels_boost(OSDBAR_VOLUME) == 1, "three presses on the top of the in-game menu open it");
	levels_game_paused(false);
	press(IN_VOLDN, BOOST_HIDE);
	ck(vol == PLAT_VOL_MAX - 1 && levels_boost(OSDBAR_VOLUME) == 0,
	   "the game over, the home with nothing heard is the menus' own again");

	/* MP4 behind a running game: each step of its boost said to MP4 at once */
	heard = true;
	start(PLAT_VOL_MAX, 0);
	levels_game_boost(OSDBAR_VOLUME, 1);
	ck(mp4_gain == 2, "a game's hold opens its boost: MP4 at +2 dB at once");
	levels_game_boost(OSDBAR_VOLUME, 2);
	levels_game_boost(OSDBAR_VOLUME, 3);
	ck(mp4_gain == 6, "each press in the game: +4, then +6 dB");
	levels_game_boost(OSDBAR_VOLUME, 2);
	ck(mp4_gain == 4, "VOL- in the game: +4 dB");
	ck(kept_boost[0] == 0 && levels_boost(OSDBAR_VOLUME) == 0 && port_gain == 0,
	   "  ...nothing kept and the ports' file left until the game pauses or ends");
	levels_game_boost(OSDBAR_BRIGHT, 2);
	ck(mp4_gain == 4, "the brightness's is Diatom's own: MP4 not told");
	levels_from_game(OSDBAR_VOLUME, PLAT_VOL_MAX, 2);
	ck(kept_boost[0] == 2 && mp4_gain == 4 && port_gain == 4, "the end hands the two back: kept, the same gain");

	if (fails) { printf("levels: %d FAILED\n", fails); return 1; }
	printf("levels: ok\n");
	return 0;
}
