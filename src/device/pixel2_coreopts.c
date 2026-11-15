/* SPDX-License-Identifier: GPL-3.0-only */
/* The GKD Pixel 2's core options: the ones measured on its RK3326, its
 * Mali-G31 under Panfrost, or its motor (src/device/profile.h, coreopts).
 * The launcher gives them at run time under the player's own rows
 * (src/coreopts.h); the ones every device takes are in src/coreopts.c.
 * Linked on every device, as 1.0.5 seeded these into every card's library
 * and the one-time cleanup takes them out wherever the card is now. */
#include "profile.h"

const dev_default dev_coreopts_pixel2[] = {
	/* flycast2021, the Dreamcast's core (config/systems.cfg), at 320x240,
	 * not 640x480: on the Pixel 2's Mali-G31 under Panfrost 640x480 held
	 * Crazy Taxi 2's attract mode to 65-75% of its speed, 320x240 ran it at
	 * full speed, 25 to 28 new frames a second on the panel (2026-11-04). */
	{ "coreopt..reicast_internal_resolution", "320x240" },
	/* Mupen64Plus-Next, the Nintendo 64's: GLideN64 at 320x240, the N64's
	 * own size, where the core's default is 640x480, on the same Mali-G31
	 * the Dreamcast's 320x240 above is for. */
	{ "coreopt..mupen64plus-43screensize", "320x240" },
	/* And no framebuffer emulation. With the core handing over a frame only
	 * for a new picture (mk/patches/mupen64plus-next-new-frame-swap.patch)
	 * Mario Kart 64 ran its full speed and Mesa's panfrost crashed it within
	 * seconds to a minute, in 10 of 11 one-minute races: a freed render
	 * target read in pan_preload_emit_pre_frame_dcd, with GLideN64's
	 * framebuffer objects in play, and still with the color copy to RDRAM
	 * off, transaction elimination off or AFBC off. With framebuffer
	 * emulation off, none of 4 races crashed, 1 to 3 minutes each, at 56 to
	 * 59 core frames a second of 60 (2026-11-08). The cost: effects
	 * that read the frame back (a paused game's backdrop, Pokemon Snap's
	 * photos, some transitions) are missing or plain. */
	{ "coreopt.N64.mupen64plus-EnableFBEmulation", "False" },
	/* And its Rumble Pak in the controller, where the core's default is the
	 * Controller Pak: the Pixel 2 has a motor (2026-11-05). Few games need
	 * the Controller Pak - most save on the cartridge, as Mario Kart 64's
	 * records do; its ghosts do not - so a game that does takes it in the
	 * in-game menu's Pak row, which keeps the choice for that game alone
	 * (src/main.c, pak_*, 2026-11-07). */
	{ "coreopt.N64.mupen64plus-pak1", "rumble" },
	/* ParallelN64, the Nintendo 64's default core (config/systems.cfg), as
	 * ROGUE OS set it (its system.cfg and setsettings.sh): Glide64 at
	 * medium accuracy, the "fullspeed" frame rate (a frame only for a new
	 * picture: the same frame again is a dupe), the HLE RSP and the
	 * dynarec, at the N64's own 320x240. On the Pixel 2 a Mario Kart 64
	 * race ran at 56 to 58 of 60 frames a second and Super Mario 64 at its
	 * full speed (2026-11-08). The Rumble Pak in, as Mupen64Plus-Next's
	 * above. */
	{ "coreopt.N64.parallel-n64-gfxplugin", "glide64" },
	{ "coreopt.N64.parallel-n64-gfxplugin-accuracy", "medium" },
	{ "coreopt.N64.parallel-n64-framerate", "fullspeed" },
	{ "coreopt.N64.parallel-n64-rspplugin", "hle" },
	{ "coreopt.N64.parallel-n64-cpucore", "dynamic_recompiler" },
	{ "coreopt.N64.parallel-n64-screensize", "320x240" },
	{ "coreopt.N64.parallel-n64-pak1", "rumble" },
	/* The PlayStation's DualShock (config/systems.cfg, controller|PS1) turns
	 * analog on a button combo, L1+R1+SELECT by default, which a game's own
	 * buttons can press. On L3+R3 it cannot be: the Pixel 2 has neither,
	 * and with no sticks there is nothing analog to turn on. */
	{ "coreopt.PS1.pcsx_rearmed_analog_combo", "l3+r3" },
	/* Opera, the 3DO's: its DSP on a thread of its own. Wolfenstein 3D's
	 * attract mode ran at 43 fps of 60 without it and 51 with it on a
	 * Pixel 2 (2026-11-04). */
	{ "coreopt..opera_dsp_threaded", "enabled" },
	/* PPSSPP v1.20.4, the PSP's (mk/build-ppsspp.sh), set as the systems
	 * that run the PSP on an RK3326 set it (ROCKNIX, Knulli, ArkOS; ROGUE OS
	 * sets the same in its standalone PPSSPP): OpenGL ES, the JIT and fast
	 * memory, the PSP's own 480x272, hardware transform and software
	 * skinning, lazy texture caching, low spline quality, GPU readbacks
	 * skipped, no anisotropic filtering (the core's default is 16x, and the
	 * Mali-G31 has little bandwidth to spare) and no texture scaling. Each
	 * key and value as libretro/libretro_core_options.h has them at the pin.
	 * Every frame is drawn: frameskip and auto frameskip off, the core's own
	 * defaults. With Knulli's auto frameskip of up to 2 in a row, LocoRoco
	 * drew one frame in three on the Pixel 2 (20 a second, each Diatom turn
	 * three PSP frames) while PPSSPP's emulation thread was 29% busy: it
	 * was not behind, and Diatom paces each turn by its sound (1.0.5's
	 * paced_by_sound), so a skipped frame only makes the next turn longer
	 * (2026-11-13). */
	{ "coreopt.PSP.ppsspp_backend",                       "opengl" },
	{ "coreopt.PSP.ppsspp_cpu_core",                      "JIT" },
	{ "coreopt.PSP.ppsspp_fast_memory",                   "enabled" },
	{ "coreopt.PSP.ppsspp_internal_resolution",           "480x272" },
	{ "coreopt.PSP.ppsspp_auto_frameskip",                "disabled" },
	{ "coreopt.PSP.ppsspp_frameskip",                     "disabled" },
	{ "coreopt.PSP.ppsspp_frame_duplication",             "disabled" },
	{ "coreopt.PSP.ppsspp_gpu_hardware_transform",        "enabled" },
	{ "coreopt.PSP.ppsspp_software_skinning",             "enabled" },
	{ "coreopt.PSP.ppsspp_lazy_texture_caching",          "enabled" },
	{ "coreopt.PSP.ppsspp_spline_quality",                "Low" },
	{ "coreopt.PSP.ppsspp_skip_gpu_readbacks",            "enabled" },
	{ "coreopt.PSP.ppsspp_texture_anisotropic_filtering", "disabled" },
	{ "coreopt.PSP.ppsspp_texture_scaling_level",         "disabled" },
	/* PrBoom's rumble, off by its own default: the Pixel 2 has a motor, and
	 * a shot or a hit is felt (libretro_core_options.h at the pin) */
	{ "coreopt.DOOM.prboom-rumble",  "enabled" },
	{ NULL, NULL },
};
