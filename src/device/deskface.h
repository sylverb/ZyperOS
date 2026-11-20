/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_DESKFACE_H
#define ZYPEROS_DESKFACE_H

/* Desk mode's handheld (src/device/desk.h): the handheld of the profile
 * drawn around the window's screen, from its model's flat drawing
 * (td_model.h's td_flat, in src/device/<id>_model.c), the same data the
 * Simulator draws from, in the Simulator's materials: a body lit from
 * above with a soft shadow, a glass bezel, keys that stand up and go down
 * when pressed. Clicked with the mouse; each key names its keyboard key on a
 * small keycap when the pointer is over it, or all of them while Option is
 * held. Host only, as desk.c. */

#include <SDL.h>
#include <stdbool.h>

#include "../inbutton.h"
#include "td_model.h"

/* The face of a handheld's model; false for none or one with no flat
 * drawing (the host's desktop, which borrows a model), which desk mode
 * shows as its screen alone */
bool face_init(const td_model *m);

/* The window around a face at s points a millimetre, and where the screen's
 * picture goes in it, in points */
void face_size(float s, float *w, float *h, SDL_FRect *screen);

/* The screen's lit width, millimetres: s is the screen's points over it */
float face_screen_mm(void);

/* Everything under the screen's picture (the body, the keys, the bezel),
 * at s points a millimetre into a renderer `ratio` pixels a point; `held`
 * the buttons down (1 << in_button) */
void face_draw(SDL_Renderer *r, float s, float ratio, Uint32 held);

/* Over the picture: the glass's sheen, and the keys' names: the one under
 * the pointer (`hover`), every one with `all` */
void face_draw_over(SDL_Renderer *r, float s, float ratio, in_button hover, bool all);

/* The key at a point in the window (points), or IN_NONE */
in_button face_hit(float s, float x, float y);

/* Text in the face's typeface (the launcher's, P_FONT), centred on cx, cy
 * in points: for desk.c's help card and notes too */
void face_text(SDL_Renderer *r, const char *s, float pt, SDL_Color c, float cx, float cy, float ratio,
               bool bold);
/* Its width in points */
float face_text_w(const char *s, float pt, float ratio, bool bold);
void face_forget(void);

/* The vector kit, for desk.c's card: a rounded rectangle (points) filled
 * top to bottom from `a` to `b`, edges smoothed; and a soft shadow under one */
void face_rrect(SDL_Renderer *r, SDL_FRect box, float radius, SDL_Color a, SDL_Color b, float ratio);
void face_shadow(SDL_Renderer *r, SDL_FRect box, float radius, float blur, float dy, Uint8 alpha, float ratio);

#endif
