/* SPDX-License-Identifier: GPL-3.0-only */
/* The record's textures and the tonearm's geometry: see vinyl.h. Drawn on
 * the CPU once, as signed distances (src/lookart.h). */
#include "vinyl.h"

#include "ds/sdf.h"
#include "lookart.h"
#include "mp4look.h"

#include <math.h>
#include <string.h>

/* The tonearm's metal and its headshell, its counterweight, the spindle's
 * steel: the materials, after the lab's */
static const la_rgba ARM_DARK   = { 201 / 255.f, 201 / 255.f, 194 / 255.f, 1 };
static const la_rgba ARM_LIGHT  = { 141 / 255.f, 139 / 255.f, 132 / 255.f, 1 };
static const la_rgba HEAD_DARK  = { 228 / 255.f, 226 / 255.f, 216 / 255.f, 1 };
static const la_rgba HEAD_LIGHT = { 85 / 255.f, 87 / 255.f, 79 / 255.f, 1 };
static const la_rgba WEIGHT     = { 74 / 255.f, 76 / 255.f, 71 / 255.f, 1 };
static const la_rgba STEEL      = { 184 / 255.f, 184 / 255.f, 176 / 255.f, 1 };
static const la_rgba SHADE      = { 0, 0, 0, 0.35f };

float vin_groove_r(int place, int n, float f)
{
	return mp4l_groove_r(place, n, f, VIN_GROOVE_OUT, VIN_GROOVE_IN);
}

float vin_arm_angle(float r)
{
	return mp4l_arm_angle(r, VIN_PIVOT_D, VIN_PIVOT_A, VIN_ARM_L);
}

/* The vinyl's color at radius `t` (units) from the spindle (q: cx cy, the
 * color's r g b, and the tracks): the grooves a satin sheen with a fine
 * ring every few units, the gaps between the album's songs glossy and dark,
 * the lead-in and lead-out smooth, the edge's bevel catching light */
static la_rgba face_col(const float *q, float x, float y)
{
	float r = hypotf(x - q[0], y - q[1]), k = 1.0f, a = 1.0f;
	int n = (int)q[5], i;
	la_rgba c = { q[2], q[3], q[4], 1 };

	if (r > VIN_R - 2.0f) {
		k = 1.25f;                                  /* the edge */
	} else if (r > VIN_GROOVE_OUT) {
		k = 0.92f;                                  /* the lead-in */
	} else if (r > VIN_GROOVE_IN) {
		/* Satin, lighter than the smooth parts, a fine ring every 5 units;
		 * under 4% so it never moires on the Pixel 2's 0.625 */
		k = 1.10f + 0.035f * sinf(r * 2.0f * (float)M_PI / 5.0f);
		for (i = 1; i < n; i++) {
			float g = VIN_GROOVE_OUT - (VIN_GROOVE_OUT - VIN_GROOVE_IN) * (float)i / (float)n;
			float d = fabsf(r - g);

			/* A gap between two songs: 3 units of smooth, darker vinyl */
			if (d < 2.0f) k = 0.78f + (1.10f - 0.78f) * ds_sd_clamp(d - 1.0f, 0, 1);
		}
	} else {
		k = 0.88f;                                  /* the lead-out */
	}
	c.r = ds_sd_clamp(c.r * k, 0, 1);
	c.g = ds_sd_clamp(c.g * k, 0, 1);
	c.b = ds_sd_clamp(c.b * k, 0, 1);
	c.a = a;
	return c;
}

/* The record's edge (q: cx cy, then face_col's) */
static float sd_record(const float *q, float x, float y)
{
	return hypotf(x - q[0], y - q[1]) - VIN_R;
}

/* The light on the grooves (q: cx cy): a bow tie of it across the record,
 * two opposite wedges, none toward the label and brightest four fifths of
 * the way out, soft at its sides - where a lamp over a turntable puts it */
static la_rgba light_col(const float *q, float x, float y)
{
	float dx = x - q[0], dy = y - q[1], r = hypotf(dx, dy) / VIN_R, a = atan2f(dy, dx);
	float best = 0, w;
	int k;

	for (k = 0; k < 2; k++) {
		float mid = k ? 1.14f : -2.0f, d = fabsf(remainderf(a - mid, 2 * (float)M_PI));

		w = ds_sd_clamp((0.42f - d) / 0.14f, 0, 1);
		if (w > best) best = w;
	}
	w = r < 0.45f ? 0 : r < 0.8f ? (r - 0.45f) / 0.35f * 0.10f : 0.10f - (r - 0.8f) / 0.2f * 0.08f;
	return (la_rgba){ 1, 1, 1, best * w };
}

/* The platter under the record: the theme's raised slab in its edge */
static SDL_Texture *deck(SDL_Renderer *r, const vin_tex *t)
{
	const int side = (int)ceilf(VIN_PLATTER_R) * 2 + 4;
	const float m = (float)side / 2;
	const float q[3] = { m, m, VIN_PLATTER_R }, rim[4] = { m, m, VIN_PLATTER_R - 1.5f, 3 };
	la_canvas c;

	if (!la_new(&c, side, side)) return NULL;
	la_paint(&c, la_disc, NULL, q, la_role(t->raised), 0, 0, side, side);
	la_paint(&c, la_ring, NULL, rim, la_role(t->edge), 0, 0, side, side);
	return la_texture(r, &c);
}

/* The record's face: the vinyl, its grooves and the album's songs as
 * rings; the label goes over it, turning, and the light over that */
static SDL_Texture *face(SDL_Renderer *r, const vin_tex *t)
{
	const int side = (int)ceilf(VIN_R) * 2 + 4;
	const float m = (float)side / 2;
	const float q[6] = { m, m, ((t->vinyl >> 16) & 255) / 255.f, ((t->vinyl >> 8) & 255) / 255.f,
	                     (t->vinyl & 255) / 255.f, (float)t->tracks };
	la_canvas c;

	if (!la_new(&c, side, side)) return NULL;
	la_paint(&c, sd_record, face_col, q, (la_rgba){ 0 }, m - VIN_R - 1, m - VIN_R - 1, m + VIN_R + 1,
	         m + VIN_R + 1);
	return la_texture(r, &c);
}

/* Over the label: the light on the grooves, the shadow round the label's
 * edge, and the spindle's steel through its hole */
static SDL_Texture *light(SDL_Renderer *r)
{
	const int side = (int)ceilf(VIN_R) * 2 + 4;
	const float m = (float)side / 2;
	/* The label's edge: 4 units on the label's line, over the fan's
	 * straight sides, which stepped at the Pixel 2's pixels without it */
	const float disc[3] = { m, m, VIN_R }, edge[4] = { m, m, VIN_LABEL_R, 4 };
	const float pin[3] = { m, m, VIN_HOLE_R }, pin_rim[4] = { m, m, VIN_HOLE_R, 1.5f };
	la_canvas c;

	if (!la_new(&c, side, side)) return NULL;
	la_paint(&c, la_disc, light_col, disc, (la_rgba){ 0 }, 0, 0, side, side);
	la_paint(&c, la_ring, NULL, edge, SHADE, m - VIN_LABEL_R - 4, m - VIN_LABEL_R - 4, m + VIN_LABEL_R + 4,
	         m + VIN_LABEL_R + 4);
	la_paint(&c, la_disc, NULL, pin, STEEL, m - 8, m - 8, m + 8, m + 8);
	la_paint(&c, la_ring, NULL, pin_rim, SHADE, m - 8, m - 8, m + 8, m + 8);
	return la_texture(r, &c);
}

/* The tonearm, pointing right from its pivot at (VIN_ARM_PX, VIN_ARM_PY):
 * the counterweight behind it, the tube, the headshell with the stylus at
 * the needle's place, VIN_ARM_L from the pivot */
static SDL_Texture *arm(SDL_Renderer *r, const vin_tex *t)
{
	const float px = VIN_ARM_PX, py = VIN_ARM_PY, nx = px + VIN_ARM_L;
	const int w = (int)ceilf(nx) + 16, h = (int)(2 * py);
	const float weight[5] = { 6, py - 13, px - 14, py + 13, 6 };
	const float tube[5] = { px, py, nx - 40, py, 8 };
	const float head[5] = { nx - 46, py - 11, nx + 10, py + 11, 5 };
	const float stylus[3] = { nx, py, 3 }, bearing[3] = { px, py, 11 };
	la_canvas c;

	if (!la_new(&c, w, h)) return NULL;
	la_paint(&c, la_rbox, NULL, weight, WEIGHT, 0, 0, px, h);
	la_paint(&c, la_seg, NULL, tube, t->dark ? ARM_DARK : ARM_LIGHT, px - 6, py - 6, nx - 34, py + 6);
	la_paint(&c, la_rbox, NULL, head, t->dark ? HEAD_DARK : HEAD_LIGHT, nx - 48, py - 13, nx + 12, py + 13);
	la_paint(&c, la_disc, NULL, stylus, SHADE, nx - 5, py - 5, nx + 5, py + 5);
	la_paint(&c, la_disc, NULL, bearing, la_role(t->edge), px - 13, py - 13, px + 13, py + 13);
	return la_texture(r, &c);
}

/* The tonearm's base, which does not turn: the theme's surface in its edge,
 * the bearing's ring */
static SDL_Texture *base(SDL_Renderer *r, const vin_tex *t)
{
	const int side = 76;
	const float m = side / 2.0f;
	const float plate[3] = { m, m, 34 }, rim[4] = { m, m, 32, 4 };
	la_canvas c;

	if (!la_new(&c, side, side)) return NULL;
	la_paint(&c, la_disc, NULL, plate, la_role(t->ground), 0, 0, side, side);
	la_paint(&c, la_ring, NULL, rim, la_role(t->edge), 0, 0, side, side);
	return la_texture(r, &c);
}

void vin_free(vin_tex *t)
{
	if (t->deck) SDL_DestroyTexture(t->deck);
	if (t->face) SDL_DestroyTexture(t->face);
	if (t->light) SDL_DestroyTexture(t->light);
	if (t->arm) SDL_DestroyTexture(t->arm);
	if (t->base) SDL_DestroyTexture(t->base);
	t->deck = t->face = t->light = t->arm = t->base = NULL;
}

bool vin_ready(SDL_Renderer *r, vin_tex *t, const ds_theme *th, bool dark, unsigned vinyl, int tracks)
{
	bool theme_same = t->deck && t->arm && t->base && t->light && t->dark == dark &&
	                  !memcmp(&t->raised, &th->raised, sizeof t->raised) &&
	                  !memcmp(&t->edge, &th->edge, sizeof t->edge) &&
	                  !memcmp(&t->ground, &th->surface, sizeof t->ground);

	if (tracks < 1) tracks = 1;
	if (!theme_same) {
		if (t->deck) SDL_DestroyTexture(t->deck);
		if (t->arm) SDL_DestroyTexture(t->arm);
		if (t->base) SDL_DestroyTexture(t->base);
		if (t->light) SDL_DestroyTexture(t->light);
		t->raised = th->raised;
		t->edge = th->edge;
		t->ground = th->surface;
		t->dark = dark;
		t->deck = deck(r, t);
		t->arm = arm(r, t);
		t->base = base(r, t);
		t->light = light(r);
	}
	if (!t->face || t->vinyl != vinyl || t->tracks != tracks) {
		if (t->face) SDL_DestroyTexture(t->face);
		t->vinyl = vinyl;
		t->tracks = tracks;
		t->face = face(r, t);
	}
	return t->deck && t->face && t->light && t->arm && t->base;
}
