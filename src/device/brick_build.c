/* SPDX-License-Identifier: GPL-3.0-only */
/* The TrimUI Brick's build and card (src/device/build.h). Its A133P is a
 * Cortex-A53. The card as plastron lays it out today: GPT, BRICKBOOT at 1,
 * the root at 2, plastron's state at 3, and the games partition datapart
 * adds after it, at 4 (board/brick/genimage.cfg, its datapart); no second
 * root yet: its own numbers come with its image (step 6). */
#include "build.h"

const dev_build device_build_brick = {
	.id = "brick",
	.arch = "aarch64",
	.mcpu = "cortex-a53",
	.cpu = "cortex-a53",
	.diatom_port = "brick",
	.table = "gpt",
	.part_boot = 1,
	.part_root_a = 2,
	.part_root_b = 0,
	.part_games = 4,
	.helpers = DEV_HELPER_BTPLAYER | DEV_HELPER_BTALSA,
};
