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
#include "device/build.h"

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
	ck(p->model == &td_model_pixel2 && !p->bluetooth && p->gpu_shaders && p->gpu_frames && p->speaker_mono && p->player2 &&
	   p->usb_shared_port && p->wifi == DEV_WIFI_ADAPTER,
	   "pixel2: its own model, no Bluetooth, GPU shaders and frames, one speaker, Player 2, a shared port, adapter Wi-Fi");
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
	ck(!p->model && p->bluetooth && !p->gpu_shaders && !p->gpu_frames && !p->speaker_mono && !p->player2 &&
	   !p->usb_shared_port && p->wifi == DEV_WIFI_BUILTIN,
	   "brick: no model yet, Bluetooth, CPU shaders, no GPU frames, two speakers, no Player 2, built-in Wi-Fi");
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
	ck(p->model == &td_model_pixel2 && p->bluetooth && p->gpu_shaders && p->gpu_frames && !p->speaker_mono && !p->player2 &&
	   !p->usb_shared_port && p->wifi == DEV_WIFI_BUILTIN,
	   "desktop: today's host answers (the Pixel 2's art, shaders and GPU frames, the Brick's rest)");
}

/* The constant questions the launcher asks the platform, answered from the
 * profile in force */
static void answers(void)
{
	static const char *const ids[] = { "pixel2", "brick", "desktop" };
	char what[96];
	size_t i;

	for (i = 0; i < sizeof ids / sizeof ids[0]; i++) {
		const device_profile *p;

		ck(plat_profile_select(ids[i]), "a profile to answer from");
		p = plat_profile();
		snprintf(what, sizeof what, "%s: the platform answers from its profile", ids[i]);
		ck(plat_has_bluetooth() == p->bluetooth && plat_gpu_shaders() == p->gpu_shaders &&
		   plat_gpu_frames() == p->gpu_frames &&
		   plat_speaker_mono() == p->speaker_mono && plat_player2() == p->player2 &&
		   plat_brightness_on_volume() == (p->chords.bright == DEV_BRIGHT_MENU_VOL) &&
		   plat_battery_lights() == p->chords.battery_chord, what);
	}
	plat_profile_select("desktop");
}

/* Developer Mode's unlock: L2 and R2 held together, as taps where they are
 * taps (the Pixel 2, whose in_state never sees them held) and as buttons
 * where they are buttons; never one alone, never the other kind */
static void unlock(void)
{
	const dev_chords *px = &device_profile_pixel2.chords, *br = &device_profile_brick.chords;

	ck(dev_l2r2_held(px, true, true, false, false), "pixel2: both taps held unlock");
	ck(!dev_l2r2_held(px, true, false, false, false), "pixel2: one tap alone does not");
	ck(!dev_l2r2_held(px, false, false, true, true), "pixel2: the buttons' own state is not its");
	ck(dev_l2r2_held(br, false, false, true, true), "brick: both buttons held unlock");
	ck(!dev_l2r2_held(br, false, false, true, false), "brick: one button alone does not");
	ck(!dev_l2r2_held(br, true, true, false, false), "brick: taps it has none of do not");
}

/* The tutorial's handheld: the Pixel 2's own beside its lessons; the
 * desktop's only with the Pixel 2's chords (--fn-volume), not with the
 * Brick's F1 and F2 it teaches; none on the Brick yet */
static void tutorial(void)
{
	const device_profile *px = &device_profile_pixel2, *br = &device_profile_brick;
	const device_profile *dk = &device_profile_desktop;
	const dev_chords fn = dev_chords_fn_volume(dk->chords);

	ck(dev_tutorial_model(px, &px->chords) == &td_model_pixel2, "pixel2: the tutorial draws its model");
	ck(!dev_tutorial_model(br, &br->chords) && !dev_tutorial_model(br, &fn), "brick: no model to draw yet");
	ck(!dev_tutorial_model(dk, &dk->chords), "desktop: none beside F1 and F2");
	ck(dev_tutorial_model(dk, &fn) == &td_model_pixel2, "desktop: the Pixel 2's with --fn-volume");
}

/* Where a button is, for the design system: the Pixel 2's model's places;
 * none for a key it has not, nor on the Brick until it has a model */
static void places(void)
{
	const device_profile *px = &device_profile_pixel2, *br = &device_profile_brick;
	td_face f = TD_FACE_TOP;
	float x = 0.0f, z = 0.0f;

	ck(dev_button_place(px, IN_ACCEPT, &f, &x, &z) && f == TD_FACE_FRONT && x == 23.6f && z == -21.5f,
	   "pixel2: A on the front at (23.6, -21.5)");
	ck(dev_button_place(px, IN_MENU, &f, &x, &z) && f == TD_FACE_RIGHT && x == -1.54f && z == 17.13f,
	   "pixel2: MENU on the right side at (-1.54, 17.13)");
	ck(dev_button_place(px, IN_UP, &f, &x, &z) && f == TD_FACE_FRONT && x == -16.9f && z == -21.0f,
	   "pixel2: up is the d-pad's place");
	ck(dev_button_place(px, IN_L2, &f, &x, &z) && f == TD_FACE_TOP && x == -12.6f,
	   "pixel2: L2 on the top edge, left of the middle");
	f = TD_FACE_TOP; x = z = 7.0f;
	ck(!dev_button_place(px, IN_BRIGHTUP, &f, &x, &z) && f == TD_FACE_TOP && x == 7.0f && z == 7.0f,
	   "pixel2: no brightness key, and nothing set");
	ck(!dev_button_place(px, IN_NONE, &f, &x, &z) && !dev_button_place(px, IN_COUNT, &f, &x, &z),
	   "no place for no button");
	ck(!dev_button_place(br, IN_ACCEPT, &f, &x, &z), "brick: no places until it has a model");
}

/* Each device's build and card (src/device/build.h): its own id, the
 * shared aarch64 baseline, its Diatom port, its partitions and helpers */
static void builds(void)
{
	const dev_build *px = &device_build_pixel2, *br = &device_build_brick;

	ck(!strcmp(px->id, device_profile_pixel2.id) && !strcmp(br->id, device_profile_brick.id),
	   "each build is its profile's device");
	ck(!strcmp(px->arch, "aarch64") && !strcmp(px->mcpu, "cortex-a53") && !strcmp(px->cpu, "cortex-a35"),
	   "pixel2: aarch64, built for the A53 baseline, an A35 inside");
	ck(!strcmp(br->arch, "aarch64") && !strcmp(br->mcpu, "cortex-a53") && !strcmp(br->cpu, "cortex-a53"),
	   "brick: aarch64, the same baseline, an A53 inside");
	ck(!strcmp(px->diatom_port, "pixel2") && !strcmp(br->diatom_port, "brick"), "each its own Diatom port");
	ck(!strcmp(px->table, "mbr") && px->part_boot == 1 && px->part_root_a == 2 && px->part_root_b == 4 &&
	   px->part_games == 3, "pixel2: MBR, boot 1, roots 2 and 4, games 3");
	ck(!strcmp(br->table, "gpt") && br->part_boot == 1 && br->part_root_a == 2 && br->part_root_b == 0 &&
	   br->part_games == 4, "brick: GPT, boot 1, one root at 2, games 4");
	ck(px->helpers == (DEV_HELPER_PORTKEYS | DEV_HELPER_ZYHOME | DEV_HELPER_UPDATE | DEV_HELPER_P2D |
	                   DEV_HELPER_PORTS), "pixel2: portkeys, zyhome, System Update, Player 2, ports");
	ck(br->helpers == (DEV_HELPER_BTPLAYER | DEV_HELPER_BTALSA), "brick: btplayer and Bluetooth sound");
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
	ck(!plat_system_has("teleporter") && !plat_system_has(NULL),
	   "a part the system cannot have is never had");
	{
		char ids[64];

		plat_profile_ids(ids, sizeof ids);
		ck(!strcmp(ids, "pixel2, brick, desktop"), "the linked ids, for --device's error");
		plat_profile_ids(ids, 9);
		ck(!strcmp(ids, "pixel2, "), "and cut to the room there is");
	}
}

int main(void)
{
	whole(&device_profile_pixel2);
	whole(&device_profile_brick);
	whole(&device_profile_desktop);
	pixel2();
	brick();
	desktop();
	answers();
	unlock();
	tutorial();
	places();
	builds();
	choosing();
	if (fails) {
		printf("devices: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("devices: %d checks passed\n", checks);
	return 0;
}
