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
 * Screen Colors (1.0.6, MENU > Display): the player's Night Light, Contrast
 * and the Vivid look's deeper shadows go in the same table, per channel, over
 * the dimming one. All off is Standard, which is every frame's own colors
 * and, from the floor up, no table at all, as before. They are opt-in
 * because the 1.0.4 correction above was not: colors change only when the
 * player asks. The launcher keeps them in its database and writes them to
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

/* The player's Screen Colors as the table carries them. All 0 is Standard.
 * Saturation is not here: the table maps each channel by itself, and
 * saturation mixes them (the VOP's BCSH block does it). */
typedef struct {
	int night;     /* Night Light: 0 off, 1 to PANELGAMMA_NIGHT_MAX warmer */
	int contrast;  /* Contrast: 0 normal, 1 to PANELGAMMA_CONTRAST_MAX more */
	int vivid;     /* the Vivid look's deeper shadows: 0 or 1 */
} panelgamma_color;

#define PANELGAMMA_NIGHT_MAX 5
#define PANELGAMMA_CONTRAST_MAX 2

/* Where the launcher leaves them for Diatom and portkeys: a fact about this
 * boot, as /tmp/zyperos-theme is (src/themefile.h); the settings are the
 * launcher's, written here at its start and at every change. One line,
 * "night=3 contrast=1 vivid=0". */
#define PANELGAMMA_COLOR_FILE "/tmp/zyperos-screen"

/* Whether c (NULL too) changes nothing */
int panelgamma_color_standard(const panelgamma_color *c);

/* c's values put in range */
void panelgamma_color_clamp(panelgamma_color *c);

/* Read path into c: Standard when the file is missing or says nothing it
 * knows (a cold boot before the launcher, a port started from the boot).
 * Returns 1 when it read the file. */
int panelgamma_color_read(const char *path, panelgamma_color *c);

/* Write c to path, through a temporary beside it and a rename, so a reader
 * sees the old line or the new. Returns 0 on success. */
int panelgamma_color_write(const char *path, const panelgamma_color *c);

/* Channel `ch` (0 red, 1 green, 2 blue) of the table for raw and c: the
 * contrast curve and Vivid's toe on the code value, then Night Light's and
 * the dimming's gains in light (the code value to the 2.2). Standard is
 * panelgamma_out. */
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
