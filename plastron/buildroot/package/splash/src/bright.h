/* SPDX-License-Identifier: GPL-3.0-only */
/* The backlight at the player's brightness from the first light: ZyperOS keeps
 * the backlight's own value at the player's rung in theme.txt on the boot
 * partition (its src/boottheme.h), boot.scr puts it on the kernel's line as
 * zybright, and the kernel hands it to init's environment, where the splash
 * reads it before it lights the panel. Pure, so ZyperOS's check-splashbright
 * holds it. */
#ifndef SPLASH_BRIGHT_H
#define SPLASH_BRIGHT_H

/* `word` as a backlight value for a backlight of `max`, -1 for none to set:
 * no word, "-" (none was ever kept, so the kernel's own level stays), 0 (no
 * light at all), a leading zero, anything but digits, or above `max` */
static inline int splash_bright_value(const char *word, int max)
{
	const char *p;
	long v = 0;

	if (!word || !*word || *word == '0' || max < 1) return -1;
	for (p = word; *p; p++) {
		if (*p < '0' || *p > '9') return -1;
		v = v * 10 + (*p - '0');
		if (v > max) return -1;
	}
	return (int)v;
}

#endif
