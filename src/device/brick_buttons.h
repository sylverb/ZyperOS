/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_BRICK_BUTTONS_H
#define ZYPEROS_BRICK_BUTTONS_H

/* The TrimUI Brick's controls as printed (src/device/profile.h), for its
 * profile (the host's desktop has the same keys, drawn as the Pixel 2's:
 * desktop_profile.c). The three round keys along the bottom: MENU shows TrimUI's mark, so it is
 * its word here (no vendor's logo is a glyph), SELECT a square, START a play
 * triangle. F1 and F2, the two lit keys above the face buttons, are
 * brightness (src/device/brick.c's keymap: BTN_THUMBL and BTN_THUMBR). */

#include "profile.h"

static const dev_button brick_buttons[] = {
	{ IN_UP,       "UP",     DEV_CAP_DPAD,     DEV_ICON_NONE,   0 },
	{ IN_DOWN,     "DOWN",   DEV_CAP_DPAD,     DEV_ICON_NONE,   0 },
	{ IN_LEFT,     "LEFT",   DEV_CAP_DPAD,     DEV_ICON_NONE,   0 },
	{ IN_RIGHT,    "RIGHT",  DEV_CAP_DPAD,     DEV_ICON_NONE,   0 },
	{ IN_ACCEPT,   "A",      DEV_CAP_ROUND,    DEV_ICON_NONE,   0 },
	{ IN_BACK,     "B",      DEV_CAP_ROUND,    DEV_ICON_NONE,   0 },
	{ IN_X,        "X",      DEV_CAP_ROUND,    DEV_ICON_NONE,   0 },
	{ IN_Y,        "Y",      DEV_CAP_ROUND,    DEV_ICON_NONE,   0 },
	{ IN_L1,       "L1",     DEV_CAP_SHOULDER, DEV_ICON_NONE,   0 },
	{ IN_R1,       "R1",     DEV_CAP_SHOULDER, DEV_ICON_NONE,   0 },
	{ IN_L2,       "L2",     DEV_CAP_SHOULDER, DEV_ICON_NONE,   0 },
	{ IN_R2,       "R2",     DEV_CAP_SHOULDER, DEV_ICON_NONE,   0 },
	{ IN_SELECT,   "SELECT", DEV_CAP_ROUND,    DEV_ICON_SQUARE, 0 },
	{ IN_START,    "START",  DEV_CAP_ROUND,    DEV_ICON_PLAY,   0 },
	{ IN_MENU,     "MENU",   DEV_CAP_ROUND,    DEV_ICON_NONE,   0 },
	{ IN_BRIGHTDN, "F1",     DEV_CAP_PILL,     DEV_ICON_NONE,   0 },
	{ IN_BRIGHTUP, "F2",     DEV_CAP_PILL,     DEV_ICON_NONE,   0 },
	{ IN_VOLUP,    "VOL+",   DEV_CAP_PILL,     DEV_ICON_NONE,   0 },
	{ IN_VOLDN,    "VOL-",   DEV_CAP_PILL,     DEV_ICON_NONE,   0 },
	{ IN_POWER,    "POWER",  DEV_CAP_ROUND,    DEV_ICON_NONE,   0 },
};

#endif
