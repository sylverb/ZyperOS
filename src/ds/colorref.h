/* SPDX-License-Identifier: GPL-3.0-only */
/* What a color setting is judged on (Screen Colors, src/panelgamma.h): a
 * chart of the colors pictures are made of - skin, sky, foliage, the
 * primaries and their mixes, and a grey step row - then a grey ramp in
 * sixteen steps from black to white and the hues in a band. Screen Colors
 * changes the whole screen, so the chart shows each setting as the panel
 * makes it. Drawn in a column beside the rows, or in a list's picture. Part
 * of the design system (src/ds/): no app.h, no panel.h. */
#ifndef ZYPEROS_DS_COLORREF_H
#define ZYPEROS_DS_COLORREF_H

#include <SDL.h>

/* The chart, the ramp and the band, stacked, filling `box` (units) */
void ds_color_ref_draw(SDL_Renderer *r, const SDL_Rect *box);

#endif
