/* SPDX-License-Identifier: GPL-3.0-only */
/* The battery run flat while the handheld runs. See src/battempty.h. */
#include "battempty.h"

#include "status.h"

/* One more in a row, held at `most`: a run stays empty, and never wraps */
static int seen_more(int seen, int most)
{
	return seen < most ? seen + 1 : most;
}

batt_verdict batt_empty_note(batt_empty *e, bool ok, int pct, bool charging, int mv, int empty_mv)
{
	if (!ok || charging) {
		e->level_seen = e->mv_seen = 0;
		return BATT_FINE;
	}
	if (pct >= 0 && pct <= BATT_EMPTY_PCT)
		e->level_seen = seen_more(e->level_seen, BATT_EMPTY_LEVEL_READINGS);
	else
		e->level_seen = 0;
	if (empty_mv > 0 && mv >= BATT_EMPTY_MV_FLOOR && mv < empty_mv)
		e->mv_seen = seen_more(e->mv_seen, BATT_EMPTY_MV_READINGS);
	else
		e->mv_seen = 0;
	if (e->level_seen >= BATT_EMPTY_LEVEL_READINGS) return BATT_EMPTY_LEVEL;
	if (e->mv_seen >= BATT_EMPTY_MV_READINGS) return BATT_EMPTY_VOLTAGE;
	return BATT_FINE;
}
