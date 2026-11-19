/* SPDX-License-Identifier: GPL-3.0-only */
#include "startfail.h"

#include <stdio.h>
#include <string.h>

#include "i18n.h"

/* The most a name takes of the panel's line: past it, the first of it and "..." */
#define NAME_SHOWN 40

static void shown(char *out, size_t n, const char *name)
{
	size_t len = strlen(name);

	if (len <= NAME_SHOWN) snprintf(out, n, "%s", name);
	else snprintf(out, n, "%.*s...", NAME_SHOWN - 3, name);
}

bool startfail_message(const plat_start_error *e, char *out, size_t n)
{
	char core[64], name[64], version[64];
	const char *slash;

	if (n) out[0] = '\0';
	if (!e || strcmp(e->code, "core_load") != 0) return false;
	slash = strrchr(e->core, '/');
	shown(core, sizeof core, slash ? slash + 1 : e->core);
	shown(name, sizeof name, e->name);
	shown(version, sizeof version, e->version);

	/* A why without the name it names (an older or a newer Diatom's) is the
	 * plain message, never one with a hole in it */
	if (!strcmp(e->why, "machine"))
		snprintf(out, n, T(S_LAUNCH_CORE_MACHINE), core);
	else if (!strcmp(e->why, "library") && e->name[0])
		snprintf(out, n, T(S_LAUNCH_CORE_LIBRARY), core, name);
	else if (!strcmp(e->why, "version") && e->name[0] && e->version[0])
		snprintf(out, n, T(S_LAUNCH_CORE_VERSION), core, name, version);
	else if (!strcmp(e->why, "symbol") && e->name[0])
		snprintf(out, n, T(S_LAUNCH_CORE_SYMBOL), core, name);
	else
		snprintf(out, n, T(S_LAUNCH_CORE_FAILED), core);
	return true;
}
