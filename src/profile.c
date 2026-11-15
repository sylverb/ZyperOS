/* SPDX-License-Identifier: GPL-3.0-only */
/* Which device profile is in force (src/device/profile.h). The build names
 * its own: mk/cross.mk the device it builds for (ZYPEROS_PROFILE is
 * device_profile_pixel2 or device_profile_brick), mk/native.mk the host's
 * desktop, with every profile linked (ZYPEROS_PROFILES_ALL) so --device can
 * wear any of them. Plain C, no SDL: the checks link it as it is. */
#include "device/profile.h"

#include <stddef.h>
#include <string.h>
#include <unistd.h>

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

bool plat_has_bluetooth(void)        { return active->bluetooth; }
bool plat_gpu_shaders(void)          { return active->gpu_shaders; }
bool plat_speaker_mono(void)         { return active->speaker_mono; }
bool plat_brightness_on_volume(void) { return active->chords.bright == DEV_BRIGHT_MENU_VOL; }
bool plat_battery_lights(void)       { return active->chords.battery_chord; }
bool plat_player2(void)              { return active->player2; }

bool plat_system_has(const char *part)
{
	static const struct { const char *part, *path; } parts[] = {
		{ "usbdrive", "/usr/sbin/usbdrive" },
		{ "hostapd",  "/usr/sbin/hostapd" },
	};
	size_t i;

	for (i = 0; part && i < sizeof parts / sizeof parts[0]; i++)
		if (!strcmp(parts[i].part, part)) return access(parts[i].path, X_OK) == 0;
	return false;
}

const dev_button *dev_button_of(const device_profile *p, in_button b)
{
	int i;

	for (i = 0; p && i < p->n_buttons; i++)
		if (p->buttons[i].id == b) return &p->buttons[i];
	return NULL;
}
