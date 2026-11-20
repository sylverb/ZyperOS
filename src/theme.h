/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_THEME_H
#define ZYPEROS_THEME_H

/* Which theme is on: the one answer every screen asks, and the one place the
 * handheld's clock is read to give it.
 *
 * DESIGN.md has two themes, Beard (DS_LIGHT) and Salt & Pepper (DS_DARK), and
 * MENU > Appearance has three settings for choosing between them: Light,
 * Dark, and Auto, which is Salt & Pepper in Night Mode's Night Hours by the
 * handheld's clock (night.from to night.until, src/nightmode.h; 21:00 to 7:00
 * as it ships) and Beard the rest of the day. 1.0.6 took Auto out for Night
 * Mode, and 1.1.0 brought it back on Night Mode's hours, one schedule for
 * both: a player missed it on 2026-11-20. Night Mode warms the screen and
 * Auto turns the theme, each on its own setting, by the same two times. The
 * setting is what a player chose and
 * is saved; the theme is what it comes to at this minute and is not. Every
 * screen draws from theme(), and nothing but this file turns one into the
 * other.
 *
 * Plain C and no SDL, like src/sys_menu.c and src/controls.c, so that
 * tools/theme-check.c links it without a window; and no database either:
 * the caller reads the saved word and hands it to theme_init, and saves the
 * word it is given by the menu. The clock is time() and localtime_r, which is
 * what the handheld's own zone setting moves (plat_clock_zone sets TZ), so
 * Auto follows MENU > Date & Time without being told about it.
 *
 * It also keeps THEME_FILE (src/themefile.h) saying the theme that is on, for
 * Diatom and portkeys, which draw the volume and brightness bar without the
 * launcher: written at every start and at every change this process sees.
 * That is every change while it runs, a resident Diatom's games included,
 * whose wait asks the clock (on_game_tick). It is not while a port runs,
 * which the launcher exits for (launch_port), and there portkeys reads the
 * setting and the Night Hours from the database and follows Auto itself
 * (theme_saved_at). Nor is it while the fallback Diatom runs a game, which
 * the launcher waits for in plat_run with nothing asking: across an edge of
 * the Night Hours there, the bar keeps the theme the game started in
 * (DESIGN.md 14). */

#include <stdbool.h>
#include <time.h>

#include "design.h"

/* The setting, in the order the row cycles through them. Light is first and
 * the default: DESIGN.md says Beard is the theme of a new device, and it is
 * what anything that is not one of the three reads as. */
typedef enum { APPEAR_LIGHT, APPEAR_DARK, APPEAR_AUTO } appearance;
#define APPEARANCES 3

/* The theme a setting makes at an hour of the local day (0-23): DS_LIGHT or
 * DS_DARK. Pure, which is the point of it being a function of two numbers:
 * Auto's edges are what a check can hold and a person cannot be awake to see.
 *
 * Light and Dark ignore the hour. Auto is Light from the hours' light edge
 * up to but not including their dark edge, and Dark for the rest
 * (theme_set_hours). An hour outside 0-23 is a clock that could not be read,
 * and that is not night: it is Light, the default, whatever the setting is
 * called. */
int theme_resolve(appearance a, int hour);

/* Auto's hours: Light from `light_min` minutes into the local day, Dark from
 * `dark_min`, to the minute. They are Night Mode's Night Hours (1.1.0):
 * light from night.until, dark from night.from, so the caller hands in
 * (until, from) - src/nightkeep.h's night_theme_hours does, at the start and
 * at each change of them on Night Mode's page; portkeys reads the same two
 * keys. 7:00 and 21:00 as it ships, the Night Hours' own. Hours that go
 * round midnight are fine, and the same two never make it dark, as the same
 * two make Night Mode no night. Anything off the day is the default.
 *
 * THEME_LIGHT_FROM and THEME_DARK_FROM are 1.0.5's Auto Hours as they
 * shipped, 7:00 and 19:00: the first start's migration of a 1.0.5 card
 * keeps them as its Night Hours (src/nightmode.h), so its Auto turns at the
 * hours it did. THEME_HOURS_STEP is the step Night Mode's page moves them by. */
#define THEME_NIGHT_FROM  (21 * 60)
#define THEME_NIGHT_UNTIL (7 * 60)
#define THEME_LIGHT_FROM  (7 * 60)
#define THEME_DARK_FROM   (19 * 60)
#define THEME_HOURS_STEP  30
void theme_set_hours(int light_min, int dark_min);
void theme_hours(int *light_min, int *dark_min);

/* theme_resolve to the minute of the local day (0 to 1439): what Auto is at
 * `minute` by the hours set; theme_resolve is it at the hour's start */
int theme_resolve_min(appearance a, int minute);

/* The theme the saved word `saved` makes at `now`: the word read as the
 * launcher reads it (appearance_parse, so NULL, "" and a word that is none of
 * the three are Light) and Auto resolved at the local hour of `now`, in the
 * zone TZ names (theme_resolve). Pure but for the clock's zone: no state here
 * changes and no file is written.
 *
 * For a program that draws in the launcher's theme without the launcher:
 * portkeys, over a port, which the launcher exits for, so THEME_FILE is not
 * written again until it is back. With this the bar follows Auto across the
 * Night Hours' edges as the shelf would, from the setting and the hours in
 * the database (theme_set_hours first). */
int theme_saved_at(const char *saved, time_t now);

/* What the menu shows: "Light", "Dark", "Auto". A setting that is not one of
 * the three is named as the Light it acts as. */
const char *appearance_name(appearance a);

/* What the database stores, under the key `appearance`: "light", "dark",
 * "auto". Words and not numbers, so the file can be read and edited by a
 * person, and so the order of the enum is not a promise the saved data keeps. */
const char *appearance_id(appearance a);

/* The setting a word names, from the database or from the menu's capitals:
 * the whole word, in either case, and nothing else. Anything that is not one
 * of the three - a missing key, an empty one, a word from a later release -
 * is APPEAR_LIGHT, so a setting gone wrong never turns the shelf dark. */
appearance appearance_parse(const char *s);

/* The setting `d` steps from `a` (-1 or +1, or 0 for none), wrapping at both
 * ends: Light, Dark, Auto and round again. A setting that is not one of the
 * three steps from Light. */
appearance appearance_step(appearance a, int d);

/* Where THEME_FILE is written, THEME_FILE unless this says otherwise, and
 * nowhere at all for NULL: a check on a development host, where a launcher
 * may be running, must not write over the one it would be testing. The path
 * is not copied, and must outlive its use. */
void theme_set_file(const char *path);

/* Starts, or changes, the setting: resolves it for now, and writes the theme
 * file whether or not the theme changed, because a file left by an earlier
 * launcher says what that one decided. At start from the saved word, and again
 * from the menu as the row is changed, where it takes effect at once.
 *
 * A file that cannot be written is a line on stderr and nothing more: a bar
 * in the wrong theme over a game is a smaller harm than a launcher that does
 * not start, and the theme on screen is right either way. */
void theme_init(appearance a);

/* theme_init at `now`, for the check, which has to ask what Auto does at
 * 20:59 and at 21:00 and cannot wait for either. */
void theme_init_at(appearance a, time_t now);

/* Auto's clock, from the main loop: true when the theme changed since it
 * was last asked, and then the file says so too; the caller redraws.
 *
 * Turns at the second of the edge itself: each look works out when the
 * next edge of the Night Hours is (a local From or Until, through mktime as
 * night_schedule finds the nights), and a pass at or past it looks again.
 * So the theme turns on the same pass as Night Mode's warmth, which
 * plat_night_tick loads at that second too: one frame, the table loaded
 * once by Night Mode and none by the theme (1.1.0; up to 1.0.5 Auto could
 * be a minute late, alone).
 *
 * Between edges it compares one number a pass and reads the clock in full
 * at most once a minute, for what the edge cannot foresee: a clock that
 * moved back, as it does when the time is set by hand in Date & Time, is
 * looked at at once, as is one that moved forward by more than the minute.
 * Light and Dark have nothing to look at and are never true. */
bool theme_tick(void);

/* theme_tick at `now`, for the check. */
bool theme_tick_at(time_t now);

/* Reads the clock now, whatever the minute says, and answers as theme_tick
 * does. For the moment something is known to have moved under Auto: the time
 * zone chosen in Date & Time puts the same instant in another hour, and the
 * theme is to follow it at once and not at the next look. A look like any
 * other, so the minute is counted from it. Never true for Light or Dark. */
bool theme_refresh(void);

/* theme_refresh at `now`, for the check. */
bool theme_refresh_at(time_t now);

/* A theme's index as one of the two: DS_DARK for DS_DARK and DS_LIGHT for
 * anything else, the default, so an index handed in from anywhere picks a
 * table that exists. Inline and plain, so the checks that link no theme.c
 * can use it. */
static inline int theme_clamp(int ix)
{
	return ix == DS_DARK ? DS_DARK : DS_LIGHT;
}

/* The theme `ix`'s table as the player's look has it (src/look.c, which
 * defines this): the tokens' own DS_THEME until a look other than the one
 * that ships is set, then the tables look_set worked out. Every screen that
 * draws in a theme asks this or theme(), so a look reaches all of them. */
const ds_theme *theme_at(int ix);

/* The theme that is on: its table in src/design.h, and its index into
 * DS_THEME, which is DS_LIGHT or DS_DARK. Beard until theme_init is called,
 * so a screen drawn before the setting is read is the default and not a
 * crash. */
const ds_theme *theme(void);
int theme_index(void);

/* The setting that is on, which is what the row shows and the next press
 * steps from. APPEAR_LIGHT until theme_init. */
appearance theme_appearance(void);

/* Theme `ix` (DS_LIGHT or DS_DARK) on the screen for a while, said to no
 * program and kept nowhere: a list's cursor showing a ground in the theme
 * that is not on (Theme's Light Background with Dark on, src/ds/hover.h).
 * -1: the theme the setting says, back, and the file said again where it is
 * behind. theme_appearance is untouched. */
void theme_show(int ix);

#endif
