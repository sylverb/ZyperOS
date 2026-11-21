/* SPDX-License-Identifier: GPL-3.0-only */
/* What portkeys shows over a program of its own, in one picture on the
 * panel's cursor window (tools/portkeys.c): the volume bar, a notice
 * ("Getting hot"), and over your PICO-8 the low-battery disc and On-Screen
 * Stats, each where and as Diatom draws it over a game
 * (diatom/port/gles_draw.h, draw_scene): the stats line under the rest, the
 * notice, the bar, the disc on top. The window is one plane, so whatever is
 * up goes into one buffer the size of all of it together, turned onto a
 * portrait panel as the bar always was.
 *
 * And the stats line's frames a second, counted as px2-shot --fps counts
 * them: at every vertical blank, whether the panel shows a new framebuffer.
 *
 * Pure: no DRM, no device, no allocation. tools/pkover-check.c holds it. */
#ifndef ZYPEROS_PKOVER_H
#define ZYPEROS_PKOVER_H

#include <stdbool.h>
#include <stdint.h>

/* A place on the landscape picture, in the panel's pixels (640 x 480 on the
 * Pixel 2, the surface Diatom draws on) */
typedef struct { int x, y, w, h; } pk_rect;

/* One layer: premultiplied ARGB, w x h, its top left at (x, y); none while
 * px is NULL */
typedef struct {
	const uint32_t *px;
	int x, y, w, h;
} pk_layer;

enum { PK_LAYER_HUD, PK_LAYER_NOTICE, PK_LAYER_BAR, PK_LAYERS };

typedef struct {
	pk_layer layer[PK_LAYERS];   /* drawn in this order */
	bool disc;                   /* the low-battery disc, on top */
	int lw, lh;                  /* the landscape surface */
} pk_scene;

/* The low-battery disc, Diatom's (draw_low_battery): radius 9, centered
 * 34 pixels from the top and right edges, in the warning red, opaque */
#define PK_DISC_R    9
#define PK_DISC_IN   34
#define PK_DISC_ARGB 0xFFE04848u
pk_rect pkover_disc_rect(int lw);
/* Whether it shows at `now_ms` for the battery's level (status.h's
 * status_batt_level: 0 fine or charging, 1 low, 2 critical): always when
 * low, the even seconds when critical (blinking), never else */
bool pkover_disc_on(int level, uint64_t now_ms);

/* Everything up, together; w 0 when nothing is */
pk_rect pkover_union(const pk_scene *s);

/* Where the window goes and how big its buffer is for the picture `u`:
 * turned onto a portrait panel `ph` pixels tall (the landscape top along
 * the panel's left edge: a landscape (x, y) is the panel's (y, ph - 1 - x))
 * or as it is on a landscape one. The buffer's width is padded to 16
 * pixels: the cursor ioctl reads rows of exactly width * 4 bytes, and
 * rockchip pads a dumb buffer's rows to 64 bytes (portkeys.c, buffers). */
typedef struct {
	int x, y;          /* the window's top left on the panel */
	int bw, bh;        /* the buffer: bw pixels a row (padded), bh rows */
	int w, h;          /* the picture in it, unpadded */
} pk_window;
pk_window pkover_window(pk_rect u, bool turned, int ph);

/* The scene, its picture `u`, into `out` as the window `win` takes it: rows
 * of win.bw pixels, everything outside the layers clear. `scratch` is
 * u.w * u.h pixels of room. */
void pkover_render(const pk_scene *s, pk_rect u, pk_window win, bool turned, uint32_t *out,
                   uint32_t *scratch);

/* ---- frames a second ------------------------------------------------------ */

/* Every vertical blank's framebuffer (fb_id) as it is seen; a change is a
 * frame. Every PK_FPS_EVERY_MS (On-Screen Stats' half second), fps is the
 * frames over the time since the last; below zero before the first. */
#define PK_FPS_EVERY_MS 500
typedef struct {
	uint32_t last_fb;
	bool seen;
	int frames;
	uint64_t since;
	double fps;
} pk_fps;
void pkover_fps_init(pk_fps *f);
/* True when fps changed at this blank */
bool pkover_fps_blank(pk_fps *f, uint32_t fb_id, uint64_t now_ms);
/* No blanks seen for a while (the program stopped, the display gone): the
 * count starts again and fps is not known */
void pkover_fps_lost(pk_fps *f);

#endif
