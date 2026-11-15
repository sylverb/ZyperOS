/* SPDX-License-Identifier: GPL-3.0-only */
/* The device profiles (src/device/profile.h): every profile filled in, every
 * control it has printed with something, no control twice, and today's
 * answers for the GKD Pixel 2, the TrimUI Brick and the host's `desktop`,
 * which the launcher's pixels depend on (docs/superpowers/specs/
 * 2026-11-14-device-modular-design.md, section 1). Then the choice of a
 * profile by its id, as --device makes it on the host. Built with all three
 * profiles, as the host build is. No SDL. */
#include <stdio.h>
#include <string.h>

#include "../src/device/profile.h"

static int fails, checks;

static void ck(int cond, const char *what)
{
	checks++;
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

static bool near(float a, float b) { return a - b < 0.01f && b - a < 0.01f; }

static void whole(const device_profile *p)
{
	char what[128];
	int i, j;

	snprintf(what, sizeof what, "%s: named, its folders and its controls", p->id ? p->id : "?");
	ck(p->id && *p->id && p->name && *p->name && p->userdata && *p->userdata &&
	   p->boot_label && *p->boot_label && p->buttons && p->n_buttons > 0, what);
	snprintf(what, sizeof what, "%s: a 4:3 screen", p->id);
	ck(p->screen.aspect_w * 3 == p->screen.aspect_h * 4, what);
	snprintf(what, sizeof what, "%s: a panel and a turn of 0, 90, 180 or 270", p->id);
	ck(p->screen.panel_w > 0 && p->screen.panel_h > 0 && p->screen.turn % 90 == 0 &&
	   p->screen.turn >= 0 && p->screen.turn < 360, what);
	snprintf(what, sizeof what, "%s: the glass's size in millimetres", p->id);
	ck(p->screen.active_w_mm > 0 && p->screen.active_h_mm > 0, what);
	for (i = 0; i < p->n_buttons; i++) {
		const dev_button *b = &p->buttons[i];

		snprintf(what, sizeof what, "%s: button %d printed with something", p->id, i);
		ck(b->label && *b->label, what);
		snprintf(what, sizeof what, "%s: button %d a launcher button", p->id, i);
		ck(b->id >= 0 && b->id < IN_COUNT, what);
		for (j = i + 1; j < p->n_buttons; j++) {
			snprintf(what, sizeof what, "%s: button %d not listed twice", p->id, b->id);
			ck(p->buttons[j].id != b->id, what);
		}
	}
}

static bool label_is(const device_profile *p, in_button b, const char *label)
{
	const dev_button *k = dev_button_of(p, b);

	return k && !strcmp(k->label, label);
}

static void pixel2(void)
{
	const device_profile *p = &device_profile_pixel2;
	const dev_button *menu = dev_button_of(p, IN_MENU);

	ck(!strcmp(p->id, "pixel2") && !strcmp(p->userdata, "pixel2") &&
	   !strcmp(p->boot_label, "PX2BOOT"), "pixel2: its id, folder and boot label");
	ck(p->screen.panel_w == 480 && p->screen.panel_h == 640 && p->screen.turn == 270,
	   "pixel2: a 480x640 panel, turned a quarter (pixel2.c's -90)");
	ck(near(p->screen.active_w_mm, 48.8f) && near(p->screen.active_h_mm, 36.6f),
	   "pixel2: 48.8 x 36.6 mm of glass");
	ck(p->screen.still, "pixel2: a still of the panel can be saved");
	ck(p->chords.bright == DEV_BRIGHT_MENU_VOL && p->chords.menu_on_release &&
	   p->chords.lr2_taps && p->chords.home_chord && p->chords.battery_chord,
	   "pixel2: brightness on MENU+VOL, MENU on release, L2/R2 taps, Home, battery");
	ck(p->handheld && !p->bluetooth && p->gpu_shaders && p->speaker_mono && p->player2 &&
	   p->usb_shared_port && p->wifi == DEV_WIFI_ADAPTER,
	   "pixel2: handheld, no Bluetooth, GPU shaders, one speaker, Player 2, a shared port, adapter Wi-Fi");
	ck(menu && !strcmp(menu->label, "MENU") && menu->cap == DEV_CAP_ROUND && menu->icon == DEV_ICON_DOTS,
	   "pixel2: MENU is the round key with three dots");
	ck(!dev_button_of(p, IN_BRIGHTUP) && !dev_button_of(p, IN_BRIGHTDN),
	   "pixel2: no brightness keys of its own");
	ck(label_is(p, IN_ACCEPT, "A") && label_is(p, IN_VOLUP, "VOL+") && label_is(p, IN_L2, "L2"),
	   "pixel2: A, VOL+ and L2 as printed");
}

static void brick(void)
{
	const device_profile *p = &device_profile_brick;
	const dev_button *menu = dev_button_of(p, IN_MENU);

	ck(!strcmp(p->id, "brick") && !strcmp(p->userdata, "brick") &&
	   !strcmp(p->boot_label, "BRICKBOOT"), "brick: its id, folder and boot label");
	ck(p->screen.panel_w == 1024 && p->screen.panel_h == 768 && p->screen.turn == 0,
	   "brick: a 1024x768 panel, not turned");
	ck(near(p->screen.active_w_mm, 65.0f) && near(p->screen.active_h_mm, 48.8f),
	   "brick: 65.0 x 48.8 mm of glass");
	ck(p->chords.bright == DEV_BRIGHT_KEYS && !p->chords.menu_on_release &&
	   !p->chords.lr2_taps && !p->chords.home_chord && !p->chords.battery_chord,
	   "brick: brightness on its own keys, no chords of the Pixel 2's");
	ck(!p->handheld && p->bluetooth && !p->gpu_shaders && !p->speaker_mono && !p->player2 &&
	   !p->usb_shared_port && p->wifi == DEV_WIFI_BUILTIN,
	   "brick: no handheld yet, Bluetooth, CPU shaders, two speakers, no Player 2, built-in Wi-Fi");
	ck(menu && !strcmp(menu->label, "MENU") && menu->icon == DEV_ICON_NONE,
	   "brick: MENU is the word (no vendor mark as an icon)");
	ck(label_is(p, IN_BRIGHTDN, "F1") && label_is(p, IN_BRIGHTUP, "F2"),
	   "brick: F1 and F2 are the brightness keys");
}

static void desktop(void)
{
	const device_profile *p = &device_profile_desktop, *b = &device_profile_brick;

	ck(!strcmp(p->id, "desktop") && !strcmp(p->userdata, "brick"),
	   "desktop: its id, and the Brick's folder the host has always used");
	ck(!memcmp(&p->chords, &b->chords, sizeof p->chords), "desktop: the Brick's chords");
	ck(p->handheld && p->bluetooth && p->gpu_shaders && !p->speaker_mono && !p->player2 &&
	   !p->usb_shared_port && p->wifi == DEV_WIFI_BUILTIN,
	   "desktop: today's host answers (the Pixel 2's art and shaders, the Brick's rest)");
}

static void choosing(void)
{
	ck(plat_profile() == &device_profile_desktop, "the host starts as desktop");
	ck(plat_profile_select("pixel2") && plat_profile() == &device_profile_pixel2,
	   "--device pixel2 takes the Pixel 2's profile");
	ck(!plat_profile_select("gameboy") && plat_profile() == &device_profile_pixel2,
	   "an unknown id is refused and changes nothing");
	ck(plat_profile_select("brick") && plat_profile() == &device_profile_brick,
	   "--device brick takes the Brick's");
	ck(plat_profile_select("desktop") && plat_profile() == &device_profile_desktop, "and back");
	ck(!dev_button_of(plat_profile(), IN_NONE), "no button for IN_NONE");
}

int main(void)
{
	whole(&device_profile_pixel2);
	whole(&device_profile_brick);
	whole(&device_profile_desktop);
	pixel2();
	brick();
	desktop();
	choosing();
	if (fails) {
		printf("devices: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("devices: %d checks passed\n", checks);
	return 0;
}
