/* SPDX-License-Identifier: GPL-3.0-only */
/* The handhelds' models (src/device/td_model.h): every key once, and the
 * GKD Pixel 2's places, views and kept boxes as src/tourdevice.c drew them
 * before they were data (its FACE[], DPAD_*, MENU_*, POWER_*, VOL*_Z,
 * SHOULDER[], SH_TOP, the FRONT/RIGHT/LEFT/BOTH/TOP poses, the tutorial's
 * 304 x 382 box and the Controls tab's 256 x 332), so the drawing that reads
 * them draws the same pixels. No SDL. */
#include <stdio.h>

#include "../src/device/td_model.h"

static int fails, checks;

static void ck(int cond, const char *what)
{
	checks++;
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

static int is(const td_model *m, td_key k, td_face f, float x, float z, float r)
{
	const td_part *p = td_part_of(m, k);

	return p && p->face == f && p->x == x && p->z == z && p->r == r;
}

static int view_is(td_view v, float deg, float s, float pitch)
{
	return v.deg == deg && v.s == s && v.pitch == pitch;
}

int main(void)
{
	const td_model *m = &td_model_pixel2;
	int i, j;

	for (i = 0; i < m->n_parts; i++) {
		ck(m->parts[i].key >= 0 && m->parts[i].key < TD_KEYS, "each part a key of the handheld");
		for (j = i + 1; j < m->n_parts; j++)
			ck(m->parts[j].key != m->parts[i].key, "no key twice");
	}
	ck(m->art == TD_ART_PIXEL2 && m->body_w == 63.0f && m->body_h == 81.0f && m->lean_centre == 8.0f,
	   "pixel2: its art, its 63 x 81 body, SELECT and START in the middle");
	ck(is(m, TD_DPAD, TD_FACE_FRONT, -16.9f, -21.0f, 9.0f), "pixel2: the d-pad");
	ck(is(m, TD_A, TD_FACE_FRONT, 23.6f, -21.5f, 3.05f) && is(m, TD_B, TD_FACE_FRONT, 16.9f, -28.2f, 3.05f) &&
	   is(m, TD_X, TD_FACE_FRONT, 16.9f, -14.8f, 3.05f) && is(m, TD_Y, TD_FACE_FRONT, 10.2f, -21.5f, 3.05f),
	   "pixel2: A B X Y");
	ck(is(m, TD_SELECT, TD_FACE_FRONT, -3.8f, -29.6f, 2.85f) && is(m, TD_START, TD_FACE_FRONT, 4.0f, -29.6f, 2.85f),
	   "pixel2: SELECT and START");
	ck(is(m, TD_MENU, TD_FACE_RIGHT, -1.54f, 17.13f, 2.65f) && is(m, TD_POWER, TD_FACE_RIGHT, -1.74f, 6.65f, 4.9f),
	   "pixel2: MENU and POWER on the right side");
	ck(is(m, TD_VOLUP, TD_FACE_LEFT, -1.82f, 16.52f, 2.95f) && is(m, TD_VOLDN, TD_FACE_LEFT, -1.82f, 7.05f, 2.95f),
	   "pixel2: the volume keys on the left side");
	ck(is(m, TD_L1, TD_FACE_TOP, -23.9f, 41.7f, 7.55f) && is(m, TD_R1, TD_FACE_TOP, 23.9f, 41.7f, 7.55f) &&
	   is(m, TD_L2, TD_FACE_TOP, -12.6f, 41.7f, 3.7f) && is(m, TD_R2, TD_FACE_TOP, 12.6f, 41.7f, 3.7f),
	   "pixel2: the shoulders on the top edge, L1/R1 outside");
	ck(!td_part_of(m, TD_F1) && !td_part_of(m, TD_F2) && !td_part_of(m, TD_KEYS), "pixel2: no F1, F2");
	ck(view_is(m->front, 10.0f, 3.55f, 0.0f) && view_is(m->right, 52.0f, 3.95f, 0.0f) &&
	   view_is(m->left, -52.0f, 3.95f, 0.0f) && view_is(m->both, 0.0f, 3.45f, 0.0f) &&
	   view_is(m->top, 10.0f, 3.55f, TD_PITCH_DEG), "pixel2: the tutorial's five views");
	ck(m->keep_tour.l == 0 && m->keep_tour.t == 66 && m->keep_tour.w == 304 && m->keep_tour.h == 382,
	   "pixel2: the tutorial's kept box");
	ck(m->keep_tab.l == 128 && m->keep_tab.t == 176 && m->keep_tab.w == 256 && m->keep_tab.h == 332,
	   "pixel2: the Controls tab's kept box");
	if (fails) {
		printf("model: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("model: %d checks passed\n", checks);
	return 0;
}
