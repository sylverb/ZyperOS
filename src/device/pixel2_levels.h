/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_PIXEL2_LEVELS_H
#define ZYPEROS_PIXEL2_LEVELS_H

/* The GKD Pixel 2's volume and brightness, as numbers: which control, which
 * window of it the twenty-one volume positions span, and the backlight's
 * rungs; and its battery's empty voltage. Read by src/device/pixel2.c and
 * src/device/pixel2_profile.c, which are the launcher's, and by
 * tools/portkeys.c, which holds the same keys while a port runs and the
 * launcher is stopped - so a level set in Stardew Valley is the level the
 * shelf comes back at, and both turn it into the same register value.
 *
 * No SDL here, and nothing of the launcher's: portkeys links only this, the
 * arithmetic in src/audioout.c, and src/osdbar.c. */

#include "../audioout.h"

/* The RK817's "Master Playback Volume": 0-255 on both channels, NOT inverted -
 * 255 is 0 dB and 0 is -95 dB, read with amixer 2026-10-01. It has no switch,
 * so 0 is the cut. Speaker or headphones is "Playback Mux", which the system's
 * jackswitch daemon drives.
 *
 * The windows were set by ear on 2026-10-02, the way the Brick's were: a game
 * playing, the register stepped over SSH until Eric called each end.
 *
 * Speaker: full volume is comfortable and does not distort, so position 20 is
 * raw 255. The floor was walked up from the old guess (raw 140, too quiet):
 * 148 and 156 too quiet, 164 too quiet beside 172, and 172 called right for
 * position 1. About 31 dB, so about 1.6 dB a press. Players found 172 too
 * loud at night (1.0.5, Discord: "one or two steps between mute and the
 * lowest"), so since 1.0.6 the floor is two of those steps lower: position 1
 * is raw 163 and 2 is 168, both under the old lowest, 3 is about it (173),
 * and the top is as it was. About 34 dB, 1.8 dB a press, and the twenty
 * positions, their bar and the levels kept on the card all as they were.
 *
 * Headphones (Apple EarPods): walked up from quiet to the most wanted - full
 * volume was a step too far, raw 244 is the top. The floor was walked down
 * from 172, 6 dB at a time, everything too loud until raw 76. About 63 dB,
 * 3.3 dB a press; EarPods are efficient, and a less sensitive pair will want
 * a different floor, as on the Brick.
 *
 * The constants are the windows that put positions 1 and 20 exactly on those
 * raw values with aout_level_to_raw's rounding: speaker 0-97, headphones
 * 11-188, in attenuation (the speaker's 0-87 until 1.0.6).
 *
 * Kept as ATTENUATION from 255, so the shared aout_level_to_raw - written for
 * the Brick's inverted register, loud end low - lays the positions out with
 * the same rounding. Diatom's Pixel port uses the same numbers and the same
 * arithmetic: a level that crosses the socket has to mean the same thing on
 * both sides. Change one, change the other. */
#define GAIN_CTL      "Master Playback Volume"
#define GAIN_RAW_MAX  255
#define SPK_ATT_TOP   0      /* attenuation at position 20 */
#define SPK_ATT_BOTTOM 97    /* and at position 0, which the cut replaces */
#define HP_ATT_TOP    11
#define HP_ATT_BOTTOM 188
#define VOL_MAX       20     /* PLAT_VOL_MAX: 21 positions, 0..20 */

/* A USB-C DAC's volume: a software one, "USB Playback Volume" on this card,
 * which plastron's ALSA config puts in front of the DAC (its pcm.usb), with
 * the DAC's own control set to full by the system when it appears. The DAC's
 * own comes and goes with it; this one is always here, so it is written with
 * every level, the codec's too, and plugging a DAC in needs nothing re-applied.
 *
 * 0-255 over -95.625 to 0 dB: 0.375 dB a step, the codec's step, so the
 * window is in the same units as the codec's. 0 is silence.
 *
 * The window is the EarPods' headphone window, a starting guess: the ear
 * sets it with a DAC playing, as it did the codec's. Diatom's Pixel port
 * writes the same control with the same numbers. Change one, change the
 * other, and asound.conf's min_dB with them. */
#define USB_CTL        "USB Playback Volume"
#define USB_RAW_MAX    255
#define USB_DB_MIN     (-9562)   /* hundredths of a dB, as alsa-lib rounds -95.625 */
#define USB_DB_STEP    37        /* and 0.375 */
#define USB_ATT_TOP    11
#define USB_ATT_BOTTOM 188

/* sysfs, 0-255, 0 dark. The device tree's brightness-levels table sends 1 to 17
 * all to the same output (a PWM duty of 1666 of 25000 ns) and is one-to-one
 * above that, so the Brick's ladder (2, 4, 8, 16, ...) spent its bottom four
 * rungs on one brightness - reported 2026-10-02 as the bottom of the scale
 * sitting still. Driving the PWM directly the same day, 1000 ns was black, so
 * 17 is the backlight's floor: Eric wanted dimmer, but not black.
 *
 * Players wanted dimmer still at night (1.0.6, Discord), and the backlight
 * cannot give it: a test system image with a 50 us period and the table
 * one-to-one down to 6 lit 17 and went black at 13, 10, 8 and 6 (Tarik,
 * 2026-11-15). So the two bottom rungs, 9 and 13, keep the backlight at its
 * floor - 1 to 16 light it as 17 does - and the display's gamma table takes
 * the light to 9/17 and 13/17 of it (src/panelgamma.h, which the launcher,
 * Diatom and portkeys each set from the value in sysfs). The rungs are equal
 * ratios from 9 to 255, about 35% a step, since brightness reads in ratios;
 * twelve of them, as before, and the top where it was.
 *
 * Diatom's port holds the same rungs; change one, change the other. */
#define BACKLIGHT       "/sys/class/backlight/backlight/brightness"
#define BACKLIGHT_POWER "/sys/class/backlight/backlight/bl_power"
static const unsigned char bright_ladder[] __attribute__((unused)) = {
	9, 13, 17, 23, 31, 42, 57, 77, 103, 140, 189, 255
};
/* The rungs up to 1.0.5, 17 at the bottom: the positions kept on the card
 * before 1.0.6 are on these (px2_bright_from_105) */
static const unsigned char bright_ladder_105[] __attribute__((unused)) = {
	17, 22, 28, 36, 46, 58, 74, 95, 122, 156, 199, 255
};
#define BRIGHT_MAX ((int)(sizeof bright_ladder / sizeof bright_ladder[0]) - 1)

/* A position kept before 1.0.6 (on bright_ladder_105) as the position on
 * this ladder nearest its light, in ratio: at most half a step away (36 goes
 * to 31, 122 to 140), and the old bottom, 17, is the floor here: no one
 * starts 1.0.6 under the floor, on the gamma table, without choosing it. */
static inline int px2_bright_from_105(int old)
{
	int i, best = 0;
	double want, d, bestd = 1e9;

	if (old < 0) return old;
	if (old >= (int)sizeof bright_ladder_105) old = (int)sizeof bright_ladder_105 - 1;
	want = (double)bright_ladder_105[old];
	for (i = 0; i <= BRIGHT_MAX; i++) {
		d = want / bright_ladder[i];
		if (d < 1) d = 1 / d;
		if (d < bestd) { bestd = d; best = i; }
	}
	return best;
}

/* The codec's raw gain for position v, by the window of the output in use. */
static inline long px2_gain_raw(int v, int headphones)
{
	return v == 0 ? 0
	     : GAIN_RAW_MAX - aout_level_to_raw(v, VOL_MAX,
	                                        headphones ? HP_ATT_TOP : SPK_ATT_TOP,
	                                        headphones ? HP_ATT_BOTTOM : SPK_ATT_BOTTOM);
}

/* And a USB-C DAC's software volume for the same position. */
static inline long px2_usb_raw(int v)
{
	return v == 0 ? 0
	     : USB_RAW_MAX - aout_level_to_raw(v, VOL_MAX, USB_ATT_TOP, USB_ATT_BOTTOM);
}

/* The battery's empty voltage, in mV, under load (src/battempty.h): the
 * launcher saves the game and powers off under it on three readings in a
 * row, and portkeys ends a port, whatever the level says. The battery's
 * design minimum is 3.40 V (voltage-min-design-microvolt in plastron's
 * device tree), near where the RK817 cuts it out; 50 mV over it leaves the
 * save its time. The voltage read is the driver's voltage_avg, under the
 * load of the moment: a game's 400 to 600 mA puts it 45 to 65 mV under the
 * battery at rest, so this is about 1 to 2% on the OCV table, the end the
 * level's own 2% (BATT_EMPTY_PCT) means on a battery as new. */
#define PX2_BATT_EMPTY_MV 3450

#endif
