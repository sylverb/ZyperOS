/* SPDX-License-Identifier: GPL-3.0-only */
/* The chip's temperature from the kernel's thermal zones, for "Getting hot"
 * (src/heat.h, through plat_chip_temp): the hottest of the zones a device's
 * profile names (thermal_zones in src/device/profile.h).
 *
 * Zones are found by their type, the name the device tree gives them, never
 * by number, so a kernel that numbers them differently is still read right;
 * a zone not there, or with no reading, is left out. Looked for again at
 * every reading, which is a few small sysfs reads every HEAT_POLL_MS.
 *
 * Whole degrees, as the warning's trips are (85 C, and 80 C to say it
 * again), so dropping the kernel's millidegrees changes neither. */
#ifndef ZYPEROS_THERMAL_H
#define ZYPEROS_THERMAL_H

/* The highest reading, in degrees C, of the zones whose type is one of
 * `types` (NULL-ended); -1 when no zone of those types has a reading, or
 * `types` is NULL. `sysroot` is the folder /sys is under: NULL or "" for the
 * system's own, a fixture for a check. */
int thermal_max_c(const char *const *types, const char *sysroot);

#endif
