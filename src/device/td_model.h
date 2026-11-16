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
 * face's own across (the depth, front negative); on the top edge and on a
 * back ledge, a shoulder's x, its top's z and its half width. No SDL, so the
 * profiles and the checks link it as it is. */

#include <stdbool.h>

#include "../tdkey.h"

/* The faces a key can be on: the front, the sides, the top edge (the Pixel
 * 2's shoulders), and a ledge on the back, the top of a thicker part below
 * the top edge (the Brick's shoulders), which no front view shows */
typedef enum { TD_FACE_FRONT, TD_FACE_LEFT, TD_FACE_RIGHT, TD_FACE_TOP, TD_FACE_LEDGE } td_face;

/* Which art draws the body and its keys (src/tourdevice.c) */
typedef enum { TD_ART_PIXEL2, TD_ART_BRICK } td_art;

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
	 * the left, front with both sides' keys out of the edges, tilted back to
	 * show the top, and turned round to the back, tilted, to show a back
	 * ledge's keys (a model with no key on a face has its front there) */
	td_view     front, right, left, both, top, back;
	/* The Controls tab's front pose: its turn and its size (its pitch 0);
	 * its leans are off it (ct_lean) */
	td_view     tab;
	/* The kept textures' boxes: the tutorial's (from the screen's corner,
	 * as it stands still in its column) and the Controls tab's (from the
	 * pose's middle) */
	td_box      keep_tour, keep_tab;
} td_model;

/* `k` on `m`, or NULL where `m` has no such key */
const td_part *td_part_of(const td_model *m, td_key k);

/* Whether the keys `lit` (1u << td_key) of `m` are on its back ledge and
 * off it: no one view shows both, so the handheld turns between the view
 * of the others and its back, front first (td_swing) */
bool td_spins(const td_model *m, unsigned lit);

/* How far round from the front view to the back, 0 to 1, `ms` after the
 * handheld came to rest on its front (0 before): held there for
 * **hold-spin**, turned in **spin** on the turn curve, held on its back,
 * turned back, and again, for as long as the keys stay lit */
float td_swing(int ms);

/* `a` to `b`, `k` of the way: the turn, the size and the tilt together */
td_view td_view_mix(td_view a, td_view b, float k);

extern const td_model td_model_pixel2;
extern const td_model td_model_brick;

#endif
