/* SPDX-License-Identifier: MPL-2.0 */
/* Every picture Diatom's GPU ports draw, the right way up on the panel: the
 * shared presenting code (port/gles_state.h, gles_draw.h, gles_objects.h,
 * gles_present.h, gles_gpu.h) built as one port builds it, its presenting
 * thread running on Mesa's llvmpipe, each frame read back from its target
 * the way that device's display reads it, and every pixel held to where the
 * picture puts it. For make check-gpu-upright (tools/gpu-upright-check.py),
 * which builds it twice, with the hooks each port defines:
 *
 *   -DUPRIGHT_BRICK   the TrimUI Brick's target (port/brick.c): a page of
 *                     fb0 as a texture under a framebuffer object, on a
 *                     context with no surface. The display reads the page's
 *                     memory from its start, top row first, and GL puts a
 *                     texture's first row of memory at its t = 0, the bottom
 *                     of what it draws there (y = 0), which is where the
 *                     dma-buf import puts the page's first bytes. Read here
 *                     with glReadPixels, whose first row is that one. A
 *                     plain texture stands in for the page's image: Mesa
 *                     makes no dma-buf of ordinary memory.
 *   -DUPRIGHT_PIXEL2  the GKD Pixel 2's (port/pixel2.c): its window surface,
 *                     here a pbuffer, the portrait panel turned. The window's
 *                     top row is the one the display scans first; GL reads
 *                     the bottom one first.
 *
 * With PRESENT_TOP_DOWN where the port defines it. On each target: a frame
 * at Nearest to the pixel, at every other filter its bright corner where it
 * belongs, under 100% too, a DS frame's two screens, a GPU core's frame both
 * ways up, the Border, a notice, the level bar, On-Screen Stats, the
 * low-battery disc, and a capture of a pixel frame and of a GPU frame. */
#include <SDL.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "diatom_port.h"
#include "hud.h"
#include "osdbar.h"

#if defined(UPRIGHT_BRICK) == defined(UPRIGHT_PIXEL2)
#error "build it as one port: -DUPRIGHT_BRICK or -DUPRIGHT_PIXEL2"
#endif

#ifdef UPRIGHT_BRICK
#define PORT_NAME "brick"
#define PANEL_W 1024
#define PANEL_H 768
#define SURF_W  1024
#define SURF_H  768
#else
#define PORT_NAME "pixel2"
#define PANEL_W 480
#define PANEL_H 640
#define SURF_W  640
#define SURF_H  480
#endif

#include "gles_state.h"

/* What the shared drawing reads, as the ports keep it (port/pixel2.c) */
static float ov_scale(void);
static const uint8_t *g_ov;
static int            g_ov_w, g_ov_h;
static uint64_t       g_ov_until;
static bool           g_ov_dirty;
static uint64_t    g_osd_shown, g_osd_changed;
static bool        g_osd_on;
static int         g_osd_level, g_osd_max;
static int         g_osd_theme;
static ds_theme    g_osd_colors;
static unsigned    g_osd_colors_gen;
static bool        g_lowbat, g_lowbat_blink;
static bool        g_hud_on, g_hud_dirty;
static char        g_hud_text[160];
static ds_theme    g_hud_colors;
static hud_style   g_hud_style;

/* The clock the shared code reads: the test's own, moved by hand, so the
 * level bar is in and still and the notice up whenever the frame is drawn */
static uint64_t g_now = 10000000;
uint64_t diatom_port_now_us(void) { return g_now; }

void diatom_port_log(diatom_log_level lvl, const char *msg)
{
	if (lvl >= DIATOM_LOG_WARN) fprintf(stderr, "upright: %s\n", msg);
}

static void port_logf(diatom_log_level lvl, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
static void port_logf(diatom_log_level lvl, const char *fmt, ...)
{
	char msg[256];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(msg, sizeof msg, fmt, ap);
	va_end(ap);
	diatom_port_log(lvl, msg);
}

static void osd_look_now(osdbar_look *l) { osdbar_look_plain(l, OSDBAR_VOLUME, g_osd_level, g_osd_max); }
static void hold_tick(uint64_t now) { (void)now; }

#include "gles_draw.h"

/* What the panel shows, in the order the display reads it: PANEL_W x
 * PANEL_H, RGBA, its first row the top. Written by show_frame on the
 * presenting thread, read by the test after drain. */
static uint8_t g_panel[PANEL_W * PANEL_H * 4];
static int     g_shown;

#ifdef UPRIGHT_BRICK
#define PAGES 3
static GLuint g_page_tex[PAGES], g_page_fb[PAGES];
static int    g_page = -1;

static bool gl_start(void)
{
	/* brick.c's, but for the surface type: Mesa's surfaceless platform has
	 * no window configs, which eglChooseConfig asks for unless told */
	static const EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE
	};
	static const EGLint context_attribs[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE };
	PFNEGLGETPLATFORMDISPLAYEXTPROC gpd =
		(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
	EGLint n = 0;
	int i;

	g_egl_display = gpd ? gpd(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL)
	                    : EGL_NO_DISPLAY;
	if (g_egl_display == EGL_NO_DISPLAY || !eglInitialize(g_egl_display, NULL, NULL)) return false;
	eglBindAPI(EGL_OPENGL_ES_API);
	if (!eglChooseConfig(g_egl_display, config_attribs, &g_egl_config, 1, &n) || n < 1) return false;
	g_egl_context = eglCreateContext(g_egl_display, g_egl_config, EGL_NO_CONTEXT, context_attribs);
	if (g_egl_context == EGL_NO_CONTEXT ||
	    !eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, g_egl_context) ||
	    !sync_procs())
		return false;
	g_panel_w = g_surface_w = PANEL_W;
	g_panel_h = g_surface_h = PANEL_H;
	g_turned = false;
	/* The pages: brick.c's pages_start, a texture of the page's size where
	 * it binds the page's image */
	glGenTextures(PAGES, g_page_tex);
	glGenFramebuffers(PAGES, g_page_fb);
	for (i = 0; i < PAGES; i++) {
		glBindTexture(GL_TEXTURE_2D, g_page_tex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, PANEL_W, PANEL_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glBindFramebuffer(GL_FRAMEBUFFER, g_page_fb[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_page_tex[i], 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

#include "gles_objects.h"
	return true;
}

/* brick.c's gl_target: the next page's framebuffer object */
static void gl_target(void)
{
	g_page = (g_page + 1) % PAGES;
	glBindFramebuffer(GL_FRAMEBUFFER, g_page_fb[g_page]);
	glViewport(0, 0, g_panel_w, g_panel_h);
}

/* brick.c's gl_drawn: the alpha made opaque, the GPU done */
static void gl_drawn(void)
{
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glFinish();
}

/* The page as the display reads it: its memory from the start */
static void show_frame(void)
{
	glBindFramebuffer(GL_FRAMEBUFFER, g_page_fb[g_page]);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glReadPixels(0, 0, PANEL_W, PANEL_H, GL_RGBA, GL_UNSIGNED_BYTE, g_panel);
	g_shown++;
}

#define PRESENT_TARGET() gl_target()
#define PRESENT_SWAP()   gl_drawn()
#define PORT_FN(n)       gl_##n
#else
static bool gl_start(void)
{
	static const EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE
	};
	static const EGLint context_attribs[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE };
	static const EGLint pbuffer_attribs[] = { EGL_WIDTH, PANEL_W, EGL_HEIGHT, PANEL_H, EGL_NONE };
	PFNEGLGETPLATFORMDISPLAYEXTPROC gpd =
		(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
	EGLint n = 0;

	g_egl_display = gpd ? gpd(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL)
	                    : EGL_NO_DISPLAY;
	if (g_egl_display == EGL_NO_DISPLAY || !eglInitialize(g_egl_display, NULL, NULL)) return false;
	eglBindAPI(EGL_OPENGL_ES_API);
	if (!eglChooseConfig(g_egl_display, config_attribs, &g_egl_config, 1, &n) || n < 1) return false;
	g_egl_context = eglCreateContext(g_egl_display, g_egl_config, EGL_NO_CONTEXT, context_attribs);
	g_egl_surface = eglCreatePbufferSurface(g_egl_display, g_egl_config, pbuffer_attribs);
	if (g_egl_context == EGL_NO_CONTEXT || g_egl_surface == EGL_NO_SURFACE ||
	    !eglMakeCurrent(g_egl_display, g_egl_surface, g_egl_surface, g_egl_context) ||
	    !sync_procs())
		return false;
	/* pixel2.c's panel: portrait, the surface landscape, turned */
	g_panel_w = PANEL_W;
	g_panel_h = PANEL_H;
	g_surface_w = SURF_W;
	g_surface_h = SURF_H;
	g_turned = true;

#include "gles_objects.h"
	return true;
}

/* The window as the display scans it: its top row first, which is GL's
 * last */
static void show_frame(void)
{
	static uint8_t rows[PANEL_W * PANEL_H * 4];
	const size_t row = (size_t)PANEL_W * 4;
	int y;

	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glReadPixels(0, 0, PANEL_W, PANEL_H, GL_RGBA, GL_UNSIGNED_BYTE, rows);
	for (y = 0; y < PANEL_H; y++)
		memcpy(g_panel + (size_t)y * row, rows + (size_t)(PANEL_H - 1 - y) * row, row);
	g_shown++;
}

#define PRESENT_TARGET() do { } while (0)
#define PRESENT_SWAP()   eglSwapBuffers(g_egl_display, g_egl_surface)
#define PORT_FN(n)       diatom_port_##n
#endif

static void gl_stop(void)
{
	if (g_egl_display == EGL_NO_DISPLAY) return;
	eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	if (g_egl_surface != EGL_NO_SURFACE) eglDestroySurface(g_egl_display, g_egl_surface);
	if (g_egl_context != EGL_NO_CONTEXT) eglDestroyContext(g_egl_display, g_egl_context);
	eglTerminate(g_egl_display);
	g_egl_display = EGL_NO_DISPLAY;
}

static void give_display(void) { }

#include "gles_present.h"

/* ==================== the checks ============================================ */

static int g_fail, g_pass;

static void ck(bool cond, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void ck(bool cond, const char *fmt, ...)
{
	va_list ap;

	if (cond) { g_pass++; return; }
	g_fail++;
	printf("FAIL " PORT_NAME ": ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

/* What the player sees at the surface's (x, y), the right way up: on the
 * Brick the panel is the surface; on the Pixel 2 the surface's (x, y) lands
 * on the portrait panel's (y, W - 1 - x), which its panel ("Left Side Up")
 * shows the right way up (port/gles_draw.h's to_clip) */
static const uint8_t *seen(int x, int y)
{
#ifdef UPRIGHT_BRICK
	return g_panel + ((size_t)y * PANEL_W + (size_t)x) * 4;
#else
	return g_panel + ((size_t)(SURF_W - 1 - x) * PANEL_W + (size_t)y) * 4;
#endif
}

static bool black(const uint8_t *p) { return p[0] < 8 && p[1] < 8 && p[2] < 8; }

/* The test picture, w x h, 0x00RRGGBB: its top-left quarter white, the rest
 * black, its first row red and its first column green, so a frame turned or
 * mirrored any way is not itself */
#define PW 64
#define PH 48
static uint32_t pic(int x, int y, int w, int h)
{
	if (y == 0) return 0xff0000;
	if (x == 0) return 0x00ff00;
	return x < w / 2 && y < h / 2 ? 0xffffff : 0x000000;
}

/* The DS frame's bottom screen: blue, its first row yellow */
static uint32_t pic_bottom(int x, int y)
{
	(void)x;
	return y == 0 ? 0xffff00 : 0x0000ff;
}

static uint32_t g_frame[PW * PH * 2];

static void fill(uint32_t (*f)(int, int, int, int), int w, int h)
{
	int x, y;

	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) g_frame[(size_t)y * w + x] = f(x, y, w, h);
}

static uint32_t pic_black(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; return 0; }

/* A pixel frame through the port's present, drawn and on the panel */
static void present(int w, int h, diatom_rect dst, diatom_filter filter)
{
	int before = g_shown;

	PORT_FN(present)(g_frame, w, h, (size_t)w * 4, DIATOM_PIX_XRGB8888, dst, filter);
	drain();
	ck(g_shown == before + 1, "a frame presented was not drawn");
}

static diatom_rect whole(void) { return (diatom_rect){ 0, 0, SURF_W, SURF_H }; }

static bool same_rgb(const uint8_t *p, uint32_t c, int tol)
{
	return abs(p[0] - (int)(c >> 16 & 0xff)) <= tol && abs(p[1] - (int)(c >> 8 & 0xff)) <= tol &&
	       abs(p[2] - (int)(c & 0xff)) <= tol;
}

/* Inside dst, every pixel what Nearest makes of the picture at dst's whole
 * scale; false at the first that is not, said */
static bool nearest_is(const char *what, diatom_rect dst, int w, int h,
                       uint32_t (*f)(int, int, int, int))
{
	int x, y;

	for (y = 0; y < dst.h; y++)
		for (x = 0; x < dst.w; x++) {
			const uint32_t want = f(x * w / dst.w, y * h / dst.h, w, h);
			const uint8_t *p = seen(dst.x + x, dst.y + y);

			if (!same_rgb(p, want, 0)) {
				ck(false, "%s: the player sees (%d, %d) as %02x%02x%02x, the picture's %06x "
				   "(the top row is %s)", what, dst.x + x, dst.y + y, p[0], p[1], p[2], want,
				   same_rgb(seen(dst.x + 1, dst.y + 1), f(0, 0, w, h), 0) ? "at the top"
				   : same_rgb(seen(dst.x + 1, dst.y + dst.h - 2), f(0, 0, w, h), 0)
				     ? "at the bottom" : "nowhere");
				return false;
			}
		}
	ck(true, "%s", what);
	return true;
}

/* The mean light of a box of the panel as the player sees it */
static double light(int x0, int y0, int x1, int y1)
{
	double sum = 0;
	int x, y, n = 0;

	for (y = y0; y < y1; y++)
		for (x = x0; x < x1; x++, n++) {
			const uint8_t *p = seen(x, y);
			sum += 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
		}
	return n ? sum / n : 0;
}

/* A filter's frame over the whole surface: the bright quarter at the top
 * left, the other three far darker, each measured round its middle */
static void bright_corner(const char *what)
{
	const int bw = SURF_W / 8, bh = SURF_H / 8;
	double q[4];
	int i;

	for (i = 0; i < 4; i++) {
		const int cx = (i % 2 ? 3 : 1) * SURF_W / 4, cy = (i / 2 ? 3 : 1) * SURF_H / 4;
		q[i] = light(cx - bw / 2, cy - bh / 2, cx + bw / 2, cy + bh / 2);
	}
	ck(q[0] > 100 && q[1] < q[0] / 2 && q[2] < q[0] / 2 && q[3] < q[0] / 2,
	   "%s: the bright quarter is not top left (light top left %.0f, top right %.0f, "
	   "bottom left %.0f, bottom right %.0f)", what, q[0], q[1], q[2], q[3]);
}

/* Nothing but black outside a rect of the surface, and something inside */
static void only_in(const char *what, int rx, int ry, int rw, int rh)
{
	int x, y, inside = 0;

	for (y = 0; y < SURF_H; y++)
		for (x = 0; x < SURF_W; x++) {
			const bool in = x >= rx && x < rx + rw && y >= ry && y < ry + rh;

			if (black(seen(x, y))) continue;
			if (!in) {
				const uint8_t *p = seen(x, y);
				ck(false, "%s: (%d, %d) is %02x%02x%02x, outside its place %dx%d at %d,%d",
				   what, x, y, p[0], p[1], p[2], rw, rh, rx, ry);
				return;
			}
			inside++;
		}
	ck(inside > rw * rh / 20, "%s: nothing drawn in its place %dx%d at %d,%d (%d pixels)",
	   what, rw, rh, rx, ry, inside);
}

/* A capture of the front frame (diatom_port_capture's, brick.c's way): a
 * BMP of the surface, its first row the top */
static bool capture(const char *what, const char *path, int w, int h,
                    uint32_t (*f)(int, int, int, int))
{
	SDL_Surface *s, *rgb;
	bool ok;
	int x, y;

	pthread_mutex_lock(&g_mx);
	g_capture_path = path;
	pthread_cond_signal(&g_cv);
	while (g_capture_path) pthread_cond_wait(&g_idle, &g_mx);
	ok = g_capture_ok;
	pthread_mutex_unlock(&g_mx);
	ck(ok, "%s: the capture failed", what);
	if (!ok) return false;
	s = SDL_LoadBMP(path);
	rgb = s ? SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGB24, 0) : NULL;
	if (s) SDL_FreeSurface(s);
	ck(rgb && rgb->w == SURF_W && rgb->h == SURF_H, "%s: the capture is not a %dx%d BMP", what, SURF_W, SURF_H);
	if (!rgb || rgb->w != SURF_W || rgb->h != SURF_H) { if (rgb) SDL_FreeSurface(rgb); return false; }
	for (y = 0; y < SURF_H; y++)
		for (x = 0; x < SURF_W; x++) {
			const uint8_t *p = (const uint8_t *)rgb->pixels + (size_t)y * rgb->pitch + (size_t)x * 3;
			const uint32_t want = f(x * w / SURF_W, y * h / SURF_H, w, h);

			if (!same_rgb(p, want, 0)) {
				ck(false, "%s: the capture's (%d, %d) is %02x%02x%02x, the picture's %06x",
				   what, x, y, p[0], p[1], p[2], want);
				SDL_FreeSurface(rgb);
				return false;
			}
		}
	SDL_FreeSurface(rgb);
	ck(true, "%s", what);
	return true;
}

/* A GPU core's frame: drawn on its own context into the port's framebuffer
 * (diatom_port_gpu_target) as a core draws, GL's way up (its first row at
 * the top of what it draws, y = h - 1) unless top_down, then presented */
static void gpu_frame(bool top_down, diatom_rect dst, diatom_filter filter)
{
	const int w = PW, h = PH;
	int before = g_shown;
	/* The picture's row r at the core's y */
#define CORE_Y(r, n) (top_down ? (r) : h - (r) - (n))

	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)diatom_port_gpu_target());
	glViewport(0, 0, w, h);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(1, 1, 1, 1);
	glScissor(0, CORE_Y(0, h / 2), w / 2, h / 2);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0, 1, 0, 1);
	glScissor(0, 0, 1, h);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(1, 0, 0, 1);
	glScissor(0, CORE_Y(0, 1), w, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);
#undef CORE_Y
	diatom_port_present_gpu(w, h, dst, filter);
	drain();
	ck(g_shown == before + 1, "a GPU frame presented was not drawn");
}

static void scene_state(bool lowbat, bool osd, bool hud)
{
	pthread_mutex_lock(&g_mx);
	g_lowbat = lowbat;
	g_lowbat_blink = false;
	g_osd_on = osd;
	if (osd) {
		/* In, still and held: shown longer ago than its slide, changed
		 * within its hold */
		g_osd_shown = g_now - (uint64_t)(OSDBAR_IN_MS + 100) * 1000;
		g_osd_changed = g_now - (uint64_t)(OSDBAR_IN_MS + 100) * 1000;
		g_osd_level = 7;
		g_osd_max = 10;
		g_osd_theme = DS_DARK;
		g_osd_colors = DS_THEME[DS_DARK];
		g_osd_colors_gen++;
	}
	g_hud_on = hud;
	if (hud) {
		snprintf(g_hud_text, sizeof g_hud_text, "60.0 FPS");
		g_hud_style = hud_default;
		g_hud_colors = DS_THEME[DS_DARK];
		g_hud_dirty = true;
	}
	pthread_mutex_unlock(&g_mx);
}

static uint32_t pic_ds(int x, int y, int w, int h)
{
	(void)h;
	return y < PH ? pic(x, y, w, PH) : pic_bottom(x, y - PH);
}

/* The Border's picture: a colour each quarter */
static uint32_t border_px(int x, int y)
{
	static const uint32_t q[4] = { 0x00c8c8, 0xc8c800, 0xc800c8, 0x646464 };
	return q[(x >= SURF_W / 2) + 2 * (y >= SURF_H / 2)];
}

int main(int argc, char **argv)
{
	static const struct { diatom_filter f; const char *name; int strength; } filters[] = {
		{ DIATOM_FILTER_SHARP, "Sharp", 100 },
		{ DIATOM_FILTER_SHIMMERLESS, "Sharp-Shimmerless", 100 },
		{ DIATOM_FILTER_CRT, "CRT", 100 },
		{ DIATOM_FILTER_CRT, "CRT at 50%", 50 },
		{ DIATOM_FILTER_CRT_CURVED, "CRT Curved", 100 },
		{ DIATOM_FILTER_SCANLINES, "Scanlines", 100 },
		{ DIATOM_FILTER_LCD, "LCD Grid", 100 },
		{ DIATOM_FILTER_LCD, "LCD Grid at 30%", 30 },
		{ DIATOM_FILTER_LCD_UNLIT, "LCD No Backlight", 100 },
	};
	const char *dir = argc > 1 ? argv[1] : "/tmp";
	char path[512];
	pthread_condattr_t ca;
	diatom_gpu_spec spec = { 3, PW + 36, PH + 32, false, false, false };
	size_t i;

	/* The thread, as brick.c's gl_ensure starts it */
	pthread_condattr_init(&ca);
	pthread_condattr_setclock(&ca, CLOCK_MONOTONIC);
	pthread_cond_destroy(&g_idle);
	pthread_cond_init(&g_idle, &ca);
	pthread_condattr_destroy(&ca);
	if (pthread_create(&g_thread, NULL, present_thread, NULL) != 0) return 2;
	pthread_mutex_lock(&g_mx);
	while (g_thread_ready == 0) pthread_cond_wait(&g_idle, &g_mx);
	pthread_mutex_unlock(&g_mx);
	if (g_thread_ready < 0) {
		pthread_join(g_thread, NULL);
		printf("skipped: no OpenGL ES 3 on EGL's surfaceless platform here\n");
		return 77;
	}
	g_thread_running = true;

	/* A pixel frame at Nearest, over the whole surface, to the pixel */
	fill(pic, PW, PH);
	present(PW, PH, whole(), DIATOM_FILTER_NEAREST);
	nearest_is("a frame at Nearest", whole(), PW, PH, pic);
	snprintf(path, sizeof path, "%s/upright-" PORT_NAME "-capture.bmp", dir);
	capture("a capture of a frame", path, PW, PH, pic);

	/* Each other filter: the bright quarter where it belongs */
	for (i = 0; i < sizeof filters / sizeof filters[0]; i++) {
		char what[64];

		PORT_FN(strength)(filters[i].strength);
		present(PW, PH, whole(), filters[i].f);
		snprintf(what, sizeof what, "a frame at %s", filters[i].name);
		bright_corner(what);
	}
	PORT_FN(strength)(100);

	/* A DS frame's two screens, the top one at the top */
	{
		diatom_plane planes[2] = {
			{ { 0, 0, PW, PH },  { 0, 0, SURF_W, SURF_H / 2 } },
			{ { 0, PH, PW, PH }, { 0, SURF_H / 2, SURF_W, SURF_H / 2 } },
		};

		fill(pic_ds, PW, PH * 2);
		PORT_FN(planes)(planes, 2);
		present(PW, PH * 2, whole(), DIATOM_FILTER_NEAREST);
		nearest_is("a DS frame's two screens", whole(), PW, PH * 2, pic_ds);
		PORT_FN(planes)(NULL, 0);
	}

	/* The Border round a smaller picture, each of its quarters where it
	 * belongs, the picture itself to the pixel */
	{
		static uint8_t bd[SURF_W * SURF_H * 4];
		const diatom_rect mid = { SURF_W / 4, SURF_H / 4, SURF_W / 2, SURF_H / 2 };
		const int at[4][2] = { { 8, 8 }, { SURF_W - 9, 8 }, { 8, SURF_H - 9 }, { SURF_W - 9, SURF_H - 9 } };
		int x, y, k;

		for (y = 0; y < SURF_H; y++)
			for (x = 0; x < SURF_W; x++) {
				uint8_t *p = bd + ((size_t)y * SURF_W + (size_t)x) * 4;
				const uint32_t c = border_px(x, y);

				p[0] = c & 0xff; p[1] = c >> 8 & 0xff; p[2] = c >> 16 & 0xff; p[3] = 255;
			}
		PORT_FN(border)(bd, SURF_W, SURF_H);
		fill(pic, PW, PH);
		present(PW, PH, mid, DIATOM_FILTER_NEAREST);
		for (k = 0; k < 4; k++) {
			const uint8_t *p = seen(at[k][0], at[k][1]);
			ck(same_rgb(p, border_px(at[k][0], at[k][1]), 2),
			   "the Border: (%d, %d) is %02x%02x%02x, its picture's %06x", at[k][0], at[k][1],
			   p[0], p[1], p[2], border_px(at[k][0], at[k][1]));
		}
		nearest_is("a frame inside the Border", mid, PW, PH, pic);
		PORT_FN(border)(NULL, 0, 0);
	}

	/* A notice, at the bottom, its top half yellow and its bottom half
	 * magenta, over a black frame */
	{
		enum { OW = 400, OH = 60 };
		static uint8_t ov[OW * OH * 4];
		const float s = ov_scale();
		const float ow = roundf(OW * s), oh = roundf(OH * s);
		const int x0 = (int)roundf((SURF_W - ow) / 2.0f);
		const int y0 = (int)roundf(SURF_H - oh - SURF_H / 24.0f);
		int x, y;

		for (y = 0; y < OH; y++)
			for (x = 0; x < OW; x++) {
				uint8_t *p = ov + ((size_t)y * OW + (size_t)x) * 4;
				p[0] = y < OH / 2 ? 0 : 255;  /* B */
				p[1] = y < OH / 2 ? 255 : 0;  /* G */
				p[2] = 255;                   /* R */
				p[3] = 255;
			}
		PORT_FN(overlay)(ov, OW, OH, 5000);
		fill(pic_black, PW, PH);
		present(PW, PH, whole(), DIATOM_FILTER_NEAREST);
		only_in("a notice", x0, y0, (int)ow, (int)oh);
		ck(same_rgb(seen(x0 + (int)ow / 2, y0 + 3), 0xffff00, 2),
		   "a notice: its top half is not at its top");
		ck(same_rgb(seen(x0 + (int)ow / 2, y0 + (int)oh - 4), 0xff00ff, 2),
		   "a notice: its bottom half is not at its bottom");
		PORT_FN(overlay)(NULL, 0, 0, 0);
	}

	/* The level bar, On-Screen Stats and the low-battery disc, each alone
	 * over a black frame, nowhere but in its place */
	{
		const float s = (float)SURF_W / 1024.0f;
		int w, h, x, y;

		fill(pic_black, PW, PH);
		scene_state(false, true, false);
		present(PW, PH, whole(), DIATOM_FILTER_NEAREST);
		osdbar_size(s, &w, &h);
		only_in("the level bar", (int)roundf(OSDBAR_X * s), (int)roundf(OSDBAR_Y * s), w, h);

		scene_state(false, false, true);
		present(PW, PH, whole(), DIATOM_FILTER_NEAREST);
		hud_size("60.0 FPS", s, &hud_default, &w, &h);
		hud_place(&hud_default, s, w, h, SURF_W, SURF_H, &x, &y);
		only_in("On-Screen Stats", x, y, w, h);

		scene_state(true, false, false);
		present(PW, PH, whole(), DIATOM_FILTER_NEAREST);
		only_in("the low-battery disc", SURF_W - 34 - 9, 34 - 9, 19, 19);
		ck(same_rgb(seen(SURF_W - 34, 34), 0xe04848, 1), "the low-battery disc: not at %d,34",
		   SURF_W - 34);
		scene_state(false, false, false);
	}

	/* A GPU core's frames, GL's way up and top down, at Nearest to the
	 * pixel and at a filter, a DS core's two screens, and a capture */
	if (!diatom_port_gpu_begin(&spec)) {
		ck(false, "no GPU frames (diatom_port_gpu_begin)");
	} else {
		gpu_frame(false, whole(), DIATOM_FILTER_NEAREST);
		fill(pic, PW, PH);
		nearest_is("a GPU core's frame", whole(), PW, PH, pic);
		snprintf(path, sizeof path, "%s/upright-" PORT_NAME "-capture-gpu.bmp", dir);
		capture("a capture of a GPU core's frame", path, PW, PH, pic);
		gpu_frame(false, whole(), DIATOM_FILTER_CRT);
		bright_corner("a GPU core's frame at CRT");
		diatom_port_gpu_end();

		spec.top_down = true;
		if (diatom_port_gpu_begin(&spec)) {
			gpu_frame(true, whole(), DIATOM_FILTER_NEAREST);
			nearest_is("a GPU core's frame drawn top down", whole(), PW, PH, pic);
			diatom_port_gpu_end();
		} else {
			ck(false, "no GPU frames top down (diatom_port_gpu_begin)");
		}
	}

	pthread_mutex_lock(&g_mx);
	g_thread_stop = true;
	pthread_cond_signal(&g_cv);
	pthread_mutex_unlock(&g_mx);
	pthread_join(g_thread, NULL);

	if (g_fail) return 1;
	printf("ok " PORT_NAME ": %d pictures the right way up on its target (%s), %s\n", g_pass,
#ifdef UPRIGHT_BRICK
	       "a page of fb0, read from its start",
#else
	       "the window surface, the portrait panel turned",
#endif
#ifdef PRESENT_TOP_DOWN
	       "PRESENT_TOP_DOWN"
#else
	       "GL's way up"
#endif
	       );
	return 0;
}
