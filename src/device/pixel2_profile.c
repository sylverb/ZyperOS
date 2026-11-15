/* SPDX-License-Identifier: GPL-3.0-only */
/* The GKD Pixel 2 as a profile (src/device/profile.h). Its 480x640 panel is
 * mounted turned, a quarter turn clockwise to the picture (src/device/
 * pixel2.c turns by -90), 48.8 x 36.6 mm of glass. MENU is the FUNCTION key,
 * a round cap with three dots, seen when it is let go; brightness is MENU
 * with a volume key, as it has no keys of its own; L2 and R2 are taps, so
 * that both together can show the battery on its four side lights. */
#include "profile.h"

static const dev_button pixel2_buttons[] = {
	{ IN_UP,     "UP",     DEV_CAP_DPAD,     DEV_ICON_NONE, 0 },
	{ IN_DOWN,   "DOWN",   DEV_CAP_DPAD,     DEV_ICON_NONE, 0 },
	{ IN_LEFT,   "LEFT",   DEV_CAP_DPAD,     DEV_ICON_NONE, 0 },
	{ IN_RIGHT,  "RIGHT",  DEV_CAP_DPAD,     DEV_ICON_NONE, 0 },
	{ IN_ACCEPT, "A",      DEV_CAP_ROUND,    DEV_ICON_NONE, 0 },
	{ IN_BACK,   "B",      DEV_CAP_ROUND,    DEV_ICON_NONE, 0 },
	{ IN_X,      "X",      DEV_CAP_ROUND,    DEV_ICON_NONE, 0 },
	{ IN_Y,      "Y",      DEV_CAP_ROUND,    DEV_ICON_NONE, 0 },
	{ IN_L1,     "L1",     DEV_CAP_SHOULDER, DEV_ICON_NONE, 0 },
	{ IN_R1,     "R1",     DEV_CAP_SHOULDER, DEV_ICON_NONE, 0 },
	{ IN_L2,     "L2",     DEV_CAP_SHOULDER, DEV_ICON_NONE, 0 },
	{ IN_R2,     "R2",     DEV_CAP_SHOULDER, DEV_ICON_NONE, 0 },
	{ IN_SELECT, "SELECT", DEV_CAP_PILL,     DEV_ICON_NONE, 0 },
	{ IN_START,  "START",  DEV_CAP_PILL,     DEV_ICON_NONE, 0 },
	{ IN_MENU,   "MENU",   DEV_CAP_ROUND,    DEV_ICON_DOTS, 0 },
	{ IN_VOLUP,  "VOL+",   DEV_CAP_ROCKER,   DEV_ICON_NONE, 0 },
	{ IN_VOLDN,  "VOL-",   DEV_CAP_ROCKER,   DEV_ICON_NONE, 0 },
	{ IN_POWER,  "POWER",  DEV_CAP_PILL,     DEV_ICON_NONE, 0 },
};

const device_profile device_profile_pixel2 = {
	.id = "pixel2",
	.name = "GKD Pixel 2",
	.userdata = "pixel2",
	.boot_label = "PX2BOOT",
	.screen = {
		.panel_w = 480, .panel_h = 640, .turn = 270,
		.active_w_mm = 48.8f, .active_h_mm = 36.6f,
		.aspect_w = 4, .aspect_h = 3,
		.still = true,
	},
	.chords = {
		.bright = DEV_BRIGHT_MENU_VOL,
		.menu_on_release = true,
		.lr2_taps = true,
		.home_chord = true,
		.battery_chord = true,
	},
	.buttons = pixel2_buttons,
	.n_buttons = (int)(sizeof pixel2_buttons / sizeof pixel2_buttons[0]),
	.handheld = true,
	.bluetooth = false,
	.gpu_shaders = true,
	.speaker_mono = true,
	.player2 = true,
	.usb_shared_port = true,
	.wifi = DEV_WIFI_ADAPTER,
};
