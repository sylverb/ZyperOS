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

/* The shader the k-th row of the Shader list of the system tagged `tag` is:
 * the list offers only the shaders this handheld draws (SHADERS' gpu,
 * plat_gpu_shaders) and that are for that system (shader_for) */
int shader_at(int k, const char *tag);

/* The notes under a Shader list's picture, a choice each, into `out` (n of
 * them, as many as the list of the system tagged `tag` has); the Strength
 * list's note for every step into `strength`, `steps` of them */
void shader_demo_notes(const char **out, int n, const char *tag);
void shader_demo_strength_notes(const char **out, int steps);

/* Shader `shader` at `strength` percent in the preview's box (units), its
 * close-up at the handheld's own pixels, the small screen framed. CRT
 * Curved's is its whole screen, its curve made larger to read that small */
void shader_demo_draw(app *a, int shader, int strength, int x, int y, int w, int h);

/* Its picture for a panel of `sw` x `sh` (res/shaderdemo/<sw>x<sh>/), read
 * once of the last few and kept: a texture with an alpha channel, opaque
 * (PLAT_TARGET_FORMAT), or NULL when the picture is not on the card, which
 * is said once on stderr */
SDL_Texture *shader_demo_texture(SDL_Renderer *r, int shader, int strength, int sw, int sh);

#endif
