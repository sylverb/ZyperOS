/* SPDX-License-Identifier: GPL-3.0-only */
/* The TrimUI Brick's 3D drawing as data (src/device/td_model.h), in the
 * millimetres of its published outline (73.2 x 109.9 mm; 11.8 deep above
 * the back ledge, 19.9 below it) and the key places measured to it
 * (from TrimUI's product images; the numbers are the ones in this file): the d-pad, F1 and F2, A B X Y and the
 * three round keys along the bottom (MENU, SELECT, START) on the front, the
 * volume keys on the left side, POWER on the right, and the four shoulders
 * on the back ledge, L1 and R1 outside. A side key's x is its depth across
 * the face: the thin part's middle, 4.05 mm behind the body's (the front at
 * -9.95, the thin part's back at 1.85, the thick part's at 9.95). Drawn by
 * src/tourdevice.c's Brick art. No SDL. */
#include "td_model.h"

static const td_part brick_parts[] = {
	/* Its half span; the arms 7.0 wide */
	{ TD_DPAD,   TD_FACE_FRONT, -20.7f,  -17.35f, 9.65f },
	/* The diamond about (19.7, -17.15), each key 7.6 from it, 7.7 across */
	{ TD_A,      TD_FACE_FRONT,  27.3f,  -17.15f, 3.85f },
	{ TD_B,      TD_FACE_FRONT,  19.7f,  -24.75f, 3.85f },
	{ TD_X,      TD_FACE_FRONT,  19.7f,   -9.55f, 3.85f },
	{ TD_Y,      TD_FACE_FRONT,  12.1f,  -17.15f, 3.85f },
	/* The brightness keys, pills 7.1 x 2.6: their half lengths */
	{ TD_F1,     TD_FACE_FRONT,  -5.7f,   -8.65f, 3.55f },
	{ TD_F2,     TD_FACE_FRONT,   5.0f,   -8.65f, 3.55f },
	/* The round keys along the bottom, 5.4 across */
	{ TD_MENU,   TD_FACE_FRONT, -10.5f,  -46.45f, 2.7f },
	{ TD_SELECT, TD_FACE_FRONT,  -0.5f,  -46.45f, 2.7f },
	{ TD_START,  TD_FACE_FRONT,   9.6f,  -46.45f, 2.7f },
	/* On the thin part's sides: the volume keys 7.7 long, POWER 7.1 across
	 * (the FN slider above it is no key of the launcher's: art only) */
	{ TD_VOLUP,  TD_FACE_LEFT,   -4.05f,  44.65f, 3.85f },
	{ TD_VOLDN,  TD_FACE_LEFT,   -4.05f,  33.55f, 3.85f },
	{ TD_POWER,  TD_FACE_RIGHT,  -4.05f,  15.65f, 3.55f },
	/* On the back ledge (z 12.05), standing up to 13.65: their half widths,
	 * L1 and R1 from the body's edges, L2 and R2 beside the USB-C housing */
	{ TD_L1,     TD_FACE_LEDGE, -27.85f,  13.65f, 8.75f },
	{ TD_L2,     TD_FACE_LEDGE, -12.3f,   13.65f, 5.5f },
	{ TD_R2,     TD_FACE_LEDGE,  12.3f,   13.65f, 5.5f },
	{ TD_R1,     TD_FACE_LEDGE,  27.85f,  13.65f, 8.75f },
};

const td_model td_model_brick = {
	.name = "TrimUI Brick",
	.art = TD_ART_BRICK,
	.body_w = 73.2f,
	.body_h = 109.9f,
	/* SELECT, START, MENU, F1 and F2 are the middle; Y, at 12.1, is the
	 * right's */
	.lean_centre = 11.0f,
	.parts = brick_parts,
	.n_parts = (int)(sizeof brick_parts / sizeof brick_parts[0]),
	/* As tall in the tutorial's column as the Pixel 2 (81 mm at 3.55), the
	 * sides a little nearer, as its are */
	.front = { 10.0f, 2.6f, 0.0f },
	.right = { 52.0f, 2.9f, 0.0f },
	.left = { -52.0f, 2.9f, 0.0f },
	/* No keys on both sides at once and none on the top edge: the front */
	.both = { 10.0f, 2.6f, 0.0f },
	.top = { 10.0f, 2.6f, 0.0f },
	/* Turned round the right side's way and tilted back, so the ledge and
	 * its shoulders show over the thick part's back: from the front the
	 * thin part hides them below some 80 degrees of tilt. A little smaller,
	 * as the tilt shows the depth too. */
	.back = { 160.0f, 2.45f, 20.0f },
	/* The Controls tab's front pose, as tall as the Pixel 2's (3.15) */
	.tab = { 10.0f, 2.3f, 0.0f },
	/* Every vertex of every pose and turn, a halo's faint edge too, as
	 * --handheld-box measures it under --device brick (src/tourdevice.c,
	 * td_box_holds), with two units to spare: the tutorial's reaches x 14.5
	 * to 265.9 and y 97.7 to 426.5 on the screen, the Controls tab's x
	 * -108.6 to 109.2 and y -145.4 to 143.8 about the handheld's middle */
	.keep_tour = { 12, 95, 256, 334 },
	.keep_tab = { 111, 148, 224, 294 },
};
