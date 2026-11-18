/* SPDX-License-Identifier: GPL-3.0-only */
/* The screens' shader (src/screenfx.h) drawn with SDL, for each device's
 * plat_present: what plat_screen_shader asked for, its mask as a texture
 * (made once a shader, a strength and a size, then a multiply a frame), and
 * CRT Curved's bend as a mesh. Every device shares it, as it shares
 * platform.c. */
#ifndef ZYPEROS_SCREENFX_DRAW_H
#define ZYPEROS_SCREENFX_DRAW_H

#include <SDL.h>
#include <stdbool.h>

/* A shader is on: plat_screen_shader asked for an old screen this device
 * draws (plat_gpu_shaders). False is Off: nothing below draws. */
bool sfx_on(void);
/* ...and it is CRT Curved, whose frame goes through sfx_draw_curved */
bool sfx_curved(void);

/* The mask over `dst` (w x h, the picture's own size before any turn),
 * turned `angle` degrees about dst's middle as SDL_RenderCopyEx turns: one
 * textured multiply over what is drawn there */
void sfx_draw_mask(SDL_Renderer *r, int w, int h, const SDL_Rect *dst, double angle);

/* `src` drawn into `dst` bent by CRT Curved's curve, turned as above: in
 * place of SDL_RenderCopyEx(r, src, NULL, dst, angle, ...) */
void sfx_draw_curved(SDL_Renderer *r, SDL_Texture *src, const SDL_Rect *dst, double angle);

/* The w x h frame drawn so far into r's target, bent by the curve where it
 * is: for a device that draws straight into its window (the Brick's file,
 * the host's), read back and drawn again. False when it could not. */
bool sfx_bend_target(SDL_Renderer *r, int w, int h);

/* The renderer is about to go (plat_video_quit): its textures go now */
void sfx_forget(void);

#endif
