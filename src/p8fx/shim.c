/* SPDX-License-Identifier: GPL-3.0-only */
/* Your PICO-8's Shader and Border, drawn in PICO-8's own process
 * (src/p8fx.h, DESIGN.md 11.55).
 *
 * pico8_64 (0.2.7 for Raspberry Pi) draws with the system's SDL2 and its
 * OpenGL ES 2 renderer, every call through SDL's own functions: at start
 * SDL_CreateRenderer, then SDL_CreateTexture for its screen (ARGB8888,
 * streaming, 128 pixels each way times its preblit scale, 1 at Sharp
 * Pixels and up to 3 otherwise); each frame SDL_UpdateTexture with the
 * whole picture, SDL_RenderClear (black), SDL_RenderCopy of the texture to
 * where its display mode puts it, its own FPS line when on, and
 * SDL_RenderPresent. sd/pixel2/PICO-8.sh loads this library first
 * (LD_PRELOAD), so those calls come here, and each goes on to SDL's:
 *
 *   SDL_CreateRenderer   kept when it is SDL's OpenGL ES 2 renderer
 *   SDL_CreateTexture    the screen's texture kept by its shape
 *   SDL_UpdateTexture    the picture kept as well, at PICO-8's own 128x128
 *   SDL_RenderCopy       the screen's copy: what SDL has queued (the clear)
 *                        drawn first, then the Border over the black with
 *                        the GL of SDL's own context, then the picture
 *                        through the shader where there is one; with Off,
 *                        SDL draws the picture as before
 *
 * The shaders are Diatom's (diatom/port/gles_shaders.h, which its Pixel 2
 * port draws with), so PICO-8 looks as a game in Diatom (fake08) does at
 * the same size; Blur and the picture are Diatom's too (diatom/src/border.c).
 *
 * Fail-safe: anything missing (SDL's functions, a renderer that is not
 * OpenGL ES's, a context below OpenGL ES 3, a program that does not link,
 * no look file, a picture that cannot be read, a texture of another shape)
 * and the call goes on to SDL as it came, so PICO-8 draws exactly as it
 * does without this library. SDL's renderer keeps the GL state it set in a
 * cache of its own, so what this draws with goes back as it was found: the
 * program, the texture and the unit, the buffers, the viewport, the blend
 * and the scissor, the unpacking; the quads have a vertex array of their
 * own, so SDL's attributes are never touched. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <SDL.h>
#include <GLES3/gl3.h>

#include "../p8fx.h"
#include "diatom.h"
#include "../../diatom/port/gles_shaders.h"
#include "../../diatom/src/border.h"

/* ---------- SDL's own, past this library ---------------------------------- */

static struct {
	SDL_Renderer *(*CreateRenderer)(SDL_Window *, int, Uint32);
	void (*DestroyRenderer)(SDL_Renderer *);
	SDL_Texture *(*CreateTexture)(SDL_Renderer *, Uint32, int, int, int);
	void (*DestroyTexture)(SDL_Texture *);
	int (*UpdateTexture)(SDL_Texture *, const SDL_Rect *, const void *, int);
	int (*RenderCopy)(SDL_Renderer *, SDL_Texture *, const SDL_Rect *, const SDL_Rect *);
	int (*RenderFlush)(SDL_Renderer *);
	int (*GetRendererInfo)(SDL_Renderer *, SDL_RendererInfo *);
	int (*GetRendererOutputSize)(SDL_Renderer *, int *, int *);
	SDL_Texture *(*GetRenderTarget)(SDL_Renderer *);
	void (*RenderGetViewport)(SDL_Renderer *, SDL_Rect *);
	void (*RenderGetScale)(SDL_Renderer *, float *, float *);
	void *(*GL_GetProcAddress)(const char *);
	SDL_GLContext (*GL_GetCurrentContext)(void);
} sdl;

/* ---------- the GL of SDL's context, asked of SDL as its renderer does ---- */

#define GL_FUNCS(X) \
	X(const GLubyte *, GetString, (GLenum)) \
	X(void, GetIntegerv, (GLenum, GLint *)) \
	X(GLboolean, IsEnabled, (GLenum)) \
	X(void, Enable, (GLenum)) \
	X(void, Disable, (GLenum)) \
	X(void, Viewport, (GLint, GLint, GLsizei, GLsizei)) \
	X(void, UseProgram, (GLuint)) \
	X(void, ActiveTexture, (GLenum)) \
	X(void, BindTexture, (GLenum, GLuint)) \
	X(void, GenTextures, (GLsizei, GLuint *)) \
	X(void, TexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
	X(void, TexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *)) \
	X(void, TexParameteri, (GLenum, GLenum, GLint)) \
	X(void, PixelStorei, (GLenum, GLint)) \
	X(GLuint, CreateShader, (GLenum)) \
	X(void, ShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
	X(void, CompileShader, (GLuint)) \
	X(void, GetShaderiv, (GLuint, GLenum, GLint *)) \
	X(void, GetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
	X(void, DeleteShader, (GLuint)) \
	X(GLuint, CreateProgram, (void)) \
	X(void, AttachShader, (GLuint, GLuint)) \
	X(void, BindAttribLocation, (GLuint, GLuint, const GLchar *)) \
	X(void, LinkProgram, (GLuint)) \
	X(void, GetProgramiv, (GLuint, GLenum, GLint *)) \
	X(void, DeleteProgram, (GLuint)) \
	X(GLint, GetUniformLocation, (GLuint, const GLchar *)) \
	X(void, Uniform1i, (GLint, GLint)) \
	X(void, Uniform1f, (GLint, GLfloat)) \
	X(void, Uniform2f, (GLint, GLfloat, GLfloat)) \
	X(void, GenBuffers, (GLsizei, GLuint *)) \
	X(void, BindBuffer, (GLenum, GLuint)) \
	X(void, BufferData, (GLenum, GLsizeiptr, const void *, GLenum)) \
	X(void, GenVertexArrays, (GLsizei, GLuint *)) \
	X(void, BindVertexArray, (GLuint)) \
	X(void, VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *)) \
	X(void, EnableVertexAttribArray, (GLuint)) \
	X(void, DrawArrays, (GLenum, GLint, GLsizei))

#define GL_MEMBER(ret, name, args) ret (*name) args;
static struct { GL_FUNCS(GL_MEMBER) } gl;

/* ---------- what is kept ---------------------------------------------------- */

#define SCREEN P8FX_SCREEN
/* The look file looked at no more often than this */
#define LOOK_EVERY_MS 250

static struct {
	bool resolved, dead;          /* SDL's functions found; nothing to do, ever */
	SDL_Renderer *r;              /* PICO-8's renderer, OpenGL ES 2's */
	SDL_Texture *screen;          /* its screen's texture */
	int screen_w;
	uint8_t frame[SCREEN * SCREEN * 4];   /* the last picture, BGRA */
	bool have_frame, frame_new;

	/* The look, from P8FX_FILE */
	const char *path;
	p8fx_look look;
	int filter;                   /* diatom_filter of look.shader, NEAREST for Off */
	struct timespec looked;
	bool looked_once;
	ino_t ino;
	struct timespec mtime;
	off_t size;
	uint8_t *picture;             /* the Border's picture, BGRA, until uploaded */
	int pic_w, pic_h;
	bool picture_new;
	unsigned blur_frames;

	/* GL, on SDL's context: made at the first copy that draws */
	bool gl_tried, gl_ok;
	GLuint prog_plain, prog[DIATOM_FILTER_COUNT], soft[DIATOM_FILTER_COUNT];
	bool failed[DIATOM_FILTER_COUNT], soft_failed[DIATOM_FILTER_COUNT];
	GLuint frame_tex, border_tex, vao, vbo;
	int border_w, border_h;       /* what the border texture holds, 0 none */
	bool border_blur;             /* ...and whether that is a blur */
} g;

static void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void say(const char *fmt, ...)
{
	va_list ap;

	fputs("p8fx: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

/* SDL's functions, past this library in the search order. Any missing and
 * this library does nothing: every call goes on to SDL's as it came. */
static void resolve(void)
{
#define NEXT(field, name) (*(void **)&sdl.field = dlsym(RTLD_NEXT, name))
	if (g.resolved) return;
	g.resolved = true;
	NEXT(CreateRenderer, "SDL_CreateRenderer");
	NEXT(DestroyRenderer, "SDL_DestroyRenderer");
	NEXT(CreateTexture, "SDL_CreateTexture");
	NEXT(DestroyTexture, "SDL_DestroyTexture");
	NEXT(UpdateTexture, "SDL_UpdateTexture");
	NEXT(RenderCopy, "SDL_RenderCopy");
	NEXT(RenderFlush, "SDL_RenderFlush");
	NEXT(GetRendererInfo, "SDL_GetRendererInfo");
	NEXT(GetRendererOutputSize, "SDL_GetRendererOutputSize");
	NEXT(GetRenderTarget, "SDL_GetRenderTarget");
	NEXT(RenderGetViewport, "SDL_RenderGetViewport");
	NEXT(RenderGetScale, "SDL_RenderGetScale");
	NEXT(GL_GetProcAddress, "SDL_GL_GetProcAddress");
	NEXT(GL_GetCurrentContext, "SDL_GL_GetCurrentContext");
#undef NEXT
	if (!sdl.CreateRenderer || !sdl.DestroyRenderer || !sdl.CreateTexture || !sdl.DestroyTexture ||
	    !sdl.UpdateTexture || !sdl.RenderCopy || !sdl.RenderFlush || !sdl.GetRendererInfo ||
	    !sdl.GetRendererOutputSize || !sdl.GetRenderTarget || !sdl.RenderGetViewport ||
	    !sdl.RenderGetScale || !sdl.GL_GetProcAddress || !sdl.GL_GetCurrentContext) {
		g.dead = true;
		say("this SDL lacks a function it needs (SDL 2.0.10 or later); PICO-8 draws as it does");
		return;
	}
	g.path = getenv("ZYPEROS_P8FX");
	if (!g.path || !*g.path) g.path = P8FX_FILE;
	p8fx_look_default(&g.look);
	g.filter = DIATOM_FILTER_NEAREST;
}

/* ---------- the look ---------------------------------------------------------- */

static long ms_since(const struct timespec *a, const struct timespec *b)
{
	return (long)(b->tv_sec - a->tv_sec) * 1000 + (b->tv_nsec - a->tv_nsec) / 1000000;
}

/* The look file again when it moved, a few times a second at most */
static void look_poll(void)
{
	struct timespec now;
	struct stat st;
	bool there;
	p8fx_look l;
	int f;

	clock_gettime(CLOCK_MONOTONIC, &now);
	if (g.looked_once && ms_since(&g.looked, &now) < LOOK_EVERY_MS) return;
	g.looked = now;
	there = stat(g.path, &st) == 0;
	if (g.looked_once && there == (g.ino != 0) &&
	    (!there || (st.st_ino == g.ino && st.st_size == g.size &&
	                st.st_mtim.tv_sec == g.mtime.tv_sec && st.st_mtim.tv_nsec == g.mtime.tv_nsec)))
		return;
	g.looked_once = true;
	g.ino = there ? st.st_ino : 0;
	if (there) {
		g.size = st.st_size;
		g.mtime = st.st_mtim;
	}
	if (!there || !p8fx_look_read(g.path, &l)) p8fx_look_default(&l);
	/* A shader Diatom has not, or not for PICO-8 (LCD No Backlight): Off */
	f = diatom_filter_index(l.shader);
	if (f < 0 || f == DIATOM_FILTER_LCD_UNLIT) {
		if (strcmp(l.shader, "nearest")) say("no shader \"%s\"; Off", l.shader);
		f = DIATOM_FILTER_NEAREST;
	}
	free(g.picture);
	g.picture = NULL;
	g.picture_new = false;
	if (l.border == P8FX_BORDER_PICTURE) {
		g.picture = border_read(l.border_file, &g.pic_w, &g.pic_h);
		if (g.picture) {
			g.picture_new = true;
		} else {
			say("cannot read the border's picture %s; no border", l.border_file[0] ? l.border_file : "(none)");
			l.border = P8FX_BORDER_OFF;
		}
	}
	/* What the border texture holds goes with the look it was for */
	g.border_w = g.border_h = 0;
	g.blur_frames = 0;
	g.look = l;
	g.filter = f;
	say("shader %s at %d%%, border %s", diatom_filters[f].name, l.strength, p8fx_border_word(l.border));
}

/* ---------- GL ----------------------------------------------------------------- */

static GLuint compile(GLenum type, const char *src)
{
	GLuint s = gl.CreateShader(type);
	GLint ok = 0;

	gl.ShaderSource(s, 1, &src, NULL);
	gl.CompileShader(s);
	gl.GetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512] = "";

		gl.GetShaderInfoLog(s, sizeof log, NULL, log);
		say("shader: %s", log);
		gl.DeleteShader(s);
		return 0;
	}
	return s;
}

/* As Diatom links its programs (port/gles_draw.h's link_program): a_pos at 0
 * and a_src at 1 */
static GLuint link_program(const char *fragment)
{
	GLuint vs = compile(GL_VERTEX_SHADER, VERTEX_SHADER);
	GLuint fs = vs ? compile(GL_FRAGMENT_SHADER, fragment) : 0;
	GLuint p;
	GLint ok = 0;

	if (!vs || !fs) {
		if (vs) gl.DeleteShader(vs);
		return 0;
	}
	p = gl.CreateProgram();
	gl.AttachShader(p, vs);
	gl.AttachShader(p, fs);
	gl.BindAttribLocation(p, 0, "a_pos");
	gl.BindAttribLocation(p, 1, "a_src");
	gl.LinkProgram(p);
	gl.DeleteShader(vs);
	gl.DeleteShader(fs);
	gl.GetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok) {
		gl.DeleteProgram(p);
		return 0;
	}
	return p;
}

/* What SDL's renderer set and keeps in its cache, put back after */
typedef struct {
	GLint program, active, tex0, array_buf, vao, unpack_buf, viewport[4], align, row_length;
	GLboolean blend, scissor;
} gl_saved;

static void save(gl_saved *s)
{
	gl.GetIntegerv(GL_CURRENT_PROGRAM, &s->program);
	gl.GetIntegerv(GL_ACTIVE_TEXTURE, &s->active);
	gl.ActiveTexture(GL_TEXTURE0);
	gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &s->tex0);
	gl.GetIntegerv(GL_ARRAY_BUFFER_BINDING, &s->array_buf);
	gl.GetIntegerv(GL_VERTEX_ARRAY_BINDING, &s->vao);
	gl.GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &s->unpack_buf);
	gl.GetIntegerv(GL_VIEWPORT, s->viewport);
	gl.GetIntegerv(GL_UNPACK_ALIGNMENT, &s->align);
	gl.GetIntegerv(GL_UNPACK_ROW_LENGTH, &s->row_length);
	s->blend = gl.IsEnabled(GL_BLEND);
	s->scissor = gl.IsEnabled(GL_SCISSOR_TEST);
}

static void restore(const gl_saved *s)
{
	if (s->scissor) gl.Enable(GL_SCISSOR_TEST);
	if (s->blend) gl.Enable(GL_BLEND);
	gl.PixelStorei(GL_UNPACK_ROW_LENGTH, s->row_length);
	gl.PixelStorei(GL_UNPACK_ALIGNMENT, s->align);
	gl.Viewport(s->viewport[0], s->viewport[1], s->viewport[2], s->viewport[3]);
	gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)s->unpack_buf);
	gl.BindVertexArray((GLuint)s->vao);
	gl.BindBuffer(GL_ARRAY_BUFFER, (GLuint)s->array_buf);
	gl.BindTexture(GL_TEXTURE_2D, (GLuint)s->tex0);
	gl.ActiveTexture((GLenum)s->active);
	gl.UseProgram((GLuint)s->program);
}

/* A texture of ours: BGRA bytes swizzled back to RGB, opaque, clamped */
static GLuint bgra_texture(GLint filter)
{
	GLuint t;

	gl.GenTextures(1, &t);
	gl.BindTexture(GL_TEXTURE_2D, t);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
	return t;
}

/* The functions, OpenGL ES 3 (Diatom's shaders are GLSL ES 3.00: SDL asks
 * for a 2 context, which Mesa's drivers make their highest), the plain
 * program, the textures and the quad's vertex array; once a renderer. False,
 * and never asked again, where any is missing. */
static bool gl_start(void)
{
	const char *v;
	int major = 0, minor = 0;
	gl_saved s;

	if (g.gl_tried) return g.gl_ok;
	g.gl_tried = true;
#define GL_LOAD(ret, name, args) *(void **)&gl.name = sdl.GL_GetProcAddress("gl" #name);
	GL_FUNCS(GL_LOAD)
#undef GL_LOAD
#define GL_HAVE(ret, name, args) if (!gl.name) { say("no gl" #name "; PICO-8 draws as it does"); return false; }
	GL_FUNCS(GL_HAVE)
#undef GL_HAVE
	v = (const char *)gl.GetString(GL_VERSION);
	if (!v || sscanf(v, "OpenGL ES %d.%d", &major, &minor) != 2 || major < 3) {
		say("%s, not OpenGL ES 3; PICO-8 draws as it does", v ? v : "no GL version");
		return false;
	}
	save(&s);
	g.prog_plain = link_program(PLAIN_SHADER);
	if (g.prog_plain) {
		g.frame_tex = bgra_texture(GL_NEAREST);
		gl.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
		gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SCREEN, SCREEN, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		/* Smooth both ways: the blur is 32x24 stretched to the screen */
		g.border_tex = bgra_texture(GL_LINEAR);
		gl.GenVertexArrays(1, &g.vao);
		gl.GenBuffers(1, &g.vbo);
		gl.BindVertexArray(g.vao);
		gl.BindBuffer(GL_ARRAY_BUFFER, g.vbo);
		gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void *)0);
		gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void *)(2 * sizeof(float)));
		gl.EnableVertexAttribArray(0);
		gl.EnableVertexAttribArray(1);
	}
	restore(&s);
	if (!g.prog_plain) {
		say("the plain program did not link; PICO-8 draws as it does");
		return false;
	}
	say("%s: shaders ready", v);
	g.gl_ok = true;
	return true;
}

/* Filter f's program at the look's strength, linked the first time, as
 * Diatom's filter_prog: under 100% the soft one where the filter has one,
 * and that failing, the full one. 0 for Off and for one that failed. */
static GLuint filter_prog(int f, bool *soft)
{
	*soft = false;
	if (f <= DIATOM_FILTER_NEAREST || f >= DIATOM_FILTER_COUNT) return 0;
	if (g.look.strength < 100 && SOFT_SHADER[f]) {
		if (!g.soft[f] && !g.soft_failed[f]) {
			g.soft[f] = link_program(*SOFT_SHADER[f]);
			g.soft_failed[f] = !g.soft[f];
			if (g.soft_failed[f]) say("%s's strength did not link; full", diatom_filters[f].name);
		}
		if (g.soft[f]) {
			*soft = true;
			return g.soft[f];
		}
	}
	if (!g.prog[f] && !g.failed[f]) {
		g.prog[f] = link_program(*FILTER_SHADER[f]);
		g.failed[f] = !g.prog[f];
		if (g.failed[f]) say("%s did not link; PICO-8 draws its picture as it does", diatom_filters[f].name);
	}
	return g.prog[f];
}

/* One quad: the rect in the output's pixels (y down), its source in
 * texels, corners top-left, top-right, bottom-left, bottom-right (Diatom's
 * draw_quad_from, unturned: SDL turns the window onto the panel) */
static void quad(float x, float y, float w, float h, float sx, float sy, float sw, float sh, int ow, int oh)
{
	const float x0 = x / ow * 2 - 1, x1 = (x + w) / ow * 2 - 1;
	const float y0 = 1 - y / oh * 2, y1 = 1 - (y + h) / oh * 2;
	const float v[16] = {
		x0, y0, sx,      sy,
		x1, y0, sx + sw, sy,
		x0, y1, sx,      sy + sh,
		x1, y1, sx + sw, sy + sh,
	};

	gl.BindBuffer(GL_ARRAY_BUFFER, g.vbo);
	gl.BufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_STREAM_DRAW);
	gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/* The Border's texture as the look says: the picture uploaded once; the
 * blur made again every BORDER_BLUR_EVERY frames drawn, as Diatom's
 * (border_tick). False for none. */
static bool border_ready(int ow, int oh)
{
	if (g.look.border == P8FX_BORDER_PICTURE && g.picture && g.picture_new) {
		gl.BindTexture(GL_TEXTURE_2D, g.border_tex);
		gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g.pic_w, g.pic_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, g.picture);
		g.border_w = g.pic_w;
		g.border_h = g.pic_h;
		g.border_blur = false;
		g.picture_new = false;
	} else if (g.look.border == P8FX_BORDER_BLUR && g.have_frame &&
	           (!g.border_w || !g.border_blur || g.blur_frames % BORDER_BLUR_EVERY == 0)) {
		static uint8_t px[BORDER_BLUR_W * BORDER_BLUR_H * 4];

		if (border_blur(g.frame, SCREEN, SCREEN, SCREEN * 4, false, ow, oh, px)) {
			gl.BindTexture(GL_TEXTURE_2D, g.border_tex);
			gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, BORDER_BLUR_W, BORDER_BLUR_H, 0, GL_RGBA,
			              GL_UNSIGNED_BYTE, px);
			g.border_w = BORDER_BLUR_W;
			g.border_h = BORDER_BLUR_H;
			g.border_blur = true;
		}
	}
	if (g.look.border == P8FX_BORDER_BLUR) g.blur_frames++;
	return g.look.border != P8FX_BORDER_OFF && g.border_w > 0;
}

/* The screen's copy, drawn here: true when the picture was (a shader), false
 * for SDL to draw it as it does (Off, or anything that cannot be drawn) -
 * with the Border under it already, where there is one */
static bool fx_copy(SDL_Renderer *r, const SDL_Rect *dst)
{
	SDL_Rect vp;
	float scx = 0, scy = 0;
	int ow = 0, oh = 0;
	bool soft = false, border, shader;
	GLuint prog;
	gl_saved s;

	/* The renderer as PICO-8 leaves it: onto the window, no viewport or
	 * scale of its own, so its rect is the output's pixels */
	if (sdl.GetRenderTarget(r)) return false;
	if (sdl.GetRendererOutputSize(r, &ow, &oh) || ow <= 0 || oh <= 0) return false;
	sdl.RenderGetViewport(r, &vp);
	sdl.RenderGetScale(r, &scx, &scy);
	if (vp.x || vp.y || vp.w != ow || vp.h != oh || scx != 1.0f || scy != 1.0f) return false;
	/* What SDL queued, the clear, onto the target first; its context is
	 * then the one current */
	if (sdl.RenderFlush(r) || !sdl.GL_GetCurrentContext()) return false;
	if (!gl_start()) {
		g.dead = true;
		return false;
	}
	save(&s);
	gl.ActiveTexture(GL_TEXTURE0);
	gl.Disable(GL_BLEND);
	gl.Disable(GL_SCISSOR_TEST);
	gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	gl.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
	gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	gl.Viewport(0, 0, ow, oh);
	gl.BindVertexArray(g.vao);

	prog = g.have_frame ? filter_prog(g.filter, &soft) : 0;
	shader = prog != 0;
	border = !p8fx_covers(dst->x, dst->y, dst->w, dst->h, ow, oh) && border_ready(ow, oh);
	/* The Border over the black, the whole output, under the picture */
	if (border) {
		gl.UseProgram(g.prog_plain);
		gl.BindTexture(GL_TEXTURE_2D, g.border_tex);
		gl.Uniform1i(gl.GetUniformLocation(g.prog_plain, "u_tex"), 0);
		gl.Uniform2f(gl.GetUniformLocation(g.prog_plain, "u_size"), (float)g.border_w, (float)g.border_h);
		quad(0, 0, (float)ow, (float)oh, 0, 0, (float)g.border_w, (float)g.border_h, ow, oh);
	}
	/* The picture through the shader, read smoothly as Diatom reads it for
	 * every shader but nearest; its uniforms as Diatom's quad_scale sets
	 * them, the quad being the whole of PICO-8's screen */
	if (shader) {
		const float dw = (float)dst->w, dh = (float)dst->h;

		gl.BindTexture(GL_TEXTURE_2D, g.frame_tex);
		if (g.frame_new) {
			gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN, SCREEN, GL_RGBA, GL_UNSIGNED_BYTE, g.frame);
			g.frame_new = false;
		}
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		gl.UseProgram(prog);
		gl.Uniform1i(gl.GetUniformLocation(prog, "u_tex"), 0);
		gl.Uniform2f(gl.GetUniformLocation(prog, "u_size"), SCREEN, SCREEN);
		if (soft) gl.Uniform1f(gl.GetUniformLocation(prog, "u_strength"), (float)g.look.strength / 100.0f);
		gl.Uniform2f(gl.GetUniformLocation(prog, "u_scale"), dw / SCREEN, dh / SCREEN);
		gl.Uniform2f(gl.GetUniformLocation(prog, "u_inv"), SCREEN / dw, SCREEN / dh);
		gl.Uniform2f(gl.GetUniformLocation(prog, "u_origin"), 0, 0);
		gl.Uniform2f(gl.GetUniformLocation(prog, "u_quad"), SCREEN, SCREEN);
		/* dst may reach past the output (Zoomed, Edges Cut): GL clips it */
		quad((float)dst->x, (float)dst->y, dw, dh, 0, 0, SCREEN, SCREEN, ow, oh);
	}
	restore(&s);
	return shader;
}

/* ---------- SDL's calls, PICO-8's way in ------------------------------------- */

/* These six alone are seen from outside (the build hides the rest,
 * -fvisibility=hidden), so nothing in pico8_64 can take the place of this
 * library's own functions, nor these of anything but SDL's */
#define HOOK __attribute__((visibility("default")))

HOOK SDL_Renderer *SDL_CreateRenderer(SDL_Window *w, int index, Uint32 flags)
{
	SDL_Renderer *r;
	SDL_RendererInfo info;

	resolve();
	if (!sdl.CreateRenderer) return NULL;
	r = sdl.CreateRenderer(w, index, flags);
	if (!r || g.dead) return r;
	if (sdl.GetRendererInfo(r, &info) == 0 && info.name && !strcmp(info.name, "opengles2")) {
		g.r = r;
		g.screen = NULL;
		g.gl_tried = g.gl_ok = false;
		memset(g.prog, 0, sizeof g.prog);
		memset(g.soft, 0, sizeof g.soft);
		memset(g.failed, 0, sizeof g.failed);
		memset(g.soft_failed, 0, sizeof g.soft_failed);
		g.border_w = g.border_h = 0;
		if (g.picture) g.picture_new = true;
	} else {
		say("the renderer is not SDL's OpenGL ES 2 one; PICO-8 draws as it does");
	}
	return r;
}

HOOK void SDL_DestroyRenderer(SDL_Renderer *r)
{
	resolve();
	/* Its context, and every GL object made on it, go with it */
	if (r && r == g.r) {
		g.r = NULL;
		g.screen = NULL;
		g.gl_tried = g.gl_ok = false;
	}
	if (sdl.DestroyRenderer) sdl.DestroyRenderer(r);
}

HOOK SDL_Texture *SDL_CreateTexture(SDL_Renderer *r, Uint32 format, int access, int w, int h)
{
	SDL_Texture *t;

	resolve();
	if (!sdl.CreateTexture) return NULL;
	t = sdl.CreateTexture(r, format, access, w, h);
	/* The screen's: four bytes a pixel, B G R then alpha or nothing,
	 * streamed, square and a whole multiple of 128 (PICO-8's FPS line is
	 * 128x8) */
	if (t && !g.dead && r && r == g.r && access == SDL_TEXTUREACCESS_STREAMING &&
	    (format == SDL_PIXELFORMAT_ARGB8888 || format == SDL_PIXELFORMAT_RGB888) &&
	    w == h && w > 0 && w % SCREEN == 0) {
		g.screen = t;
		g.screen_w = w;
		g.have_frame = false;
	}
	return t;
}

HOOK void SDL_DestroyTexture(SDL_Texture *t)
{
	resolve();
	if (t && t == g.screen) g.screen = NULL;
	if (sdl.DestroyTexture) sdl.DestroyTexture(t);
}

HOOK int SDL_UpdateTexture(SDL_Texture *t, const SDL_Rect *rect, const void *pixels, int pitch)
{
	resolve();
	if (!sdl.UpdateTexture) return -1;
	/* The whole picture, kept at PICO-8's own pixels for the shader and the
	 * blur: 64 KB a frame, whatever the preblit */
	if (t && t == g.screen && !g.dead && !rect &&
	    p8fx_shrink(pixels, pitch, g.screen_w, g.screen_w, g.frame)) {
		g.have_frame = true;
		g.frame_new = true;
	}
	return sdl.UpdateTexture(t, rect, pixels, pitch);
}

HOOK int SDL_RenderCopy(SDL_Renderer *r, SDL_Texture *t, const SDL_Rect *src, const SDL_Rect *dst)
{
	resolve();
	if (!sdl.RenderCopy) return -1;
	if (t && t == g.screen && r == g.r && !g.dead && !src && dst && dst->w > 0 && dst->h > 0) {
		look_poll();
		/* Off and no border: SDL's copy alone, as with no library */
		if ((g.filter != DIATOM_FILTER_NEAREST || g.look.border != P8FX_BORDER_OFF) && fx_copy(r, dst))
			return 0;
	}
	return sdl.RenderCopy(r, t, src, dst);
}
