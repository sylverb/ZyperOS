/* SPDX-License-Identifier: GPL-3.0-only */
/* Your PICO-8's buttons as SDL's mapping line: see p8pad.h. */
#include "p8pad.h"

#include <stdio.h>
#include <string.h>

/* Each of the handheld's buttons (RM_*) as SDL numbers it on this pad: the
 * order of the joypad's key codes, BTN_SOUTH (A) first, SELECT before
 * START, the d-pad's four after them (b10 to b13) */
static const int SDL_BUTTON[RM_BUTTONS] = {
	[RM_A] = 0, [RM_B] = 1, [RM_X] = 2, [RM_Y] = 3, [RM_L1] = 4, [RM_R1] = 5,
	[RM_L2] = 6, [RM_R2] = 7, [RM_START] = 9, [RM_SELECT] = 8,
};

/* SDL's buttons, in the order the line names them, each with the target
 * (RT_*) that is on it and, for the d-pad's, the pad's own button */
static const struct { const char *name; int target, own; } OUT[] = {
	{ "a", RT_B, -1 },            { "b", RT_A, -1 },
	{ "x", RT_X, -1 },            { "y", RT_Y, -1 },
	{ "back", RT_SELECT, -1 },    { "start", RT_START, -1 },
	{ "leftshoulder", RT_L1, -1 }, { "rightshoulder", RT_R1, -1 },
	{ "dpup", RT_UP, 10 },        { "dpdown", RT_DOWN, 11 },
	{ "dpleft", RT_LEFT, 12 },    { "dpright", RT_RIGHT, 13 },
	{ "lefttrigger", RT_L2, -1 }, { "righttrigger", RT_R2, -1 },
};

bool p8_pad_line(const rm_table *t, char *out, size_t n)
{
	size_t at, i;
	int b;

	if (!n) return false;
	at = (size_t)snprintf(out, n, "%s,%s,", P8_PAD_GUID, P8_PAD_NAME);
	for (i = 0; i < sizeof OUT / sizeof OUT[0] && at < n; i++) {
		if (OUT[i].own >= 0)
			at += (size_t)snprintf(out + at, n - at, "%s:b%d,", OUT[i].name, OUT[i].own);
		for (b = 0; b < RM_BUTTONS && at < n; b++)
			if (t->b[b].to == OUT[i].target)
				at += (size_t)snprintf(out + at, n - at, "%s:b%d,", OUT[i].name, SDL_BUTTON[b]);
	}
	/* SDL's own fields for this pad: its name's checksum, and the system */
	if (at < n) at += (size_t)snprintf(out + at, n - at, "crc:d243,platform:Linux,");
	if (at >= n) {
		out[0] = '\0';
		return false;
	}
	return true;
}
