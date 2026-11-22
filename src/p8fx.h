/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_P8FX_H
#define ZYPEROS_P8FX_H

/* Your PICO-8's Shader and Border (DESIGN.md 11.55): the PICO-8 shelf's
 * Shader, Strength and Border rows applied to Lexaloffle's own PICO-8 as
 * to a game in Diatom (fake08's).
 *
 * pico8_64 draws with the system's SDL2 itself: a 128x128 picture (or a
 * whole multiple of it, PICO-8's "preblit") into a streaming texture, which
 * its SDL_RenderCopy puts where its display mode says, its SDL_RenderClear
 * black round it, every frame. sd/pixel2/PICO-8.sh puts a small library in
 * front of SDL in its process (LD_PRELOAD, src/p8fx/shim.c), which draws
 * that copy with Diatom's own old screens (diatom/port/gles_shaders.h) and
 * the Border under it. No change to the system image: the library comes
 * with ZyperOS's files (ZyperOS/pico8/p8fx.so).
 *
 * What it draws is this file, P8FX_FILE, which the launcher writes as it
 * hands over to PICO-8 (launch_port), the in-game menu over PICO-8 writes
 * again when a row changes, and portkeys's Next Shader rewrites the shader
 * line of (p8fx_next_shader). The shim looks at it a few times a second, so
 * a change shows at once. Text, one "key value" line each:
 *
 *   shader crt                   a SHADERS name (src/app.h); "nearest" is Off
 *   strength 70                  10 to 100 (src/strength.h)
 *   border blur                  "off", "blur", or "picture"
 *   border_file /tmp/...dtov     the picture for "picture": a DTOV file
 *                                (Diatom's overlay format) the panel's size
 *
 * Written whole under another name and renamed in, so the shim never reads
 * half of one. Whatever the shim cannot use (no file, a shader it has not,
 * a picture it cannot read, a GPU without OpenGL ES 3) it draws as PICO-8
 * does, never a black screen.
 *
 * Plain C, no SDL: the launcher, portkeys and the shim share it, and
 * tools/p8fx-check.c holds it. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "i18n_ids.h"

#define P8FX_FILE         "/tmp/zyperos-pico8fx"
/* The Border's picture for your PICO-8 (Your Own), made by the launcher */
#define P8FX_BORDER_FILE  "/tmp/zyperos-pico8-border.dtov"
/* The PICO-8 shelf's tag: its shader.PICO8, shader_strength.PICO8 and
 * border.PICO8 are what is drawn */
#define P8FX_TAG          "PICO8"
/* PICO-8's screen, each way */
#define P8FX_SCREEN       128

typedef enum { P8FX_BORDER_OFF, P8FX_BORDER_BLUR, P8FX_BORDER_PICTURE } p8fx_border;

typedef struct {
	char        shader[24];      /* a SHADERS name; "nearest" for Off */
	int         strength;        /* 10 to 100 */
	p8fx_border border;
	char        border_file[256];
} p8fx_look;

/* Off, 100%, no border: what is drawn with no file */
void p8fx_look_default(p8fx_look *l);

/* The file at `path` read into `l`: true when it was there; a line it does
 * not know is passed over, a value out of range is the default's */
bool p8fx_look_read(const char *path, p8fx_look *l);

/* `l` written to `path`, whole, by a rename: true when it is there */
bool p8fx_look_write(const char *path, const p8fx_look *l);

/* Whether `l` changes anything PICO-8 draws (a shader or a border) */
bool p8fx_look_on(const p8fx_look *l);

/* The words the file keeps for a border, and the kind for a word */
const char *p8fx_border_word(p8fx_border b);
p8fx_border p8fx_border_parse(const char *word);

/* The SHADERS names your PICO-8 is offered, in the order its Shader row
 * and Next Shader step, on a handheld whose GPU draws the old screens
 * (plat_gpu_shaders), the only kind the shim draws on: Off,
 * Sharp-Shimmerless, CRT, CRT Curved, Scanlines and LCD Grid. Never LCD No
 * Backlight, which is the Game Boys'. The count, up to `max` names into
 * `names`. tools/p8fx-check.c holds it to the launcher's shader_step for
 * P8FX_TAG. */
int p8fx_shaders(const char **names, int max);

/* The same where the GPU draws none (`gpu` false): Off and
 * Sharp-Shimmerless */
int p8fx_shaders_on(bool gpu, const char **names, int max);

/* The words for shader `name` ("CRT"), Off's for one that is none */
str_id p8fx_shader_text(const char *name);

/* `name` stepped by `d` (+1 or -1) round p8fx_shaders_on(gpu), into `out`;
 * a name that is none steps from Off */
void p8fx_shader_step(bool gpu, const char *name, int d, char *out, size_t n);

/* Next Shader over your PICO-8 (portkeys's hotkey, src/hkstore.h): the
 * shader drawn now (P8FX_FILE's, else shader.PICO8 in the device database
 * `dev`, else the old every-system `shader`, else Off) stepped on round
 * p8fx_shaders, kept as shader.PICO8, and P8FX_FILE rewritten with it, its
 * other lines as they were. The new name into `name`, for the pop-up
 * (p8fx_shader_text). False, nothing changed, when the database cannot be
 * written. In src/p8fx_step.c, which needs SQLite: `dev` is a sqlite3. */
struct sqlite3;
bool p8fx_next_shader(struct sqlite3 *dev, char *name, size_t n);
/* The same with the look file at `file` (tools/p8fx-check.c's scratch one) */
bool p8fx_next_shader_at(struct sqlite3 *dev, const char *file, char *name, size_t n);

/* PICO-8's picture at its own 128x128 pixels from a texture upload:
 * `pixels` the texture's `tex_w` x `tex_h` (a whole multiple of 128 each
 * way, the same both ways), `pitch` bytes a row, four bytes a pixel, into
 * `out` as the same four bytes. PICO-8 makes its larger pictures by
 * repeating each pixel (codo_stretch_blit), so the first of each square is
 * the pixel. False, `out` untouched, for any other size. */
bool p8fx_shrink(const void *pixels, int pitch, int tex_w, int tex_h,
                 uint8_t out[P8FX_SCREEN * P8FX_SCREEN * 4]);

/* Whether the rect `x`, `y`, `w` x `h` covers an `out_w` x `out_h` screen,
 * leaving no room for a border */
bool p8fx_covers(int x, int y, int w, int h, int out_w, int out_h);

#endif
