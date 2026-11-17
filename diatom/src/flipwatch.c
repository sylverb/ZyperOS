/* Refreshes missed, and the clocks they climb. See flipwatch.h. */
#include "flipwatch.h"

void flipwatch_begin(flipwatch *w, unsigned missed, uint64_t start_us)
{
	w->seen = missed;
	w->held = false;
	w->calm_until = start_us + FLIPWATCH_START_US;
}

flipwatch_step flipwatch_turn(flipwatch *w, unsigned missed, bool full, uint64_t now_us)
{
	bool more = missed != w->seen;

	w->seen = missed;
	if (!more || now_us < w->calm_until) return FLIPWATCH_NONE;
	if (!full) {
		w->calm_until = now_us + FLIPWATCH_CALM_US;
		return FLIPWATCH_LEVEL_UP;
	}
	if (w->held) return FLIPWATCH_NONE;
	w->held = true;
	return FLIPWATCH_HOLD;
}
