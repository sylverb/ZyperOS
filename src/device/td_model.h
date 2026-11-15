/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_TD_MODEL_H
#define ZYPEROS_TD_MODEL_H

/* A handheld's 3D drawing as data (docs/superpowers/specs/
 * 2026-11-14-device-modular-design.md, section 3): where each of its keys is
 * and on which face, the views the tutorial and the Controls tab turn it to,
 * the boxes its kept textures need, and which art draws its body. What the
 * tutorial and the Controls tab work out from this - which way to turn for a
 * lesson, which way to lean for a row, which keys a lesson lights - follows
 * from the keys' places, so a second handheld is its own data and its own
 * art, not a copy of the logic.
 *
 * The art itself stays code (src/tourdevice.c): a handheld's caps, tabs and
 * edges are drawn by hand, and each device's are its own. It reads the key
 * places from here. Millimetres, as tools/build-pixel2-model.py measures the
 * Pixel 2: x right, z up, from the body's middle; on a side face, x is the
 * face's own across (the depth, front negative). No SDL, so the profiles and
 * the checks link it as it is. */

#include <stdbool.h>

#include "../tdkey.h"

typedef enum { TD_FACE_FRONT, TD_FACE_LEFT, TD_FACE_RIGHT, TD_FACE_TOP } td_face;

/* Which art draws the body and its keys (src/tourdevice.c) */
typedef enum { TD_ART_PIXEL2 } td_art;

/* A key: where on which face, and how big - a round key's radius, an
 * elongated one's half length, a shoulder's half width */
typedef struct {
	td_key  key;
	td_face face;
	float   x, z, r;
} td_part;

/* How far turned (degrees, more than 0 shows the right side), how big
 * (units a millimetre), how far tilted back (degrees, more than 0 shows the
 * top) */
typedef struct { float deg, s, pitch; } td_view;

/* A kept texture's box: from the pose's middle, left and up, and its size */
typedef struct { int l, t, w, h; } td_box;

typedef struct td_model {
	const char *name;
	td_art      art;
	float       body_w, body_h;
	/* Keys within this of the middle, across, lean the Controls tab no way:
	 * SELECT and START between the two halves */
	float       lean_centre;
	const td_part *parts;
	int         n_parts;
	/* The tutorial's views: facing front, turned to show the right side or
	 * the left, front with both sides' keys out of the edges, and tilted
	 * back to show the top */
	td_view     front, right, left, both, top;
	/* The kept textures' boxes: the tutorial's (from the screen's corner,
	 * as it stands still in its column) and the Controls tab's (from the
	 * pose's middle) */
	td_box      keep_tour, keep_tab;
} td_model;

/* `k` on `m`, or NULL where `m` has no such key */
const td_part *td_part_of(const td_model *m, td_key k);

extern const td_model td_model_pixel2;

#endif
