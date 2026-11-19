/* SPDX-License-Identifier: GPL-3.0-only */
/* POWER: a press, or a hold, turns the handheld off (1.0.6).
 *
 * MENU > Power Button chooses. **Press**, as it has always been: POWER
 * pressed saves the game and turns the handheld off. **Hold**: POWER held
 * for DS_MS_HOLD_POWER, one second (Tarik, 2026-11-17: "so make it 1
 * second"); a press does nothing but show how far a hold has to go. The
 * charging screen's Hold POWER to start is the same second (Tarik, 1.0.6),
 * and takes the hold it starts with (powerhold_consume). Asked for on the
 * Discord (drmoze: "A long press on the power button should be required to
 * shut down. My Pixel shuts down after a momentary press, which is too easy
 * to do accidentally"), and my pick to put it in MENU, Press staying the
 * default.
 *
 * One rule for every place that reads the key: the launcher's input (every
 * screen's in_state, src/platform.c), its watch over a game (resident.c's
 * device_power_pressed) and over a program it waits for (plat_run), and
 * portkeys over a port. Each feeds this the key's own events and asks it
 * whether the handheld turns off now, so a hold is the same second
 * whichever of them happens to read it.
 *
 * While a hold is under way the settings pill shows it (src/osdbar.h,
 * OSDBAR_POWER): the power mark and a segment lit for each twentieth of the
 * hold (50 ms), the same pill the volume is, where it is on every screen, over a
 * game (Diatom, SETPOWERHOLD) and over a port (portkeys). Let go early, it
 * fades out at once.
 *
 * The mode is one setting for the whole process, set where it is read
 * (powerhold_set_mode): the launcher from its database at start and from the
 * row, portkeys from the same database as a port starts.
 *
 * Plain C, no SDL, so tools/powerhold-check.c and portkeys link it. */
#ifndef ZYPEROS_POWERHOLD_H
#define ZYPEROS_POWERHOLD_H

#include <stdbool.h>

#include "design.h"

#define POWERHOLD_MS  DS_MS_HOLD_POWER
/* The setting in the device database: "hold" for Hold; anything else, or
 * nothing, is Press */
#define POWERHOLD_KEY "power.button"

typedef struct {
	bool     down;      /* the key is down, as its last event said */
	bool     fired;     /* this time down has turned the handheld off, or was
	                     * taken for something else (powerhold_consume) */
	unsigned since;     /* when it went down */
} powerhold;

/* Hold (true) or Press (false), for every powerhold in the process */
void powerhold_set_mode(bool hold);
bool powerhold_mode(void);

/* The key went down (`down`) or up at `now`. True when this is the turn-off:
 * the press, in Press mode. A down while already down (a repeat) is nothing. */
bool powerhold_key(powerhold *p, bool down, unsigned now);

/* True once per time down, when a hold in Hold mode reaches POWERHOLD_MS */
bool powerhold_due(powerhold *p, unsigned now);

/* How far the hold has got, 0 to 1000, while the key is down in Hold mode and
 * has not fired; -1 when no hold is under way (Press mode, the key up, or the
 * hold done) */
int powerhold_permille(const powerhold *p, unsigned now);

/* Milliseconds until powerhold_due fires, for a wait to end in time; -1 when
 * nothing is due */
int powerhold_wait_ms(const powerhold *p, unsigned now);

/* Milliseconds until the next of `steps` even steps of the hold (the pill's
 * next segment) or its end, whichever is sooner; -1 when nothing is due */
int powerhold_next_ms(const powerhold *p, unsigned now, int steps);

/* This time down is not a turn-off: a screen that used the key for something
 * else, the charging screen's hold to start, takes it, and the key must go up
 * and down again to turn the handheld off */
void powerhold_consume(powerhold *p);

#endif
