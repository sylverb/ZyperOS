/* SPDX-License-Identifier: GPL-3.0-only */
/* The GKD Pixel 2's 3D drawing as data (src/device/td_model.h), in the
 * millimetres tools/build-pixel2-model.py builds the landing page's model
 * from (its dimensioned drawing, 63 x 81 x 17 mm): the front's keys, MENU
 * and POWER on the right side, the volume rocker on the left, the shoulders
 * on the top edge, L1 and R1 outside. Drawn by src/tourdevice.c's Pixel 2
 * art. No SDL. */
#include "td_model.h"

static const td_part pixel2_parts[] = {
	{ TD_DPAD,   TD_FACE_FRONT, -16.9f, -21.0f, 9.0f },
	{ TD_A,      TD_FACE_FRONT,  23.6f, -21.5f, 3.05f },
	{ TD_B,      TD_FACE_FRONT,  16.9f, -28.2f, 3.05f },
	{ TD_X,      TD_FACE_FRONT,  16.9f, -14.8f, 3.05f },
	{ TD_Y,      TD_FACE_FRONT,  10.2f, -21.5f, 3.05f },
	{ TD_SELECT, TD_FACE_FRONT,  -3.8f, -29.6f, 2.85f },
	{ TD_START,  TD_FACE_FRONT,   4.0f, -29.6f, 2.85f },
	/* Round, with three dimples in a column */
	{ TD_MENU,   TD_FACE_RIGHT, -1.54f, 17.13f, 2.65f },
	/* A capsule at 45 degrees, 9.8 mm long */
	{ TD_POWER,  TD_FACE_RIGHT, -1.74f,  6.65f, 4.9f },
	/* The rocker's two ends, 5.9 mm each */
	{ TD_VOLUP,  TD_FACE_LEFT,  -1.82f, 16.52f, 2.95f },
	{ TD_VOLDN,  TD_FACE_LEFT,  -1.82f,  7.05f, 2.95f },
	/* Up to 41.7 mm, over the 40.5 mm top edge; their half widths */
	{ TD_L1,     TD_FACE_TOP,  -23.9f,  41.7f, 7.55f },
	{ TD_R1,     TD_FACE_TOP,   23.9f,  41.7f, 7.55f },
	{ TD_L2,     TD_FACE_TOP,  -12.6f,  41.7f, 3.7f },
	{ TD_R2,     TD_FACE_TOP,   12.6f,  41.7f, 3.7f },
};

/* Drawn flat (td_model.h): desk mode's window and the Simulator's */
static const td_flat_key pixel2_flat_keys[] = {
	{ .key = TD_X, .label = "X", .hint = TD_HINT_ABOVE },
	{ .key = TD_Y, .label = "Y", .hint = TD_HINT_LEFT },
	{ .key = TD_A, .label = "A", .hint = TD_HINT_RIGHT },
	{ .key = TD_B, .label = "B", .hint = TD_HINT_BELOW },
	{ .key = TD_SELECT, .label = "SE", .hint = TD_HINT_BELOW, .hint_dx = -14 },
	{ .key = TD_START, .label = "ST", .hint = TD_HINT_BELOW, .hint_dx = 14 },
	{ .key = TD_L1, .label = "L1", .hint = TD_HINT_ON },
	{ .key = TD_L2, .label = "L2", .hint = TD_HINT_ON },
	{ .key = TD_R2, .label = "R2", .hint = TD_HINT_ON },
	{ .key = TD_R1, .label = "R1", .hint = TD_HINT_ON },
	/* The rocker's two halves, meeting in its middle */
	{ .key = TD_VOLUP, .label = "+", .hint = TD_HINT_RIGHT, .y = 15.75f, .len = 8.0f },
	{ .key = TD_VOLDN, .label = "-", .hint = TD_HINT_RIGHT, .y = 7.65f, .len = 8.0f },
	{ .key = TD_MENU, .hint = TD_HINT_LEFT },
	/* The 45-degree capsule, as tall as it stands on the edge */
	{ .key = TD_POWER, .hint = TD_HINT_LEFT, .y = 6.65f, .len = 7.2f },
};
static const td_flat_label pixel2_flat_labels[] = {
	{ "VOL", -34.6f, 11.7f, true }, { "MENU", 34.6f, 17.1f, true }, { "POWER", 34.6f, 6.65f, true },
};
/* Under POWER, top to bottom: battery4, battery3, battery2, and the bottom
 * one, red (battery0) or green (battery1) */
static const td_flat_light pixel2_flat_lights[] = {
	{ 1.4f, 4, -1 }, { -4.03f, 3, -1 }, { -9.67f, 2, -1 }, { -15.3f, 1, 0 },
};
static const td_flat pixel2_flat = {
	.corner = 3.9f,
	.margin_x = 6.0f, .margin_top = 4.0f, .margin_bottom = 2.5f,
	.glass = { 0, 14.2f, 55.1f, 43.8f }, .glass_r = 1.2f,
	.screen = { 0, 15.55f, 48.8f, 36.6f },
	.word = "ZyperOS", .word_y = -5.5f,
	.recess = { 0.1f, -29.6f, 16.4f, 7.6f }, .recess_r = 3.8f,
	.dpad_arm = 3.2f,
	.side_out = 0.3f, .side_w = 3.0f, .shoulder_rise = 0.4f, .shoulder_h = 3.6f,
	.light_x = 32.0f, .light_w = 1.3f, .light_h = 3.4f,
	.lights = pixel2_flat_lights, .n_lights = (int)(sizeof pixel2_flat_lights / sizeof pixel2_flat_lights[0]),
	.body_hi = 0xB4CC6C, .body = 0x9CB849, .body_lo = 0x83A03A, .recess_c = 0x708B32,
	.cap = 0x3A3C3F, .cap_down = 0x1E2021, .marking = 0xA4A7AA, .glass_c = 0x0B0E0E, .light_off = 0x56723A,
	.keys = pixel2_flat_keys, .n_keys = (int)(sizeof pixel2_flat_keys / sizeof pixel2_flat_keys[0]),
	.labels = pixel2_flat_labels, .n_labels = (int)(sizeof pixel2_flat_labels / sizeof pixel2_flat_labels[0]),
};

const td_model td_model_pixel2 = {
	.name = "GKD Pixel 2",
	.art = TD_ART_PIXEL2,
	.body_w = 63.0f,
	.body_h = 81.0f,
	.lean_centre = 8.0f,
	.parts = pixel2_parts,
	.n_parts = (int)(sizeof pixel2_parts / sizeof pixel2_parts[0]),
	.front = { 10.0f, 3.55f, 0.0f },
	.right = { 52.0f, 3.95f, 0.0f },
	.left = { -52.0f, 3.95f, 0.0f },
	.both = { 0.0f, 3.45f, 0.0f },
	.top = { 10.0f, 3.55f, TD_PITCH_DEG },
	/* No key on a back ledge: the front */
	.back = { 10.0f, 3.55f, 0.0f },
	/* The Controls tab's front pose, 3.15 units a millimetre */
	.tab = { 10.0f, 3.15f, 0.0f },
	/* Every vertex of every pose and turn, a halo's faint edge too, as
	 * --handheld-box measures it (src/tourdevice.c, td_box_holds) */
	.keep_tour = { 0, 66, 304, 382 },
	.keep_tab = { 128, 176, 256, 332 },
	.flat = &pixel2_flat,
};
