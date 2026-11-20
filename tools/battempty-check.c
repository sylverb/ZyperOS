/* SPDX-License-Identifier: GPL-3.0-only */
/* make check-battempty
 *
 * The battery run flat while the handheld runs (src/battempty.h), the rule
 * the launcher's battery_empty and portkeys go by: the level at
 * BATT_EMPTY_PCT or under on two readings in a row, the voltage under the
 * device's empty voltage on three whatever the level says, never on the
 * cable, and never on one stray reading. The readings are the callers', one
 * every 5 s. No device, no SDL. */
#include "../src/battempty.h"

#include <stdio.h>

#include "../src/device/pixel2_levels.h"
#include "../src/status.h"

static int failures, checks;

#define CHECK(cond, what) do { \
	checks++; \
	if (!(cond)) { printf("  FAIL: %s (line %d)\n", what, __LINE__); failures++; } \
} while (0)

#define EMPTY PX2_BATT_EMPTY_MV

/* `n` readings the same, off the charger unless `charging`; the verdict of the last */
static batt_verdict readings(batt_empty *e, int n, int pct, bool charging, int mv)
{
	batt_verdict v = BATT_FINE;

	while (n-- > 0) v = batt_empty_note(e, true, pct, charging, mv, EMPTY);
	return v;
}

int main(void)
{
	batt_empty e = { 0 };
	batt_verdict v;
	int i;

	CHECK(BATT_EMPTY_LEVEL_READINGS == 2 && BATT_EMPTY_MV_READINGS == 3,
	      "two readings in a row for the level, three for the voltage");
	CHECK(EMPTY == 3450, "the Pixel 2's empty voltage is 3.45 V under load");
	CHECK(BATT_EMPTY_MV_FLOOR < EMPTY && BATT_EMPTY_MV_FLOOR == 2700,
	      "a reading under 2.7 V is no reading, and under the empty voltage");

	/* The level, as it always was */
	CHECK(readings(&e, 1, BATT_EMPTY_PCT, false, 3700) == BATT_FINE, "the level: one reading at 2% is not empty");
	CHECK(readings(&e, 1, BATT_EMPTY_PCT, false, 3700) == BATT_EMPTY_LEVEL, "the level: the second in a row is");
	CHECK(readings(&e, 1, BATT_EMPTY_PCT, false, 3700) == BATT_EMPTY_LEVEL, "the level: and stays so while it reads empty");
	e = (batt_empty){ 0 };
	CHECK(readings(&e, 5, BATT_EMPTY_PCT + 1, false, 3700) == BATT_FINE, "the level: a point over, never");
	e = (batt_empty){ 0 };
	readings(&e, 1, BATT_EMPTY_PCT, false, 3700);
	readings(&e, 1, 40, false, 3700);
	CHECK(readings(&e, 1, BATT_EMPTY_PCT, false, 3700) == BATT_FINE,
	      "the level: one stray reading, then one over, then one more: not empty");
	e = (batt_empty){ 0 };
	CHECK(readings(&e, 5, BATT_EMPTY_PCT, true, 3700) == BATT_FINE, "the level: on the cable, never");
	e = (batt_empty){ 0 };
	CHECK(readings(&e, 5, -1, false, 3700) == BATT_FINE, "the level: no level (-1), never");
	e = (batt_empty){ 0 };
	readings(&e, 1, BATT_EMPTY_PCT, false, 3700);
	batt_empty_note(&e, false, BATT_EMPTY_PCT, false, 3700, EMPTY);
	CHECK(readings(&e, 1, BATT_EMPTY_PCT, false, 3700) == BATT_FINE,
	      "the level: a failed reading between two at 2% ends the run");

	/* The voltage, whatever the level says: the worn battery that gave out
	 * at 43% */
	e = (batt_empty){ 0 };
	CHECK(readings(&e, 2, 43, false, EMPTY - 20) == BATT_FINE,
	      "the voltage: two readings under the empty voltage at 43% are not empty");
	CHECK(readings(&e, 1, 43, false, EMPTY - 20) == BATT_EMPTY_VOLTAGE,
	      "the voltage: the third in a row is, at 43%");
	CHECK(readings(&e, 1, 43, false, EMPTY - 20) == BATT_EMPTY_VOLTAGE,
	      "the voltage: and stays so while it reads under");
	e = (batt_empty){ 0 };
	CHECK(readings(&e, 10, 43, false, EMPTY) == BATT_FINE, "the voltage: at the empty voltage itself, never");
	e = (batt_empty){ 0 };
	readings(&e, 2, 43, false, EMPTY - 20);
	readings(&e, 1, 43, false, EMPTY + 100);
	readings(&e, 2, 43, false, EMPTY - 20);
	readings(&e, 1, 43, false, EMPTY + 100);
	CHECK(readings(&e, 2, 43, false, EMPTY - 20) == BATT_FINE,
	      "the voltage: sags of one and two readings with one over between: never three in a row, not empty");
	e = (batt_empty){ 0 };
	CHECK(readings(&e, 10, 43, true, EMPTY - 200) == BATT_FINE, "the voltage: on the cable, never");
	e = (batt_empty){ 0 };
	readings(&e, 2, 43, false, EMPTY - 20);
	readings(&e, 1, 43, false, 0);
	CHECK(readings(&e, 2, 43, false, EMPTY - 20) == BATT_FINE,
	      "the voltage: a reading with no voltage (0) ends the run");
	e = (batt_empty){ 0 };
	CHECK(readings(&e, 10, 43, false, BATT_EMPTY_MV_FLOOR - 1) == BATT_FINE,
	      "the voltage: a voltage under 2.7 V, no handheld's, is a bad reading, never empty");
	CHECK(readings(&e, 3, 43, false, BATT_EMPTY_MV_FLOOR) == BATT_EMPTY_VOLTAGE,
	      "the voltage: 2.7 V itself is a reading, and under empty");
	e = (batt_empty){ 0 };
	for (i = 0, v = BATT_FINE; i < 10; i++) v = batt_empty_note(&e, true, 43, false, EMPTY - 200, 0);
	CHECK(v == BATT_FINE, "the voltage: a device with no empty voltage (0) goes by the level alone");
	e = (batt_empty){ 0 };
	for (i = 0, v = BATT_FINE; i < 10; i++) v = batt_empty_note(&e, false, -1, false, EMPTY - 200, EMPTY);
	CHECK(v == BATT_FINE, "the voltage: with no battery read at all, never (the cable is not known)");

	/* Both, and the cable after */
	e = (batt_empty){ 0 };
	v = readings(&e, 3, BATT_EMPTY_PCT, false, EMPTY - 20);
	CHECK(v == BATT_EMPTY_LEVEL, "both rules at once: the level's is named first");
	CHECK(readings(&e, 1, BATT_EMPTY_PCT, true, EMPTY - 20) == BATT_FINE && e.level_seen == 0 && e.mv_seen == 0,
	      "the cable in ends both runs");
	CHECK(readings(&e, 2, 43, false, EMPTY - 20) == BATT_FINE,
	      "the cable out again: the voltage's run starts from nothing");

	/* A long run never wraps */
	e = (batt_empty){ 0 };
	CHECK(readings(&e, 100000, BATT_EMPTY_PCT, false, EMPTY - 20) == BATT_EMPTY_LEVEL &&
	      e.level_seen == BATT_EMPTY_LEVEL_READINGS && e.mv_seen == BATT_EMPTY_MV_READINGS,
	      "100000 readings in a row: still empty, the runs held at their counts");

	printf("battempty-check: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
