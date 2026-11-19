/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_VINYL_H
#define ZYPEROS_VINYL_H

/* MP4's Vinyl look drawn (DESIGN.md 11.64): a 12-inch record, 302 mm
 * across, at 33 1/3 turns a minute on its platter, pressed in its cover's
 * main color (my pick, 2026-11-19), darkened so its grooves read, the cover
 * as its label; the tonearm's needle on the song's place in the album. A
 * drawing in units of a record 424 across (1.404 units a millimetre), each
 * part's millimetres beside it.
 *
 * What does not turn is drawn once into textures: the platter and the
 * tonearm's base, the record's face - its grooves, the album's tracks as
 * rings, the label's edge and the spindle - and the light on it, which
 * stays where it is while the record turns under it. The tonearm is a
 * texture turned on its pivot. The label is the cover itself, drawn each
 * frame as a disc of triangles (SDL_RenderGeometry) whose picture turns,
 * so it is round with no mask and turns with the record. */

#include <SDL.h>
#include <stdbool.h>

#include "design.h"

/* The record and its platter: a 12-inch record, 151 mm a side of its
 * spindle, on a platter's rim of 8.5 mm round it */
#define VIN_R         212.0f
#define VIN_PLATTER_R 224.0f
/* The grooves, 146 to 60 mm out, the lead-in outside them and the
 * lead-out inside; the label 100 mm across; the spindle 7.2 */
#define VIN_GROOVE_OUT 205.0f
#define VIN_GROOVE_IN  84.0f
#define VIN_LABEL_R    70.0f
#define VIN_HOLE_R     5.0f
/* The tonearm: its pivot 215 mm from the spindle, up and right of it at
 * 40 degrees, its needle 230 mm from the pivot (a Technics' geometry) */
#define VIN_PIVOT_D    302.0f
#define VIN_PIVOT_A    (-40.0f)
#define VIN_ARM_L      323.0f
/* The tonearm's texture: its pivot at (VIN_ARM_PX, VIN_ARM_PY) in it, the
 * arm going right from there to its needle */
#define VIN_ARM_PX     48.0f
#define VIN_ARM_PY     24.0f

typedef struct {
	SDL_Texture *deck;      /* the platter, 2 x VIN_PLATTER_R across */
	SDL_Texture *face;      /* the record's face round its label */
	SDL_Texture *light;     /* the light on it, still */
	SDL_Texture *arm;       /* the tonearm, turned on its pivot */
	SDL_Texture *base;      /* the tonearm's base under the pivot */
	ds_rgb raised, edge, ground;     /* the deck's and arm's colors, from the theme */
	bool dark;
	unsigned vinyl;         /* the face's: the vinyl's color */
	int tracks;             /* ...and the album's tracks */
} vin_tex;

/* The textures for the theme and the record (`vinyl` its color, 0xRRGGBB;
 * `tracks` the rings), each made again only when what it shows changes.
 * False when one would not make. */
bool vin_ready(SDL_Renderer *r, vin_tex *t, const ds_theme *th, bool dark, unsigned vinyl, int tracks);
void vin_free(vin_tex *t);

/* The needle's radius for song `place` of `n`, `f` into it: the grooves
 * shared out evenly, the first song's start at their outer edge */
float vin_groove_r(int place, int n, float f);

/* The tonearm's angle (degrees, SDL's clockwise) that puts its needle at
 * radius `r` from the spindle */
float vin_arm_angle(float r);

#endif
