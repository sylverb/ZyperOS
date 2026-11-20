/* SPDX-License-Identifier: GPL-3.0-only */
/* The battery run flat while the handheld runs: when the launcher saves the
 * game and powers off (battery_empty, src/zyperdraw.c, for every screen and
 * the game's wait loop), and when portkeys ends a port (tools/portkeys.c),
 * off the charger.
 *
 * By the level, as always: at or under BATT_EMPTY_PCT on two readings in a
 * row. And by the voltage, whatever the level says: under the device's
 * empty voltage (its profile's batt_empty_mv) on three readings in a row.
 * A gauge that counts down from a full charge a worn battery no longer
 * holds reads too high all the way down: a GKD Pixel 2 turned off at 43%,
 * the battery cut out under a game that had not saved, and the level's rule
 * never saw it coming. The voltage under load is what the battery cuts out
 * on, so it is watched too. plastron's rk817 driver also sets its level from
 * the voltage at the empty end (9-px2-rk817-wear2-empty.patch); this is the
 * launcher's own backstop, on every device with an empty voltage.
 *
 * The readings are the caller's, every 5 s, so one stray reading never
 * powers off: two in a row for the level, three for the voltage, the first
 * to the third 10 s apart, which on the Pixel 2 (whose driver averages
 * voltage_avg and updates it every 8 s) span two of the driver's own
 * readings or more.
 *
 * Pure: no files, the caller's readings, so tools/battempty-check.c holds it. */
#ifndef ZYPEROS_BATTEMPTY_H
#define ZYPEROS_BATTEMPTY_H

#include <stdbool.h>

/* Readings in a row that make the battery empty: by the level, by the voltage */
#define BATT_EMPTY_LEVEL_READINGS 2
#define BATT_EMPTY_MV_READINGS    3

/* A voltage under this, in mV, is no reading: no handheld runs on a battery
 * that low (it is the RK817 driver's own floor for a reading it trusts), so
 * it is a gauge's bad answer, as one after a resume can be */
#define BATT_EMPTY_MV_FLOOR 2700

typedef enum {
	BATT_FINE,           /* not empty, or not yet */
	BATT_EMPTY_LEVEL,    /* at or under BATT_EMPTY_PCT, two readings in a row */
	BATT_EMPTY_VOLTAGE,  /* under the empty voltage, three readings in a row */
} batt_verdict;

typedef struct {
	int level_seen;   /* readings in a row at or under BATT_EMPTY_PCT */
	int mv_seen;      /* readings in a row under the empty voltage */
} batt_empty;

/* One reading into `e`, zeroed to begin: `ok`, the battery was read (the
 * level and the cable, plat_battery); `pct`, its level; `charging`, the
 * cable in; `mv`, its voltage under load in mV, 0 or less for none read;
 * `empty_mv`, the device's empty voltage, 0 or less for none (the level's
 * rule alone). Returns the verdict now: empty while a run holds, the level's
 * named first when both do. A reading that failed, or the cable, ends both
 * runs; a voltage not read ends the voltage's. */
batt_verdict batt_empty_note(batt_empty *e, bool ok, int pct, bool charging, int mv, int empty_mv);

#endif
