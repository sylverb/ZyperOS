/* SPDX-License-Identifier: GPL-3.0-only */
/* Whether the handheld is on the charger, from what its power supplies say:
 * the charger's `online` and the battery's `status`, as the kernel's
 * power_supply class gives them (the Pixel 2's rk817-charger and battery,
 * the Brick's axp2202-usb and axp2202-battery).
 *
 * The cable, not the battery's status. rk817_charger says Discharging
 * whenever the battery's current is below zero, cable in or not, and a full
 * battery on the cable sits a few mA below zero for hours. That read as off
 * the charger, and Auto Off powered a plugged-in Pixel 2 down, twice on
 * 2026-11-04. So `online` decides wherever there is one: 1 is on the
 * charger and 0 is not, whatever the status says. Only where it gives no
 * answer (-1: no file, or one that says neither) does the status speak,
 * "Charging" or "Full" being on the charger.
 *
 * Pure: no files, the caller's readings, so tools/battery-check.c holds it
 * on a build machine. */
#ifndef ZYPEROS_CHARGING_RULE_H
#define ZYPEROS_CHARGING_RULE_H

#include <stdbool.h>

/* On the charger, from `online` (0, 1, or anything else for no answer) and
 * the battery's status as read, its newline and all, or NULL when it was not
 * read. The status is only looked at when `online` gives no answer, so a
 * caller with an answer from the cable need not read it. */
bool charging_from(int online, const char *status);

#endif
