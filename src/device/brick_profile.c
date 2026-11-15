/* SPDX-License-Identifier: GPL-3.0-only */
/* The TrimUI Brick (TG3040) as a profile (src/device/profile.h). Its glass:
 * 3.2 in at 1024x768, 65.0 x 48.8 mm. No handheld drawing yet (step 5 of
 * the device spec draws one), so its tutorial keeps its block in the middle
 * and its Controls tab draws no other device's. */
#include "profile.h"
#include "brick_buttons.h"

const device_profile device_profile_brick = {
	.id = "brick",
	.name = "TrimUI Brick",
	.userdata = "brick",
	.boot_label = "BRICKBOOT",
	.screen = {
		.panel_w = 1024, .panel_h = 768, .turn = 0,
		.active_w_mm = 65.0f, .active_h_mm = 48.8f,
		.aspect_w = 4, .aspect_h = 3,
		.still = false,
	},
	.chords = { .bright = DEV_BRIGHT_KEYS },
	.buttons = brick_buttons,
	.n_buttons = (int)(sizeof brick_buttons / sizeof brick_buttons[0]),
	.handheld = false,
	.bluetooth = true,
	.gpu_shaders = false,
	.speaker_mono = false,
	.player2 = false,
	.usb_shared_port = false,
	.wifi = DEV_WIFI_BUILTIN,
};
