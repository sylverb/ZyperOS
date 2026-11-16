/* SPDX-License-Identifier: GPL-3.0-only */
/* The chip's temperature from its thermal zones. See src/thermal.h. */
#include "thermal.h"

#include "fileio.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The zones looked at, as the Pixel 2's own scan always did: its device tree
 * has two, the Brick's three. A number with no zone is passed over rather
 * than taken as the end, so a gap in the numbering hides nothing after it. */
#define THERMAL_ZONES_SEEN 8

static bool named(const char *const *types, const char *type)
{
	for (; *types; types++)
		if (!strcmp(*types, type)) return true;
	return false;
}

int thermal_max_c(const char *const *types, const char *sysroot)
{
	int best = -1, i;

	if (!types) return -1;
	if (!sysroot) sysroot = "";
	for (i = 0; i < THERMAL_ZONES_SEEN; i++) {
		char path[256], type[32], temp[16], *end;
		long mc;

		if (snprintf(path, sizeof path, "%s/sys/class/thermal/thermal_zone%d/type", sysroot, i) >=
		    (int)sizeof path)
			return -1;
		if (file_head(path, type, sizeof type) <= 0) continue;
		type[strcspn(type, "\n")] = '\0';
		if (!named(types, type)) continue;
		snprintf(path, sizeof path, "%s/sys/class/thermal/thermal_zone%d/temp", sysroot, i);
		if (file_head(path, temp, sizeof temp) <= 0) continue;
		mc = strtol(temp, &end, 10);
		/* A reading under 0 C is none, as the Pixel 2's scan had it */
		if (end == temp || mc < 0) continue;
		if (mc / 1000 > best) best = (int)(mc / 1000);
	}
	return best;
}
