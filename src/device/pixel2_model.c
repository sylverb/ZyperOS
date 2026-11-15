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
};
