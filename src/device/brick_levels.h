/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_BRICK_LEVELS_H
#define ZYPEROS_BRICK_LEVELS_H

/* The TrimUI Brick's volume and brightness, as numbers: the codec's controls,
 * which window of its gain register the twenty-one volume positions span on
 * the speakers and on the jack, and the display engine's rungs - and why
 * each is what it is. Read by src/device/brick.c, as src/device/
 * pixel2_levels.h is the Pixel 2's. Diatom's Brick port (diatom/port/brick.c)
 * holds the same numbers, so that a level crossing the socket means the same
 * thing on both sides: change one, change the other.
 *
 * No SDL here, and nothing of the launcher's. */

#define GAIN_CTL     "digital volume"
#define GAIN_RAW_MAX 63          /* 0 is loudest, 63 quietest */

/* Only the top of that register is worth spending on the volume keys. The
 * control is 1.16 dB per step, so using all 63 puts 73 dB across 21 positions -
 * 3.65 dB a press, which made 60% of the scale -29 dB and the bottom two thirds
 * inaudible. That is not a curve shape problem: dB is already the perceptually
 * even axis, the same reason the brightness ladder below is geometric. It is a
 * RANGE problem, and 73 dB is simply more than a handheld speaker has.
 *
 * Measured on the device 2026-08-28 with a 440 Hz tone at -1.4 dBFS captured on
 * its own microphone. Coarse sweep, room baseline ~40 rms:
 *
 *   raw 0   10034 rms   201x room      raw 31    114 rms   2.3x room
 *   raw 16   1485 rms    30x room      raw 47     34 rms   inaudible
 *
 * The floor was first set at 34 by reading that table, and Eric reported the
 * bottom of the scale as dead. A finer sweep says why - room baseline 33:
 *
 *   raw 18  1091 rms  32.7x      raw 30    85 rms   2.5x
 *   raw 22   404 rms  12.1x      raw 34    55 rms   1.6x
 *   raw 26   173 rms   5.2x
 *
 * 34 is 1.6x room noise, so the last few positions were all sitting in the
 * noise and were indistinguishable from each other. 26 is 5.2x: quiet, and
 * unmistakably present. That is the floor, ~1.5 dB a press across the 21
 * positions.
 *
 * Those rms numbers were captured with HP_CTL already at 0 - the sweep script
 * sets it before measuring - so they describe the chain as it behaves now, and
 * the 18 dB that mixer_defaults() restored was never inside them. A note here
 * briefly claimed the opposite. That was inferred rather than read off the
 * script which produced the table, and the script was on disk the whole time.
 * The 18 dB gap was between the SWEEP and gameplay, not inside the sweep, which
 * is exactly why the table looked sane while the device sounded quiet.
 *
 * What is weak here is the instrument, not the conditions. Those readings came
 * from a microphone across the room, where raw 26 is 5.2x the room and raw 34
 * is indistinguishable from it. A handheld at arm's length is a different
 * question: what reads as silence over there is plainly audible in your hands.
 * That is why the bottom of the scale was still reported as too loud once the
 * chain was fixed, and why the floor below is now set by ear rather than by
 * that table.
 *
 * That session had ONE WORKING SPEAKER, which nobody knew at the time. The
 * quiet channel turned out on 2026-09-02 to be a loose connection on the PCB;
 * it was resoldered and both now play evenly. So 39 was originally judged
 * against roughly half this device's output.
 *
 * It stood anyway. Re-heard on the repaired hardware the same day - the quiet
 * end, the balance and the general sound - and called right, so the constant
 * now has two independent confirmations rather than one lucky derivation. Do
 * not weaken it back to a guess on the strength of the history above.
 *
 * 39, chosen on the device on 2026-08-31 with a game playing, stepping the
 * register down until Eric called it: raw 37 is barely audible and is where he
 * wanted position 1. 39 is the constant that puts position 1 on 37 in this
 * ladder AND in Diatom's, which round differently; 26 put it on 25. Position 20
 * still lands on raw 0, so nothing about maximum changes.
 *
 * It costs resolution. Twenty positions across 39 is about 2.3 dB a press
 * rather than 1.5, bought with a range of 45 dB rather than 30. That is the
 * trade worth making: 30 dB down is not quiet in a room that is quiet, which
 * a microphone across the room could not tell us and an ear could. */
#define GAIN_RAW_USABLE 39

/* Headphones need a different window, not the same one moved.
 *
 * Set by ear on 2026-09-02 with a game playing and a plug in the jack, the same
 * method that produced 39. The ceiling first: above raw 8 it is uncomfortable,
 * so 8 is where position 20 belongs. Then the floor, stepping down - 37, 45,
 * 49, 53 and 57 were each still too loud to be a minimum, and 61 was called
 * right.
 *
 * So the jack spends 53 register steps where the speaker spends 39, and an
 * offset cannot express that. An offset from the measured ceiling would have
 * put position 1 on 47, which was rejected on the way past. That is physically
 * unsurprising: headphones are far more efficient than this speaker, so the
 * same twenty presses have to cover more ground.
 *
 * The cost is resolution. 53 steps across 20 positions is about 3.1 dB a press
 * against the speaker's 2.3. Giving the jack its own number of positions would
 * fix that and is not worth it - 21 positions are shared verbatim with Diatom
 * and launch.sh so a level crossing the socket needs no conversion, and that
 * property is worth more than 0.8 dB of granularity.
 *
 * Both pairs are tied to one game's mix, exactly as 39 was. A quieter game sits
 * under this floor. Consistent with what was already here, not worse. */
#define SPK_RAW_TOP     0
#define SPK_RAW_BOTTOM  GAIN_RAW_USABLE
#define HP_RAW_TOP      8
#define HP_RAW_BOTTOM   61

#define SPEAKER_CTL  "HpSpeaker Switch"   /* the speaker's only true mute */
#define HP_CTL       "Headphone Volume"   /* 0-7, 6 dB a step, INVERTED */
#define HP_CTL_QUIET 7                    /* its quiet end, for the cut */
#define SWAP_CTL     "DAC Swap"           /* 1 crosses left and right */
#define HPSW_CTL     "Headphone Switch"   /* the headphone stage, which the speaker runs off */
#define VOL_MAX      20                   /* PLAT_VOL_MAX: 21 positions, 0..20 - Diatom's scale */

/* Perceived brightness is proportional, not linear, so the rungs are
 * geometric. The first is the panel's measured floor: 0 and 1 are black on
 * this display and the driver clamps neither. Identical to Diatom's ladder,
 * which is what makes the level handoff exact. */
static const unsigned char bright_ladder[] __attribute__((unused)) = {
	2, 4, 8, 16, 32, 48, 72, 96, 128, 160, 192, 255
};
#define BRIGHT_MAX ((int)(sizeof bright_ladder / sizeof bright_ladder[0]) - 1)

#endif
