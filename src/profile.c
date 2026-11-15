/* SPDX-License-Identifier: GPL-3.0-only */
/* Which device profile is in force (src/device/profile.h). The build names
 * its own: mk/cross.mk the device it builds for (ZYPEROS_PROFILE is
 * device_profile_pixel2 or device_profile_brick), mk/native.mk the host's
 * desktop, with every profile linked (ZYPEROS_PROFILES_ALL) so --device can
 * wear any of them. Plain C, no SDL: the checks link it as it is. */
#include "device/profile.h"

#include <stddef.h>
#include <string.h>

#ifndef ZYPEROS_PROFILE
#error "ZYPEROS_PROFILE names the build's device profile (mk/cross.mk, mk/native.mk)"
#endif

static const device_profile *const linked[] = {
#ifdef ZYPEROS_PROFILES_ALL
	&device_profile_pixel2, &device_profile_brick, &device_profile_desktop,
#else
	&ZYPEROS_PROFILE,
#endif
};

static const device_profile *active = &ZYPEROS_PROFILE;

const device_profile *plat_profile(void) { return active; }

bool plat_profile_select(const char *id)
{
	size_t i;

	for (i = 0; id && i < sizeof linked / sizeof linked[0]; i++)
		if (!strcmp(linked[i]->id, id)) {
			active = linked[i];
			return true;
		}
	return false;
}

const dev_button *dev_button_of(const device_profile *p, in_button b)
{
	int i;

	for (i = 0; p && i < p->n_buttons; i++)
		if (p->buttons[i].id == b) return &p->buttons[i];
	return NULL;
}
