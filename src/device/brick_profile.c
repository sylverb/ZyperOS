/* SPDX-License-Identifier: GPL-3.0-only */
/* The TrimUI Brick (TG3040) as a profile (src/device/profile.h). Its glass:
 * 3.2 in at 1024x768, 65.0 x 48.8 mm. Its own handheld beside the
 * tutorial's lessons and on the Controls tab (src/device/brick_model.c). */
#include "profile.h"
#include "brick_buttons.h"

/* The A133's sensors as its device tree names their zones
 * (plastron/buildroot/board/brick/dts/sun50iw10p1-trimui-brick.dts): the
 * CPU's, whose trips slow the CPU at 58 and 63 C and power the Brick off at
 * 90 C, and the GPU's. Its third, the DDR's, is the memory's, not the chip's
 * to warn of. */
static const char *const brick_zones[] = { "cpu_thermal_zone", "gpu_thermal_zone", NULL };

const device_profile device_profile_brick = {
	.id = "brick",
	.name = "TrimUI Brick",
	/* Allwinner's Linux 4.9 for the A133 (plastron/THIRD-PARTY.md) */
	.kernel = { "Allwinner", "Brick" },
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
	.model = &td_model_brick,
	.bluetooth = true,
	.gpu_shaders = false,
	/* Off until a GPU core runs on its PowerVR (the spec's section 6) */
	.gpu_frames = false,
	/* None measured on it yet: every device's alone */
	.coreopts = NULL,
	.speaker_mono = false,
	.player2 = false,
	.usb_shared_port = false,
	.screen_colors = false,
	.wifi = DEV_WIFI_BUILTIN,
	.thermal_zones = brick_zones,
};
