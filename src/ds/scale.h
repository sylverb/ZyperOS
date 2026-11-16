/* SPDX-License-Identifier: GPL-3.0-only */
/* A graded bar (DESIGN.md 11.3, a row with a scale): the whole of a value's
 * range drawn as a track whose colors say what the scale does (cool to warm,
 * green to magenta, flat to strong, grey to vivid, darker to lighter), its
 * marks at the ends, the quarters and zero, zero's the tallest, the value a
 * knob on it and its signed number after it (-3, 0, +5). A menu row carries
 * one as its value (ds_scale_value), and the panel draws it in the value's
 * column, the labels' widest from the left, so the bars of a page line up.
 * Screen Colors' rows are the first (src/panelgamma.h). Part of the design
 * system (src/ds/): no app.h, no panel.h. */
#ifndef ZYPEROS_DS_SCALE_H
#define ZYPEROS_DS_SCALE_H

#include <SDL.h>
#include <stdbool.h>

typedef enum {
	DS_SCALE_TEMPERATURE,   /* cool blue, white, warm orange */
	DS_SCALE_TINT,          /* green, white, magenta */
	DS_SCALE_CONTRAST,      /* two greys close together, to black and white */
	DS_SCALE_SATURATION,    /* a color's grey, to the color at its fullest */
	DS_SCALE_GAMMA,         /* darker mid-tones, to lighter */
	DS_SCALE_KINDS
} ds_scale_kind;

/* A row's value that is a scale, `v` from -range to +range, written into
 * `buf` (DS_SCALE_VALUE bytes): the panel knows it by its first bytes */
#define DS_SCALE_VALUE 24
const char *ds_scale_value(char buf[DS_SCALE_VALUE], ds_scale_kind kind, int v, int range);
/* Whether a row's value is a scale, and its parts */
bool ds_scale_is(const char *value);
bool ds_scale_parse(const char *value, ds_scale_kind *kind, int *v, int *range);

/* The number's room after the bar: "+10" and "-10" in the menu's face */
int ds_scale_num_w(void);
/* The least a bar is, so a scale row is never a sliver */
#define DS_SCALE_MIN_W 192

/* The scale in `box` (units): the track across it less the number's room,
 * centered on its height. `on_fill`: drawn over the focus fill (the row under
 * the cursor), its marks, knob and number in on-accent. */
void ds_scale_draw(SDL_Renderer *r, const SDL_Rect *box, ds_scale_kind kind, int v, int range, bool on_fill);

#endif
