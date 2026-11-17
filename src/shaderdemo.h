/* SPDX-License-Identifier: GPL-3.0-only */
/* The Shader and Strength lists' picture (DESIGN.md 11.3, a choice with a
 * picture): what a shader does, shown on a game-like scene of ZyperOS's own
 * at the handheld's own pixels, beside the list as the Display list's mini
 * screen is. The images are made by tools/shaderdemo.c from Diatom's own
 * filters (res/shaderdemo/<panel>/<filter>[-<strength>].png), one a filter
 * and a Strength step, so the picture is what the panel will show there; it
 * costs a PNG read when the cursor first comes to a choice. */
#ifndef ZYPEROS_SHADERDEMO_H
#define ZYPEROS_SHADERDEMO_H

#include "app.h"

/* The shader the k-th row of a Shader list is: the list offers only the
 * shaders this handheld draws (SHADERS' gpu, plat_gpu_shaders) */
int shader_offered(int k);

/* The notes under a Shader list's picture, a choice each, into `out` (n of
 * them, as many as the list has); the Strength list's note for every step
 * into `strength`, `steps` of them */
void shader_demo_notes(const char **out, int n);
void shader_demo_strength_notes(const char **out, int steps);

/* Shader `shader` at `strength` percent in the preview's box (units), its
 * close-up at the handheld's own pixels, the small screen framed. CRT
 * Curved's has its corner set in, with black round it, so its curve shows */
void shader_demo_draw(app *a, int shader, int strength, int x, int y, int w, int h);

#endif
