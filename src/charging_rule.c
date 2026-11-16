/* SPDX-License-Identifier: GPL-3.0-only */
/* Whether the handheld is on the charger. See src/charging_rule.h. */
#include "charging_rule.h"

#include <string.h>

bool charging_from(int online, const char *status)
{
	if (online == 0 || online == 1) return online == 1;
	/* No answer from the cable: the battery's word, its newline and all */
	return status && (strncmp(status, "Charging", 8) == 0 || strncmp(status, "Full", 4) == 0);
}
