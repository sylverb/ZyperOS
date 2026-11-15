/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_DEVICE_PROFILE_H
#define ZYPEROS_DEVICE_PROFILE_H

/* What a handheld is, stated once (docs/superpowers/specs/
 * 2026-11-14-device-modular-design.md, section 1): its name and folders, its
 * screen as glass and pixels, its controls as they are printed, the chords it
 * needs because a button is missing or doubles as another, and what it has -
 * Bluetooth, a speaker or two, a GPU that draws the old screens. Facts, never
 * guesses: everything here was true of the device when it was written, and
 * nothing is worked out at run time.
 *
 * Live questions stay in the device files (src/device/<name>.c): whether a
 * Wi-Fi adapter is plugged in, whether the charger's limit can be written now,
 * whether MENU is being held. A screen asks the profile what the handheld has
 * and the platform what it is doing.
 *
 * One profile a device, in src/device/<id>_profile.c, plain data with no SDL
 * so a check links it as it is. The device build links its own; the host
 * build links them all and wears one (--device), so every handheld's screens
 * can be looked at on a computer. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../inbutton.h"
#include "td_model.h"   /* td_model: a handheld's 3D drawing */

/* How brightness is changed: keys of its own (the Brick's F1 and F2), or
 * MENU with a volume key, on a device that has none (the Pixel 2) */
typedef enum { DEV_BRIGHT_KEYS, DEV_BRIGHT_MENU_VOL } dev_bright;

/* What a missing or doubled button makes the player press instead. */
typedef struct {
	dev_bright bright;
	/* MENU is FUNCTION, seen when it is let go (a chord's press may be next) */
	bool menu_on_release;
	/* L2 and R2 are taps, seen when let go without the other: together they
	 * are the battery's level on the lights */
	bool lr2_taps;
	/* MENU+START held a second goes Home from anything (tools/zyhome.c) */
	bool home_chord;
	/* L2+R2 flashes the battery's level on lights of the device's own */
	bool battery_chord;
} dev_chords;

/* A control's cap as the device has it, and the mark on it from the design
 * system's closed set (never a vendor's logo: a key that shows one is its
 * word, as the Brick's MENU). The glyph on screen is drawn from these in the
 * theme's colors; cap_rgb is for the 3D handheld alone. */
typedef enum { DEV_CAP_ROUND, DEV_CAP_PILL, DEV_CAP_ROCKER, DEV_CAP_DPAD,
               DEV_CAP_SHOULDER, DEV_CAP_STICK } dev_cap;
typedef enum { DEV_ICON_NONE, DEV_ICON_SQUARE, DEV_ICON_PLAY, DEV_ICON_DOTS,
               DEV_ICON_STICK } dev_icon;

typedef struct {
	in_button   id;
	const char *label;       /* as printed: "A", "MENU", "F1", "VOL+" */
	dev_cap     cap;
	dev_icon    icon;
	uint32_t    cap_rgb;     /* 0xRRGGBB, 0 for the body's own dark */
} dev_button;

typedef enum { DEV_WIFI_NONE, DEV_WIFI_ADAPTER, DEV_WIFI_BUILTIN } dev_wifi;

typedef struct device_profile {
	const char *id;          /* "pixel2": --device, the build's DEVICE */
	const char *name;        /* "GKD Pixel 2", as people call it */
	const char *userdata;    /* the card's .userdata/<this> folder */
	const char *boot_label;  /* the boot partition's FAT label */
	/* The glass and its pixels: physical facts only. The layout system
	 * works the canvas out from them, the platform the scale. */
	struct {
		int   panel_w, panel_h;          /* pixels, as the panel scans */
		int   turn;                      /* clockwise degrees to the picture */
		float active_w_mm, active_h_mm;  /* the lit area, landscape */
		int   inset_t, inset_r, inset_b, inset_l;   /* panel pixels */
		int   aspect_w, aspect_h;
		bool  still;                     /* a panel still can be saved */
	} screen;
	dev_chords        chords;
	const dev_button *buttons;
	int               n_buttons;
	/* Its 3D drawing for the tutorial and the Controls tab (td_model.h),
	 * NULL where it has none yet: never another device's */
	const td_model *model;
	bool bluetooth;
	bool gpu_shaders;        /* Diatom's old screens are a GPU's pass here */
	bool speaker_mono;       /* one built-in speaker */
	bool player2;            /* NES Player 2 was built and tried on it */
	bool usb_shared_port;    /* one port for the computer and accessories */
	dev_wifi wifi;
} device_profile;

extern const device_profile device_profile_pixel2;
extern const device_profile device_profile_brick;
extern const device_profile device_profile_desktop;

/* The profile in force, never NULL: the build's own, or the one the host
 * build was told to wear */
const device_profile *plat_profile(void);
/* Wear the linked profile named `id`; false, and nothing changed, when the
 * build has none by that name (a device build has its own alone) */
bool plat_profile_select(const char *id);
/* The `i`th linked profile, from 0, or NULL past the last: every handheld on
 * the host, the device's own alone on a device */
const device_profile *plat_profile_at(int i);
/* The linked profiles' ids, ", " between them, into `out` (for --device's
 * error: "pixel2, brick, desktop" on the host, the device's own on a device) */
void plat_profile_ids(char *out, size_t n);
/* `b` as `p` prints it, or NULL where `p` has no such control */
const dev_button *dev_button_of(const device_profile *p, in_button b);
/* Where `b` is on `p`'s handheld, as its model has it (td_model.h: the face
 * in *face, (x, z) on it in *x and *z): the d-pad for its four directions,
 * F1 and F2 for brightness down and up. False, and nothing set, where `p`
 * has no model or its model has no such key. For the design system, which
 * points at a key where it is. */
bool dev_button_place(const device_profile *p, in_button b, td_face *face, float *x, float *z);
/* `base` with the Pixel 2's brightness chord, MENU on release and Home: the
 * shots' --fn-volume, which shows those screens on the host as the Pixel 2
 * has them until the shots wear --device pixel2 (step 5 of the device spec) */
dev_chords dev_chords_fn_volume(dev_chords base);
/* The handheld the tutorial draws beside its lessons for `p` with chords
 * `c`, NULL for none: `p`'s model where brightness is MENU+VOL. The second
 * half is for the host's desktop profile alone, which borrows the Pixel 2's
 * drawing for the Controls tab but teaches the Brick's F1 and F2; it goes
 * when the Brick gets a drawing of its own (step 5). */
const td_model *dev_tutorial_model(const device_profile *p, const dev_chords *c);
/* L2 and R2 both held this moment, for Developer Mode's unlock: as taps where
 * the chords say they are taps (`tap_l2`, `tap_r2`: plat_tap_held), as the
 * buttons' own state everywhere else (`down_l2`, `down_r2`: in_state) */
bool dev_l2r2_held(const dev_chords *c, bool tap_l2, bool tap_r2, bool down_l2, bool down_r2);

/* The questions the launcher asks the platform whose answers are facts about
 * the handheld, from the profile in force (src/profile.c). Live questions
 * (plat_has_wifi: an adapter plugged in; plat_menu_held, plat_tap_held) stay
 * in src/platform.h and the device files. */
/* Bluetooth at all, not whether it is on: a device without it hides
 * everything that needs it (src/sys_menu.h) */
bool plat_has_bluetooth(void);
/* Whether Diatom draws its old screens here, the shaders a GPU draws (the
 * CRT, scanlines, the LCD grid; src/app.h SHADERS' gpu): on the Pixel 2,
 * whose pass is a GPU's; not on the Brick, whose is the CPU's */
bool plat_gpu_shaders(void);
/* Whether the built-in speaker is one speaker, as on the GKD Pixel 2, where
 * the Brick has two: Audio Channels' Auto is mono there and stereo on
 * everything else (src/audioout.h). */
bool plat_speaker_mono(void);
/* Whether brightness is FUNCTION with the volume keys, the device having no
 * brightness keys of its own. What MENU > Controls calls them (controls.h). */
bool plat_brightness_on_volume(void);
/* Whether the device shows the battery on lights of its own, L2 with R2
 * flashing the level there outside a game (the Pixel 2's four side lights,
 * which plastron's gauge drives). What MENU > Controls says. */
bool plat_battery_lights(void);
/* Whether NES Player 2 (src/p2.h) is offered here: a device whose own
 * hotspot (a USB Wi-Fi adapter, hostapd) and cable link (the USB network
 * gadget) it was built and tried on, the Pixel 2. Nothing in a game's menu
 * says it elsewhere. */
bool plat_player2(void);

/* What the system on this card has, a second fact beside the profile's: a
 * card can hold a newer ZyperOS folder on an older system (Flash Drive came
 * with 1.0.4's), so a feature shows when the handheld can do it AND the
 * system has its part. "usbdrive" (Flash Drive's program), "hostapd" (the
 * hotspot's); any other part false. Read from the system's files for now;
 * the image will say it in /etc/zyperos/system (step 4 of the device spec). */
bool plat_system_has(const char *part);

#endif
