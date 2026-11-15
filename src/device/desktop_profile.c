/* SPDX-License-Identifier: GPL-3.0-only */
/* The host build's own profile (src/device/profile.h): what the launcher run
 * on a computer has always answered, so every shot taken before profiles is
 * taken the same after them. Mostly the Brick, whose device file the host
 * build runs (its host half, src/device/brick.c): its folder, its chords and
 * buttons, Bluetooth, two speakers, Wi-Fi of its own. And two of the Pixel
 * 2's, which the host borrowed to show the Pixel 2's screens: the GPU's old
 * screens in Display's Shader row (they were offered when headless, which
 * every shot is), and the 3D handheld on the in-game Controls tab. The
 * tutorial shows that handheld only with --fn-volume, as before (src/
 * profile.c, dev_tutorial_handheld). Shots of a real handheld wear its own
 * profile: --device pixel2, --device brick. */
#include "profile.h"
#include "brick_buttons.h"

const device_profile device_profile_desktop = {
	.id = "desktop",
	.name = "Desktop",
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
	.handheld = true,
	.bluetooth = true,
	.gpu_shaders = true,
	.speaker_mono = false,
	.player2 = false,
	.usb_shared_port = false,
	.wifi = DEV_WIFI_BUILTIN,
};
