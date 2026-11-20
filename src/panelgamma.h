/* SPDX-License-Identifier: GPL-3.0-only OR MPL-2.0 */
/* The display controller's gamma table: empty, so the panel shows each
 * frame's own colors, or a dimming one, for the two brightness steps under
 * the backlight's floor.
 *
 * The GKD Pixel 2's display controller (the PX30's VOP) has a 256-entry
 * table per channel between the frame and the panel, and plastron's splash
 * filled it at every boot up to ZyperOS 1.0.4: red and green as in^1.5, blue
 * as it was, a correction set by eye against a TrimUI Brick. It darkened
 * every game and the shelf alike, and took the warmth out of them: a mid
 * grey (128,128,128) reached the panel as (91,91,134), a warm sand
 * (216,184,120) as (199,156,133). The stock firmware fills no table, so the
 * same game looked brighter and warmer there, which is how a player found
 * it (SNES and PS1, 2026-11-12). The splash no longer fills it; this empties
 * the table an older system image's splash filled, since System Update over
 * Wi-Fi brings a new ZyperOS folder to a card whose system image may be
 * older.
 *
 * Under the floor (1.0.6): the backlight goes no lower than its value 17 - the
 * device tree gives 1 to 16 the same output, and a longer PWM period that
 * would have let the duty go lower left the panel black under 17 (Tarik,
 * 2026-11-15) - so the two bottom brightness steps, sysfs values 9 and 13,
 * keep the backlight there and take the light down with this table: every
 * channel's code value times (raw/17)^(1/2.2), so the light, which goes
 * about as the code value to the 2.2, is raw/17 of the floor's - 53% and 76%,
 * the ladder's own ratio a step. The value in sysfs is the step's name: a
 * program that reads 9 there knows the table for 9 belongs on the display.
 *
 * Screen Colors (1.0.6, MENU > Display): the player's Temperature, Tint,
 * Contrast and Gamma go in the same table, per channel, over the dimming one.
 * All 0 is Standard, which is every frame's own colors and, from the floor
 * up, no table at all, as before. They are opt-in because the 1.0.4
 * correction above was not: colors change only when the player asks. The
 * launcher keeps them in its database and writes them to
 * PANELGAMMA_COLOR_FILE for Diatom and portkeys, which write the table too.
 *
 * The launcher calls panelgamma_apply when it takes the display from the
 * splash and at each brightness step; Diatom when it takes the display (it
 * can come first, with a game resumed at boot) and at each step; portkeys at
 * each step over a port, on the port's display it borrows for its bar.
 *
 * The kernel's own ioctls, with the numbers and structures written out here
 * rather than taken from <drm/drm.h> or libdrm: the launcher links no libdrm,
 * the structures are the kernel's fixed ABI, and the check
 * (tools/panelgamma-check.c) builds on a computer without Linux's headers.
 *
 * Diatom carries a copy, diatom/port/panelgamma.c and .h, checked against
 * these by diatom/tools/check-copies.py. */
#ifndef PANELGAMMA_H
#define PANELGAMMA_H

/* The backlight's floor, sysfs: from here up the backlight itself makes the
 * light, and no table is wanted */
#define PANELGAMMA_FLOOR 17

/* ioctl(2) as these call it; a check puts a stand-in display here. */
typedef int (*panelgamma_ioctl_fn)(int fd, unsigned long request, void *arg);
extern panelgamma_ioctl_fn panelgamma_ioctl;

/* The table's output (0..255) for input code value `in` (0..255) at backlight
 * value `raw`: `in` itself at the floor and above, and for 0 (the light off);
 * under the floor, in * (raw/17)^(1/2.2), rounded. */
int panelgamma_out(int raw, int in);

/* The player's Screen Colors as the table carries them, each
 * -PANELGAMMA_RANGE..PANELGAMMA_RANGE with 0 the frame as drawn. All 0 is
 * Standard. Saturation is not here: the table maps each channel by itself,
 * and saturation mixes them (the VOP's BCSH block does it). */
typedef struct {
	int temp;      /* Temperature: below 0 cooler (bluer), above warmer */
	int tint;      /* Tint: below 0 greener, above more magenta */
	int contrast;  /* Contrast: below 0 flatter, above stronger */
	int gamma;     /* Gamma: below 0 darker mid-tones, above lighter */
	/* Night Mode's warmth on the display now (1.0.6, MENU > Display >
	 * Night Mode, src/nightmode.h): 0 none, up to PANELGAMMA_NIGHT_MAX.
	 * Its gains are Temperature's at +night, multiplied after the
	 * player's, so it warms whatever the player set and never clips
	 * against their Temperature: +5 with Night 8 takes blue to 0.725 x
	 * 0.56 = 0.41 of its light, warmer than either alone. Never in the
	 * file: a reader works it out from the nights there (panelgamma_night). */
	int night;
	/* The brightness boost past the top rung (src/boost.h, DESIGN.md 11.6):
	 * 0 none, up to PANELGAMMA_LIFT_MAX. The top rung is the backlight's
	 * own maximum, so a step is a lift of the mid-tones, like Gamma +3,
	 * applied after the player's colors: x^0.88, x^0.76, x^0.64, black and
	 * white kept. Never in the file: each of the three that load the table
	 * keeps it with the level (the launcher, Diatom, portkeys). */
	int lift;
} panelgamma_color;

#define PANELGAMMA_RANGE 10
#define PANELGAMMA_LIFT_MAX 3
#define PANELGAMMA_NIGHT_MAX 10

/* Where the launcher leaves them for Diatom and portkeys: a fact about this
 * boot, as /tmp/zyperos-theme is (src/themefile.h); the settings are the
 * launcher's, written here at its start and at every change. One line,
 * "temp=4 tint=0 contrast=3 gamma=-2", and with Night Mode on its warmth
 * and when: " warmth=8 nights=always", or the next nights as seconds since
 * 1970, " warmth=8 nights=1763434800-1763478000,1763521200-1763564400". */
#define PANELGAMMA_COLOR_FILE "/tmp/zyperos-screen"

/* Night Mode as the file carries it, for the two that load the table
 * without the launcher: Diatom over a game and portkeys over a port, which
 * look at the clock and load the table at the night's start and end. The
 * nights are UTC seconds, which the launcher works out in its own time zone
 * (src/nightmode.c), so neither needs one. Three: the launcher writes them
 * again at each night's end, and a port played two days with the launcher
 * stopped still has its nights. */
#define PANELGAMMA_NIGHTS 3
typedef struct {
	int warmth;                       /* 1..PANELGAMMA_NIGHT_MAX; 0 is off */
	int always;                       /* On: at every moment */
	int n;                            /* By Hours: how many nights */
	long long from[PANELGAMMA_NIGHTS], until[PANELGAMMA_NIGHTS];   /* [from, until) */
} panelgamma_night;

/* The warmth n puts on the display at `now` (seconds since 1970): its
 * warmth always, or inside one of its nights; else 0. NULL is 0. */
int panelgamma_night_at(const panelgamma_night *n, long long now);

/* The first moment after `now` that panelgamma_night_at's answer can
 * change: the next night's start or end. 0 when none is known (off, always,
 * or past the last night). */
long long panelgamma_night_next(const panelgamma_night *n, long long now);

/* Whether c (NULL too) changes nothing */
int panelgamma_color_standard(const panelgamma_color *c);

/* c's values put in range */
void panelgamma_color_clamp(panelgamma_color *c);

/* Read path into c, and its Night Mode into n (NULL: not wanted): Standard
 * and no night when the file is missing or says nothing it knows (a cold
 * boot before the launcher, a port started from the boot). c->night is 0:
 * the caller sets it from n and the clock. A word it does not know is
 * skipped, as an older reader skips warmth= and nights=. Returns 1 when it
 * read the file. */
int panelgamma_screen_read(const char *path, panelgamma_color *c, panelgamma_night *n);
int panelgamma_color_read(const char *path, panelgamma_color *c);

/* Write c (its night ignored) and n (NULL: none) to path, through a
 * temporary beside it and a rename, so a reader sees the old line or the
 * new. Returns 0 on success. */
int panelgamma_screen_write(const char *path, const panelgamma_color *c, const panelgamma_night *n);
int panelgamma_color_write(const char *path, const panelgamma_color *c);

/* Channel `ch` (0 red, 1 green, 2 blue) of the table for raw and c: Gamma
 * (a power) and Contrast (an S about mid grey) on the code value, then
 * Temperature's, Tint's, Night Mode's and the dimming's gains in light (the
 * code value to the 2.2). Black stays black, and white keeps its brightest
 * channel at 255.
 * Standard is panelgamma_out. */
int panelgamma_color_out(int raw, const panelgamma_color *c, int ch, int in);

/* Every CRTC's GAMMA_LUT as backlight value `raw` wants it: empty from the
 * floor up, the dimming table under it. Needs DRM master on fd. A CRTC that
 * already holds what is wanted is left alone: loading or emptying a table
 * makes the controller show a frame with none and wait a refresh. Returns
 * how many it changed: 0 when none needed it, or the kernel has no
 * GAMMA_LUT (QEMU's display, a kernel without plastron's patch 7); -1 when
 * asking or setting failed. */
int panelgamma_apply(int fd, int raw);

/* The same with the player's Screen Colors: a table at every backlight
 * value when c is not Standard. NULL is Standard. */
int panelgamma_apply_color(int fd, int raw, const panelgamma_color *c);

/* Every table emptied: panelgamma_apply at the floor. */
int panelgamma_clear(int fd);

#endif
