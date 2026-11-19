/* SPDX-License-Identifier: GPL-3.0-only */
/* Desk mode (desk.h): the host build as a handheld in a window. */
#include "desk.h"

#include "../device.h"
#include "keymap.h"
#include "profile.h"

#include <SDL_image.h>
#include <SDL_ttf.h>

#include "../../diatom/include/diatom_deskfb.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static SDL_Window   *g_win;
static SDL_Renderer *g_ren;
static SDL_Texture  *g_screen;      /* ZyperOS's 1024x768 */
static SDL_Texture  *g_panel;       /* the panel's pixels, when not the screen's */
static int           g_pw, g_ph;    /* the panel, landscape */
static float         g_zoom = 1;    /* window points a panel pixel */
static bool          g_bar = true;  /* the buttons under the screen */
static bool          g_help;
static double        g_hz = 60;

/* The buttons held by desk mode's own ways in (keys, mouse, controller,
 * socket), for the bar to light */
static Uint32 g_held;

/* A game: Diatom's frames in the window, the buttons to Diatom
 * (diatom_deskfb.h). From the launcher's device_display_release to its
 * device_display_take, as a handheld's panel goes to Diatom and back. */
static bool           g_game;
static diatom_deskfb *g_fb;
static size_t         g_fb_bytes;
static SDL_Texture   *g_game_tex;
static uint32_t       g_fb_seq;          /* the last frame shown */
static uint32_t       g_power_edges;     /* POWER's downs (1) and ups (2), queued */
static int            g_npower;

/* ---- what this run is ---------------------------------------------------- */

bool desk_on(void)
{
	static int on = -1;

	if (on < 0) {
		const char *drv = getenv("SDL_VIDEODRIVER");

		on = !getenv("ZYPEROS_HEADLESS") &&
		     !(drv && (!strcmp(drv, "dummy") || !strcmp(drv, "offscreen")));
		/* tools/desk-check.py: desk mode in SDL's dummy window */
		if (getenv("ZYPEROS_DESK_FORCE")) on = 1;
	}
	return on;
}

SDL_Texture *desk_screen(void) { return desk_on() ? g_screen : NULL; }

in_button desk_key_button(SDL_Scancode sc) { return keymap_button(sc); }

/* ---- the bar: the handheld's buttons, drawn ------------------------------ */

#define BAR_H 150     /* points */

typedef struct {
	in_button   id;
	const char *label;
	int         shape;     /* 0 round, 1 pill, 2 d-pad arm */
	SDL_FRect   r;         /* points, from the bar's top left */
	SDL_FPoint  hint;      /* where its key's name goes, the same way */
} bar_key;

static bar_key g_keys[24];
static int     g_nkeys;

static const char *profile_label(in_button b, const char *fallback)
{
	const device_profile *p = plat_profile();

	for (int i = 0; p && i < p->n_buttons; i++)
		if (p->buttons[i].id == b) return p->buttons[i].label;
	return fallback;
}

static bool profile_has(in_button b)
{
	const device_profile *p = plat_profile();

	for (int i = 0; p && i < p->n_buttons; i++)
		if (p->buttons[i].id == b) return true;
	return false;
}

static void key_at(in_button id, int shape, float cx, float cy, float w, float h)
{
	if (g_nkeys >= (int)(sizeof g_keys / sizeof g_keys[0]) || !profile_has(id)) return;
	g_keys[g_nkeys++] = (bar_key){ id, profile_label(id, "?"), shape,
	                              { cx - w / 2, cy - h / 2, w, h }, { cx, cy + h / 2 + 8 } };
}

/* A handheld's face, as much of it as a strip shows: the shoulders over the
 * d-pad and the face buttons, SELECT START and MENU between, the volume and
 * the power key under. Laid out for the window's width. */
static void bar_layout(float w)
{
	float l = w * 0.16f < 92 ? 92 : w * 0.16f, r = w - l, c = w / 2;
	float y = 75, arm = 23, face = 25;

	g_nkeys = 0;
	key_at(IN_L2, 1, l - 25, 16, 44, 20);
	key_at(IN_L1, 1, l + 25, 16, 44, 20);
	key_at(IN_R1, 1, r - 25, 16, 44, 20);
	key_at(IN_R2, 1, r + 25, 16, 44, 20);
	key_at(IN_UP,    2, l, y - arm, 24, 24);
	key_at(IN_DOWN,  2, l, y + arm, 24, 24);
	key_at(IN_LEFT,  2, l - arm, y, 24, 24);
	key_at(IN_RIGHT, 2, l + arm, y, 24, 24);
	key_at(IN_X,      0, r, y - face, 28, 28);
	key_at(IN_ACCEPT, 0, r + face, y, 28, 28);
	key_at(IN_BACK,   0, r, y + face, 28, 28);
	key_at(IN_Y,      0, r - face, y, 28, 28);
	/* Round keys printed SE and ST (the Pixel 2), or pills with the word
	 * (the Brick's SELECT and START) */
	if (strlen(profile_label(IN_SELECT, "")) > 2) {
		key_at(IN_SELECT, 1, c - 34, 54, 56, 20);
		key_at(IN_START,  1, c + 34, 54, 56, 20);
	} else {
		key_at(IN_SELECT, 0, c - 30, 54, 26, 26);
		key_at(IN_START,  0, c + 30, 54, 26, 26);
	}
	key_at(IN_MENU,   0, c, 88, 30, 30);
	key_at(IN_VOLDN,    1, c - 92, 124, 40, 18);
	key_at(IN_VOLUP,    1, c - 48, 124, 40, 18);
	key_at(IN_BRIGHTDN, 1, c + 48, 124, 40, 18);
	key_at(IN_BRIGHTUP, 1, c + 92, 124, 40, 18);
	key_at(IN_POWER,    1, c + (profile_has(IN_BRIGHTUP) ? 150 : 70), 124, 54, 18);
	/* The face buttons' keys outside the diamond, beside each, so they
	 * never sit between two buttons */
	for (int i = 0; i < g_nkeys; i++) {
		bar_key *k = &g_keys[i];
		float kx = k->r.x + k->r.w / 2, ky = k->r.y + k->r.h / 2;
		if (k->id == IN_Y) k->hint = (SDL_FPoint){ kx - k->r.w / 2 - 9, ky };
		else if (k->id == IN_X || k->id == IN_ACCEPT || k->id == IN_BACK)
			k->hint = (SDL_FPoint){ kx + k->r.w / 2 + 9, ky };
	}
}

/* ---- the window's geometry ---------------------------------------------- */

static float pixel_ratio(void)
{
	int ww, wh, pw, ph;

	SDL_GetWindowSize(g_win, &ww, &wh);
	SDL_GetRendererOutputSize(g_ren, &pw, &ph);
	return ww > 0 ? (float)pw / ww : 1;
}

/* The screen's place in the window, in points: the area over the bar, the
 * panel fitted into it whole */
static SDL_FRect screen_rect(void)
{
	int ww, wh;
	float aw, ah, s;

	SDL_GetWindowSize(g_win, &ww, &wh);
	aw = (float)ww;
	ah = (float)wh - (g_bar ? BAR_H : 0);
	if (ah < 1) ah = 1;
	s = aw / g_pw < ah / g_ph ? aw / g_pw : ah / g_ph;
	return (SDL_FRect){ (aw - g_pw * s) / 2, (ah - g_ph * s) / 2, g_pw * s, g_ph * s };
}

static void window_fit(void)
{
	SDL_SetWindowSize(g_win, (int)(g_pw * g_zoom + 0.5f), (int)(g_ph * g_zoom + 0.5f) + (g_bar ? BAR_H : 0));
	bar_layout(g_pw * g_zoom);
}

/* The panel at its own size on the computer's display: its lit width in
 * millimetres (the profile's) over the display's points an inch */
static float actual_zoom(void)
{
	const device_profile *p = plat_profile();
	float hdpi = 0, ratio = pixel_ratio();
	int d = SDL_GetWindowDisplayIndex(g_win);

	if (!p || d < 0 || SDL_GetDisplayDPI(d, NULL, &hdpi, NULL) != 0 || hdpi <= 0) return 1;
	/* SDL gives the display's pixels an inch; points are pixels over the ratio */
	return (p->screen.active_w_mm / 25.4f) * (hdpi / ratio) / g_pw;
}

/* ---- text, for the bar and the help -------------------------------------- */

typedef struct { char text[48]; int px; SDL_Color c; SDL_Texture *t; int w, h; } text_tex;
static text_tex g_text[96];
static int      g_ntext;
static TTF_Font *g_font[3];
static int       g_font_px[3];

static TTF_Font *font_px(int px)
{
	int slot = px <= 18 ? 0 : px <= 26 ? 1 : 2;

	if (!g_font[slot] || g_font_px[slot] != px) {
		if (g_font[slot]) TTF_CloseFont(g_font[slot]);
		g_font[slot] = TTF_OpenFont(P_FONT, px);
		g_font_px[slot] = px;
	}
	return g_font[slot];
}

static text_tex *text(const char *s, int px, SDL_Color c)
{
	SDL_Surface *surf;
	TTF_Font *f;

	for (int i = 0; i < g_ntext; i++)
		if (g_text[i].px == px && !memcmp(&g_text[i].c, &c, sizeof c) && !strcmp(g_text[i].text, s))
			return &g_text[i];
	if (g_ntext >= (int)(sizeof g_text / sizeof g_text[0])) {
		for (int i = 0; i < g_ntext; i++) SDL_DestroyTexture(g_text[i].t);
		g_ntext = 0;
	}
	if (!(f = font_px(px)) || !(surf = TTF_RenderUTF8_Blended(f, s, c))) return NULL;
	text_tex *t = &g_text[g_ntext];
	snprintf(t->text, sizeof t->text, "%s", s);
	t->px = px;
	t->c = c;
	t->w = surf->w;
	t->h = surf->h;
	t->t = SDL_CreateTextureFromSurface(g_ren, surf);
	SDL_FreeSurface(surf);
	if (!t->t) return NULL;
	g_ntext++;
	return t;
}

static void text_at(const char *s, float pt, SDL_Color c, float cx, float cy, float ratio)
{
	text_tex *t = text(s, (int)(pt * ratio + 0.5f), c);

	if (!t) return;
	SDL_Rect d = { (int)(cx * ratio - t->w / 2.0f), (int)(cy * ratio - t->h / 2.0f), t->w, t->h };
	SDL_RenderCopy(g_ren, t->t, NULL, &d);
}

/* ---- drawing the bar ------------------------------------------------------ */

static void fill_shape(const SDL_FRect *r, int shape)
{
	/* A round key as a stack of rows; a pill as a box and two half rounds */
	float rad = shape == 2 ? 3 * pixel_ratio() : r->h / 2;

	for (int y = 0; y < (int)r->h; y++) {
		float dy = y + 0.5f - r->h / 2, inset = 0;

		if (shape == 0) {
			float half = r->w / 2;
			float dx = half * half - dy * dy;
			inset = dx > 0 ? half - SDL_sqrtf(dx) : half;
		} else {
			float edge = r->h / 2 - rad;
			if (dy > edge || dy < -edge) {
				float ddy = (dy > 0 ? dy - edge : -dy - edge);
				float dx = rad * rad - ddy * ddy;
				inset = rad - (dx > 0 ? SDL_sqrtf(dx) : 0);
			}
		}
		SDL_FRect row = { r->x + inset, r->y + y, r->w - 2 * inset, 1 };
		if (row.w > 0) SDL_RenderFillRectF(g_ren, &row);
	}
}

static void arrow(const SDL_FRect *r, in_button dir, SDL_Color c)
{
	float cx = r->x + r->w / 2, cy = r->y + r->h / 2, s = r->w * 0.22f;
	SDL_Vertex v[3];
	SDL_FPoint p[3];

	switch (dir) {
	case IN_UP:    p[0] = (SDL_FPoint){ cx, cy - s }; p[1] = (SDL_FPoint){ cx - s, cy + s * 0.6f }; p[2] = (SDL_FPoint){ cx + s, cy + s * 0.6f }; break;
	case IN_DOWN:  p[0] = (SDL_FPoint){ cx, cy + s }; p[1] = (SDL_FPoint){ cx + s, cy - s * 0.6f }; p[2] = (SDL_FPoint){ cx - s, cy - s * 0.6f }; break;
	case IN_LEFT:  p[0] = (SDL_FPoint){ cx - s, cy }; p[1] = (SDL_FPoint){ cx + s * 0.6f, cy + s }; p[2] = (SDL_FPoint){ cx + s * 0.6f, cy - s }; break;
	default:       p[0] = (SDL_FPoint){ cx + s, cy }; p[1] = (SDL_FPoint){ cx - s * 0.6f, cy - s }; p[2] = (SDL_FPoint){ cx - s * 0.6f, cy + s }; break;
	}
	for (int i = 0; i < 3; i++) v[i] = (SDL_Vertex){ p[i], c, { 0, 0 } };
	SDL_RenderGeometry(g_ren, NULL, v, 3, NULL, 0);
}

static const SDL_Color INK = { 236, 236, 236, 255 }, HINT = { 150, 150, 156, 255 },
                       LIT_INK = { 20, 20, 24, 255 };

static void draw_bar(float ratio)
{
	int ww, wh;
	float top;

	SDL_GetWindowSize(g_win, &ww, &wh);
	top = (float)wh - BAR_H;
	SDL_SetRenderDrawColor(g_ren, 28, 28, 32, 255);
	SDL_FRect back = { 0, top * ratio, ww * ratio, BAR_H * ratio };
	SDL_RenderFillRectF(g_ren, &back);
	for (int i = 0; i < g_nkeys; i++) {
		const bar_key *k = &g_keys[i];
		bool lit = g_held >> k->id & 1;
		SDL_FRect r = { k->r.x * ratio, (top + k->r.y) * ratio, k->r.w * ratio, k->r.h * ratio };

		if (lit) SDL_SetRenderDrawColor(g_ren, 168, 214, 74, 255);   /* the A button's green */
		else     SDL_SetRenderDrawColor(g_ren, 62, 62, 68, 255);
		fill_shape(&r, k->shape);
		if (k->shape == 2) arrow(&r, k->id, lit ? LIT_INK : INK);
		else text_at(k->label, k->shape == 0 ? (strlen(k->label) > 2 ? 9 : 12) : 10,
		             lit ? LIT_INK : INK, k->r.x + k->r.w / 2, top + k->r.y + k->r.h / 2, ratio);
		/* Its key's name, under it or beside it */
		const char *hint = keymap_hint(k->id);
		if (*hint) text_at(hint, 8, HINT, k->hint.x, top + k->hint.y, ratio);
	}
	/* The d-pad's keys, once */
	for (int i = 0; i < g_nkeys; i++)
		if (g_keys[i].id == IN_DOWN) {
			text_at("arrows", 8, HINT, g_keys[i].r.x + g_keys[i].r.w / 2,
			        top + g_keys[i].r.y + g_keys[i].r.h + 7, ratio);
			break;
		}
	text_at("F1 help", 8, HINT, ww - 26.0f, top + BAR_H - 9.0f, ratio);
}

static const char *const HELP[] = {
	"Keys (by their place, as on a US keyboard)",
	"Arrows: the d-pad.  X, Z: A, B.  S, A: X, Y.",
	"Q, W: L1, R1.  E, R: L2, R2.",
	"Return: START.  Right Shift or Delete: SELECT.  Esc or M: MENU.",
	"- and =: volume.  [ and ]: brightness keys.  P: POWER.",
	"",
	"Mouse: click and hold a button below. The wheel is Up and Down.",
	"A game controller: buttons by their place (A right, B bottom).",
	"",
	"Cmd 1, 2, 3: zoom.  Cmd 0: the panel's real size.",
	"Cmd B: buttons on or off.  Cmd S: the screen to the Desktop.",
	"F1: this help.  Cmd Q: quit.",
};

static void draw_help(float ratio)
{
	int ww, wh;
	float y;

	SDL_GetWindowSize(g_win, &ww, &wh);
	SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(g_ren, 12, 12, 16, 225);
	SDL_FRect back = { 0, 0, ww * ratio, (wh - (g_bar ? BAR_H : 0)) * ratio };
	SDL_RenderFillRectF(g_ren, &back);
	y = 26;
	for (size_t i = 0; i < sizeof HELP / sizeof HELP[0]; i++, y += 22)
		if (*HELP[i]) text_at(HELP[i], i == 0 ? 13 : 12, i == 0 ? INK : (SDL_Color){ 210, 210, 214, 255 }, ww / 2.0f, y, ratio);
}

/* ---- presenting ----------------------------------------------------------- */

static Uint64 g_last_present;
static double g_present_ms[128];   /* a ring of the presents' times */
static unsigned g_presents;

static double now_ms(void)
{
	return SDL_GetPerformanceCounter() * 1000.0 / SDL_GetPerformanceFrequency();
}

/* The panel's 60 a second, on a display that may do 120: wait until a
 * frame's time has gone by since the last, less a little, and let vsync put
 * the present on the display's next refresh */
static void pace(void)
{
	Uint64 f = SDL_GetPerformanceFrequency(), now = SDL_GetPerformanceCounter();
	double period, early;

	if (g_hz <= 0 || !g_last_present) return;
	period = f / g_hz;
	early = f * 0.002;
	if ((double)(now - g_last_present) < period - early) {
		Uint32 ms = (Uint32)(((double)g_last_present + period - early - (double)now) * 1000.0 / f);
		if (ms) SDL_Delay(ms);
	}
}

/* Press to photon, as desk mode can see it: from a press (its event's time,
 * or the socket's line) to the end of the first present after the launcher
 * read it */
static double g_press_t0 = -1;     /* a press waiting for its frame */
static bool   g_press_read;        /* ...which the launcher has read */
static double g_lat[16];
static unsigned g_nlat;

static void deferred_frame(void);

static bool g_dirty;    /* desk mode's own picture changed: help, lights, zoom */

/* The window's picture: the screen, the bar, the help, presented. From the
 * launcher's present, or from desk mode itself when only its own parts
 * changed while the launcher rests (the screen texture holds the last whole
 * frame then) */
static void compose(SDL_Renderer *r, bool launcher)
{
	Uint8 cr, cg, cb, ca;
	SDL_BlendMode bm;
	SDL_Texture *src = g_game && g_game_tex ? g_game_tex : g_screen;
	float ratio = pixel_ratio();
	SDL_FRect s = screen_rect();
	SDL_Rect d = { (int)(s.x * ratio + 0.5f), (int)(s.y * ratio + 0.5f),
	               (int)(s.w * ratio + 0.5f), (int)(s.h * ratio + 0.5f) };

	SDL_GetRenderDrawColor(r, &cr, &cg, &cb, &ca);
	SDL_GetRenderDrawBlendMode(r, &bm);
	/* Down to the panel's own pixels, as the Pixel 2 scales its screen (a
	 * game's frame is the panel's size already) */
	if (g_panel && src == g_screen) {
		SDL_SetRenderTarget(r, g_panel);
		SDL_RenderCopy(r, g_screen, NULL, NULL);
		src = g_panel;
	}
	SDL_SetRenderTarget(r, NULL);
	SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
	SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
	SDL_RenderClear(r);
	/* Each panel pixel a block of the window's when the zoom is whole */
	{
		float k = (float)d.w / g_pw;
		SDL_SetTextureScaleMode(src, k - (int)k < 0.01f ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
	}
	SDL_RenderCopy(r, src, NULL, &d);
	SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
	if (g_bar) draw_bar(ratio);
	if (g_help) draw_help(ratio);
	/* A game's frames come at the core's own rate, Diatom's pacing: each
	 * goes up as it comes */
	if (!g_game) pace();
	SDL_RenderPresent(r);
	g_dirty = false;
	g_last_present = SDL_GetPerformanceCounter();
	if (launcher) {
		double t = now_ms();

		g_present_ms[g_presents++ % 128] = t;
		if (g_press_t0 >= 0 && g_press_read) {
			g_lat[g_nlat++ % 16] = t - g_press_t0;
			g_press_t0 = -1;
			g_press_read = false;
		}
	}
	SDL_SetRenderTarget(r, g_screen);
	SDL_SetRenderDrawBlendMode(r, bm);
	SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
	if (launcher) deferred_frame();
}

void desk_present(SDL_Renderer *r) { compose(r, true); }

/* ---- the window ----------------------------------------------------------- */

static void ctl_start(void);

bool desk_video_init(SDL_Window **win, SDL_Renderer **ren)
{
	const device_profile *p = plat_profile();
	const char *e;
	char title[96];

	g_pw = p->screen.turn % 180 ? p->screen.panel_h : p->screen.panel_w;
	g_ph = p->screen.turn % 180 ? p->screen.panel_w : p->screen.panel_h;
	if ((e = getenv("ZYPEROS_DESK_ZOOM")) && atof(e) >= 0.25 && atof(e) <= 6) g_zoom = (float)atof(e);
	if ((e = getenv("ZYPEROS_DESK_BAR"))) g_bar = atoi(e) != 0;
	if ((e = getenv("ZYPEROS_DESK_HZ"))) g_hz = atof(e);
	snprintf(title, sizeof title, "ZyperOS - %s", p->name);

	/* A controller's buttons by their place, never their printed letters:
	 * A on the right, B at the bottom, as the handheld has them */
	SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
	if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
		fprintf(stderr, "desk: no game controllers (%s)\n", SDL_GetError());
	if (!TTF_WasInit()) TTF_Init();

	g_win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                         (int)(g_pw * g_zoom + 0.5f), (int)(g_ph * g_zoom + 0.5f) + (g_bar ? BAR_H : 0),
	                         SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
	if (!g_win) {
		fprintf(stderr, "window: %s\n", SDL_GetError());
		return false;
	}
	g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC |
	                                      SDL_RENDERER_TARGETTEXTURE);
	/* No GPU (SDL's dummy window, a check): drawn in software */
	if (!g_ren) g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE);
	if (!g_ren) {
		fprintf(stderr, "renderer: %s\n", SDL_GetError());
		return false;
	}
	g_screen = SDL_CreateTexture(g_ren, PLAT_TARGET_FORMAT, SDL_TEXTUREACCESS_TARGET,
	                             ZYPEROS_SCREEN_W, ZYPEROS_SCREEN_H);
	if (!g_screen) {
		fprintf(stderr, "desk: no screen texture (%s)\n", SDL_GetError());
		return false;
	}
	SDL_SetTextureBlendMode(g_screen, SDL_BLENDMODE_NONE);
	SDL_SetTextureScaleMode(g_screen, SDL_ScaleModeLinear);
	if (g_pw != ZYPEROS_SCREEN_W || g_ph != ZYPEROS_SCREEN_H) {
		g_panel = SDL_CreateTexture(g_ren, PLAT_TARGET_FORMAT, SDL_TEXTUREACCESS_TARGET, g_pw, g_ph);
		if (g_panel) SDL_SetTextureBlendMode(g_panel, SDL_BLENDMODE_NONE);
	}
	SDL_SetWindowMinimumSize(g_win, g_pw / 4, g_ph / 4 + (g_bar ? BAR_H : 0));
	bar_layout(g_pw * g_zoom);
	SDL_SetRenderTarget(g_ren, g_screen);
	SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
	SDL_RenderClear(g_ren);
	*win = g_win;
	*ren = g_ren;
	fprintf(stderr,
	        "desk: %s, panel %dx%d at %.2fx, %s a second. Keys: arrows d-pad, X Z S A = A B X Y,\n"
	        "desk: Q W E R = L1 R1 L2 R2, Return START, Right Shift or Delete SELECT, Esc or M MENU,\n"
	        "desk: - = volume, P power. F1 in the window lists the rest.\n",
	        p->name, g_pw, g_ph, g_zoom, g_hz > 0 ? "60 frames" : "the display's frames");
	ctl_start();
	return true;
}

void desk_video_quit(void)
{
	for (int i = 0; i < g_ntext; i++) SDL_DestroyTexture(g_text[i].t);
	g_ntext = 0;
	for (int i = 0; i < 3; i++) if (g_font[i]) { TTF_CloseFont(g_font[i]); g_font[i] = NULL; }
	if (g_panel) { SDL_DestroyTexture(g_panel); g_panel = NULL; }
	if (g_screen) { SDL_DestroyTexture(g_screen); g_screen = NULL; }
	g_ren = NULL;
	g_win = NULL;
}

/* The panel as a PNG: what the window shows, at the panel's own pixels */
static bool save_panel(const char *path)
{
	if (g_game && g_fb) {
		/* The game's frame, as the window shows it */
		SDL_Surface *f = SDL_CreateRGBSurfaceWithFormatFrom(g_fb + 1, (int)g_fb->w, (int)g_fb->h, 32,
		                                                    (int)g_fb->w * 4, SDL_PIXELFORMAT_ARGB8888);
		SDL_Surface *o = f ? SDL_ConvertSurfaceFormat(f, SDL_PIXELFORMAT_RGB24, 0) : NULL;
		bool ok = o && IMG_SavePNG(o, path) == 0;

		SDL_FreeSurface(o);
		SDL_FreeSurface(f);
		return ok;
	}
	SDL_Texture *src = g_panel ? g_panel : g_screen, *was = SDL_GetRenderTarget(g_ren);
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, g_pw, g_ph, 32, SDL_PIXELFORMAT_RGBA32);
	bool ok = false;

	if (!s) return false;
	if (g_panel) {
		SDL_SetRenderTarget(g_ren, g_panel);
		SDL_RenderCopy(g_ren, g_screen, NULL, NULL);
	}
	SDL_SetRenderTarget(g_ren, src);
	if (SDL_RenderReadPixels(g_ren, NULL, SDL_PIXELFORMAT_RGBA32, s->pixels, s->pitch) == 0) {
		/* The screen keeps no alpha of its own: opaque */
		for (int i = 0; i < g_pw * g_ph; i++) ((Uint8 *)s->pixels)[i * 4 + 3] = 255;
		ok = IMG_SavePNG(s, path) == 0;
	}
	SDL_SetRenderTarget(g_ren, was);
	SDL_FreeSurface(s);
	return ok;
}

static void shot_to_desktop(void)
{
	char path[512], stamp[32];
	time_t t = time(NULL);
	const char *home = getenv("HOME");

	strftime(stamp, sizeof stamp, "%Y-%m-%d at %H.%M.%S", localtime(&t));
	snprintf(path, sizeof path, "%s/Desktop/ZyperOS %s.png", home ? home : ".", stamp);
	fprintf(stderr, "desk: %s %s\n", save_panel(path) ? "saved" : "could not save", path);
}

/* ---- input ------------------------------------------------------------------ */

static void fb_buttons(void);
static uint32_t diatom_buttons(Uint32 held);
static uint32_t g_game_press;    /* a press in a game: Diatom's bit, until a frame used it */

static void set_btn(in_state *st, in_button b, bool down)
{
	if (b == IN_NONE) return;
	if ((g_held >> b & 1) != (Uint32)down) g_dirty = true;
	if (down) {
		if (!(g_held >> b & 1) && g_press_t0 < 0) {
			g_press_t0 = now_ms();
			g_press_read = !g_game;  /* the launcher reads st after this poll */
			/* in a game, the first frame Diatom made with it */
			g_game_press = g_game ? diatom_buttons(1u << b) : 0;
		}
		g_held |= 1u << b;
	} else {
		g_held &= ~(1u << b);
	}
	if (!g_game) {
		plat_set_btn(st, b, down);
	} else if (b != IN_POWER) {
		fb_buttons();
	} else if (g_npower < 16) {
		/* POWER is the launcher's in a game too (device_power_pressed) */
		g_power_edges |= (down ? 1u : 2u) << (2 * g_npower++);
	}
}

static uint32_t diatom_buttons(Uint32 held)
{
	static const struct { in_button b; int bit; } map[] = {
		{ IN_UP, 0 }, { IN_DOWN, 1 }, { IN_LEFT, 2 }, { IN_RIGHT, 3 },
		{ IN_ACCEPT, 4 }, { IN_BACK, 5 }, { IN_X, 6 }, { IN_Y, 7 },
		{ IN_L1, 8 }, { IN_R1, 9 }, { IN_L2, 10 }, { IN_R2, 11 },
		{ IN_SELECT, 12 }, { IN_START, 13 }, { IN_MENU, 14 },
	};
	uint32_t m = 0;

	for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
		if (held >> map[i].b & 1) m |= 1u << map[i].bit;
	return m;
}

static void fb_buttons(void)
{
	if (g_fb) __atomic_store_n(&g_fb->buttons, g_game ? diatom_buttons(g_held) : 0, __ATOMIC_RELEASE);
}

/* The wheel's taps: down now, up at the next poll */
static Uint32 g_tap_release;
static float  g_wheel;

void desk_poll_begin(in_state *st)
{
	for (int b = 0; b < IN_COUNT; b++)
		if (g_tap_release >> b & 1) set_btn(st, (in_button)b, false);
	g_tap_release = 0;
}

static void tap(in_state *st, in_button b)
{
	set_btn(st, b, true);
	g_tap_release |= 1u << b;
}

/* The mouse on the bar */
static in_button g_mouse = IN_NONE;

static in_button bar_hit(int x, int y)
{
	int ww, wh;

	if (!g_bar) return IN_NONE;
	SDL_GetWindowSize(g_win, &ww, &wh);
	float by = (float)y - ((float)wh - BAR_H);
	for (int i = 0; i < g_nkeys; i++) {
		const SDL_FRect *r = &g_keys[i].r;
		if (x >= r->x - 3 && x <= r->x + r->w + 3 && by >= r->y - 3 && by <= r->y + r->h + 3)
			return g_keys[i].id;
	}
	return IN_NONE;
}

static bool is_dpad(in_button b) { return b == IN_UP || b == IN_DOWN || b == IN_LEFT || b == IN_RIGHT; }

/* A controller: its buttons by place, its left stick as the d-pad, its
 * triggers as L2 and R2 */
static in_button pad_button(Uint8 b)
{
	switch (b) {
	case SDL_CONTROLLER_BUTTON_B:             return IN_ACCEPT;   /* east */
	case SDL_CONTROLLER_BUTTON_A:             return IN_BACK;     /* south */
	case SDL_CONTROLLER_BUTTON_Y:             return IN_X;        /* north */
	case SDL_CONTROLLER_BUTTON_X:             return IN_Y;        /* west */
	case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return IN_L1;
	case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return IN_R1;
	case SDL_CONTROLLER_BUTTON_BACK:          return IN_SELECT;
	case SDL_CONTROLLER_BUTTON_START:         return IN_START;
	case SDL_CONTROLLER_BUTTON_GUIDE:         return IN_MENU;
	case SDL_CONTROLLER_BUTTON_MISC1:         return IN_MENU;
	case SDL_CONTROLLER_BUTTON_DPAD_UP:       return IN_UP;
	case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     return IN_DOWN;
	case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     return IN_LEFT;
	case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    return IN_RIGHT;
	default:                                  return IN_NONE;
	}
}

static void pad_axis(in_state *st, Uint8 axis, Sint16 v)
{
	static bool on[6][2];
	const int press = 16000, let_go = 11000;
	in_button lo = IN_NONE, hi = IN_NONE;

	switch (axis) {
	case SDL_CONTROLLER_AXIS_LEFTX:        lo = IN_LEFT; hi = IN_RIGHT; break;
	case SDL_CONTROLLER_AXIS_LEFTY:        lo = IN_UP;   hi = IN_DOWN;  break;
	case SDL_CONTROLLER_AXIS_TRIGGERLEFT:  hi = IN_L2; break;
	case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: hi = IN_R2; break;
	default: return;
	}
	if (axis >= 6) return;
	if (lo != IN_NONE) {
		bool want = on[axis][0] ? v < -let_go : v < -press;
		if (want != on[axis][0]) { on[axis][0] = want; set_btn(st, lo, want); }
	}
	{
		bool want = on[axis][1] ? v > let_go : v > press;
		if (want != on[axis][1]) { on[axis][1] = want; set_btn(st, hi, want); }
	}
}

static void zoom_to(float z)
{
	if (z < 0.25f) z = 0.25f;
	if (z > 6) z = 6;
	g_zoom = z;
	window_fit();
	g_dirty = true;
	fprintf(stderr, "desk: zoom %.2fx\n", z);
}

static void ctl_event(const SDL_Event *e, in_state *st);
static void ctl_drop(const SDL_Event *e);
static Uint32 g_ctl_event = (Uint32)-1;

bool desk_event(const SDL_Event *e, in_state *st)
{
	switch (e->type) {
	case SDL_KEYDOWN:
	case SDL_KEYUP: {
		SDL_Keymod mod = e->key.keysym.mod;
		bool down = e->type == SDL_KEYDOWN;

		if (mod & (KMOD_GUI | KMOD_CTRL)) {
			/* The window's own keys, Command (Control elsewhere) and a key */
			if (!down || e->key.repeat) return true;
			switch (e->key.keysym.scancode) {
			case SDL_SCANCODE_1: zoom_to(1); break;
			case SDL_SCANCODE_2: zoom_to(2); break;
			case SDL_SCANCODE_3: zoom_to(3); break;
			case SDL_SCANCODE_0: zoom_to(actual_zoom()); break;
			case SDL_SCANCODE_B: g_bar = !g_bar; window_fit(); g_dirty = true; break;
			case SDL_SCANCODE_S: shot_to_desktop(); break;
			case SDL_SCANCODE_SLASH: g_help = !g_help; g_dirty = true; break;
			default: return false;   /* Cmd Q and the rest are the system's */
			}
			return true;
		}
		if (e->key.keysym.scancode == SDL_SCANCODE_F1) {
			if (down && !e->key.repeat) { g_help = !g_help; g_dirty = true; }
			return true;
		}
		if (e->key.repeat) return true;
		set_btn(st, keymap_button(e->key.keysym.scancode), down);
		return true;
	}
	case SDL_MOUSEBUTTONDOWN:
		if (e->button.button != SDL_BUTTON_LEFT) return true;
		g_mouse = bar_hit(e->button.x, e->button.y);
		if (g_mouse != IN_NONE) set_btn(st, g_mouse, true);
		return true;
	case SDL_MOUSEBUTTONUP:
		if (e->button.button == SDL_BUTTON_LEFT && g_mouse != IN_NONE) {
			set_btn(st, g_mouse, false);
			g_mouse = IN_NONE;
		}
		return true;
	case SDL_MOUSEMOTION:
		/* The d-pad rolls from one way to the next under a held press */
		if (g_mouse != IN_NONE && is_dpad(g_mouse)) {
			in_button now = bar_hit(e->motion.x, e->motion.y);
			if (is_dpad(now) && now != g_mouse) {
				set_btn(st, g_mouse, false);
				g_mouse = now;
				set_btn(st, g_mouse, true);
			}
		}
		return true;
	case SDL_MOUSEWHEEL: {
		float v = e->wheel.preciseY != 0 ? e->wheel.preciseY : (float)e->wheel.y;
		float h = e->wheel.preciseX != 0 ? e->wheel.preciseX : (float)e->wheel.x;

		if (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) { v = -v; h = -h; }
		/* A notch is a press; a trackpad's flick is many small steps */
		g_wheel += v;
		while (g_wheel >= 1)  { tap(st, IN_UP);   g_wheel -= 1; }
		while (g_wheel <= -1) { tap(st, IN_DOWN); g_wheel += 1; }
		if (h >= 1) tap(st, IN_RIGHT);
		else if (h <= -1) tap(st, IN_LEFT);
		return true;
	}
	case SDL_CONTROLLERDEVICEADDED: {
		SDL_GameController *c = SDL_GameControllerOpen(e->cdevice.which);
		if (c) fprintf(stderr, "desk: controller \"%s\"\n", SDL_GameControllerName(c));
		return true;
	}
	case SDL_CONTROLLERDEVICEREMOVED: {
		SDL_GameController *c = SDL_GameControllerFromInstanceID(e->cdevice.which);
		if (c) SDL_GameControllerClose(c);
		return true;
	}
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		set_btn(st, pad_button(e->cbutton.button), e->type == SDL_CONTROLLERBUTTONDOWN);
		return true;
	case SDL_CONTROLLERAXISMOTION:
		pad_axis(st, e->caxis.axis, e->caxis.value);
		return true;
	case SDL_WINDOWEVENT:
		if (e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
			int ww, wh;
			SDL_GetWindowSize(g_win, &ww, &wh);
			bar_layout((float)ww);
		}
		if (e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e->window.event == SDL_WINDOWEVENT_EXPOSED)
			g_dirty = true;
		return true;
	default:
		if (e->type == g_ctl_event) {
			ctl_event(e, st);
			return true;
		}
		return false;
	}
}

void desk_wait(unsigned ms)
{
	/* The launcher rests with a whole frame drawn: show what desk mode
	 * changed meanwhile, then wait. The event stays queued for the poll
	 * that follows. */
	if (g_dirty && g_ren) compose(g_ren, false);
	SDL_WaitEventTimeout(NULL, (int)ms);
}

/* ---- the control socket (tools/desk) ---------------------------------------
 *
 * One command a connection, one JSON line back, as simulator/simctl's: a
 * thread takes the line and hands it to the main thread as an event, which
 * wakes the launcher's wait; the main thread does it between frames and
 * answers. A tap's let-go and a wait for frames come back the same way. */

enum { CTL_LINE, CTL_RELEASE };

typedef struct { int fd; int frames_left; bool latency; } ctl_wait;
static ctl_wait g_waits[8];
static int g_nwaits;
static int g_listen = -1;

static void reply(int fd, const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof buf - 1, fmt, ap);
	va_end(ap);
	if (n < 0) n = 0;
	if (n > (int)sizeof buf - 2) n = (int)sizeof buf - 2;
	buf[n++] = '\n';
	if (write(fd, buf, (size_t)n) < 0) { /* the client went */ }
	close(fd);
}

static void *ctl_thread(void *arg)
{
	(void)arg;
	for (;;) {
		int fd = accept(g_listen, NULL, NULL);
		char line[1024];
		size_t used = 0;
		ssize_t n;

		if (fd < 0) {
			if (errno == EINTR) continue;
			return NULL;
		}
		while (used < sizeof line - 1 && (n = read(fd, line + used, sizeof line - 1 - used)) > 0) {
			used += (size_t)n;
			if (memchr(line, '\n', used)) break;
		}
		line[used] = '\0';
		line[strcspn(line, "\r\n")] = '\0';
		SDL_Event e;
		SDL_zero(e);
		e.type = g_ctl_event;
		e.user.code = CTL_LINE;
		e.user.data1 = strdup(line);
		e.user.data2 = (void *)(intptr_t)fd;
		if (!e.user.data1 || SDL_PushEvent(&e) != 1) {
			free(e.user.data1);
			close(fd);
		}
	}
}

static void ctl_start(void)
{
	const char *path = getenv("ZYPEROS_DESK_SOCK");
	struct sockaddr_un a;
	pthread_t t;

	/* Once a run: the window can be made again (a port's menu) */
	if (g_ctl_event != (Uint32)-1) return;
	g_ctl_event = SDL_RegisterEvents(1);
	if (!path || !*path || strlen(path) >= sizeof a.sun_path) return;
	unlink(path);
	g_listen = socket(AF_UNIX, SOCK_STREAM, 0);
	if (g_listen < 0) return;
	memset(&a, 0, sizeof a);
	a.sun_family = AF_UNIX;
	snprintf(a.sun_path, sizeof a.sun_path, "%s", path);
	mode_t old = umask(077);
	int ok = bind(g_listen, (struct sockaddr *)&a, sizeof a) == 0 && listen(g_listen, 8) == 0;
	umask(old);
	if (!ok || pthread_create(&t, NULL, ctl_thread, NULL) != 0) {
		fprintf(stderr, "desk: no control socket at %s (%s)\n", path, strerror(errno));
		close(g_listen);
		g_listen = -1;
		return;
	}
	pthread_detach(t);
	fprintf(stderr, "desk: control socket %s (tools/desk)\n", path);
}

static const char *const BTN_NAMES[IN_COUNT] = {
	[IN_LEFT] = "left", [IN_RIGHT] = "right", [IN_UP] = "up", [IN_DOWN] = "down",
	[IN_ACCEPT] = "a", [IN_BACK] = "b", [IN_X] = "x", [IN_Y] = "y",
	[IN_L1] = "l1", [IN_R1] = "r1", [IN_START] = "start", [IN_SELECT] = "select",
	[IN_MENU] = "menu", [IN_VOLUP] = "volup", [IN_VOLDN] = "voldown",
	[IN_BRIGHTUP] = "brightup", [IN_BRIGHTDN] = "brightdown", [IN_POWER] = "power",
	[IN_L2] = "l2", [IN_R2] = "r2",
};

/* "a+right" as a mask of buttons; 0 when one is no button */
static Uint32 chord(const char *s)
{
	char buf[128], *tok, *save = NULL;
	Uint32 m = 0;

	snprintf(buf, sizeof buf, "%s", s);
	for (tok = strtok_r(buf, "+", &save); tok; tok = strtok_r(NULL, "+", &save)) {
		int b;
		for (b = 0; b < IN_COUNT; b++)
			if (BTN_NAMES[b] && !strcmp(tok, BTN_NAMES[b])) break;
		if (b == IN_COUNT) return 0;
		m |= 1u << b;
	}
	return m;
}

static Uint32 release_timer(Uint32 ms, void *mask)
{
	SDL_Event e;

	(void)ms;
	SDL_zero(e);
	e.type = g_ctl_event;
	e.user.code = CTL_RELEASE;
	e.user.data1 = mask;
	SDL_PushEvent(&e);
	return 0;
}

static double fps_last_second(void)
{
	double now = now_ms();
	int n = 0;

	for (unsigned i = 0; i < 128 && i < g_presents; i++)
		if (now - g_present_ms[(g_presents - 1 - i) % 128] <= 1000.0) n++;
	return n;
}

static void held_json(char *out, size_t cap)
{
	size_t used = 0;

	out[0] = '\0';
	for (int b = 0; b < IN_COUNT; b++)
		if ((g_held >> b & 1) && BTN_NAMES[b])
			used += (size_t)snprintf(out + used, used < cap ? cap - used : 0, "%s\"%s\"", used ? "," : "", BTN_NAMES[b]);
}

static void ctl_event(const SDL_Event *e, in_state *st)
{
	if (e->user.code == CTL_RELEASE) {
		Uint32 m = (Uint32)(uintptr_t)e->user.data1;
		for (int b = 0; b < IN_COUNT; b++) if (m >> b & 1) set_btn(st, (in_button)b, false);
		return;
	}
	char *line = e->user.data1, verb[32] = "", a1[512] = "", a2[64] = "";
	int fd = (int)(intptr_t)e->user.data2;
	sscanf(line, "%31s %511s %63s", verb, a1, a2);

	if (!strcmp(verb, "state")) {
		char held[256];
		held_json(held, sizeof held);
		reply(fd, "{\"ok\":true,\"device\":\"%s\",\"panel\":[%d,%d],\"zoom\":%.2f,\"bar\":%s,"
		          "\"hz\":%.0f,\"fps\":%.0f,\"held\":[%s]}",
		      plat_profile()->id, g_pw, g_ph, g_zoom, g_bar ? "true" : "false", g_hz, fps_last_second(), held);
	} else if (!strcmp(verb, "press") || !strcmp(verb, "release") || !strcmp(verb, "tap") ||
	           !strcmp(verb, "latency")) {
		Uint32 m = chord(a1);
		int ms = *a2 ? atoi(a2) : 80;
		if (!m || ms < 1 || ms > 5000) {
			reply(fd, "{\"ok\":false,\"error\":\"buttons: up down left right a b x y l1 r1 l2 r2 select start menu volup voldown brightup brightdown power, joined with +\"}");
		} else {
			bool down = strcmp(verb, "release") != 0;
			if (!strcmp(verb, "latency")) g_press_t0 = -1;    /* this press is the one timed */
			for (int b = 0; b < IN_COUNT; b++) if (m >> b & 1) set_btn(st, (in_button)b, down);
			if (!strcmp(verb, "tap") || !strcmp(verb, "latency"))
				SDL_AddTimer((Uint32)ms, release_timer, (void *)(uintptr_t)m);
			if (!strcmp(verb, "latency") && g_nwaits < 8)
				g_waits[g_nwaits++] = (ctl_wait){ fd, 0, true };
			else
				reply(fd, "{\"ok\":true}");
		}
	} else if (!strcmp(verb, "shot") && *a1) {
		bool ok = save_panel(a1);
		reply(fd, "{\"ok\":%s,\"file\":\"%s\"}", ok ? "true" : "false", a1);
	} else if (!strcmp(verb, "wait-frames") && atoi(a1) > 0 && g_nwaits < 8) {
		g_waits[g_nwaits++] = (ctl_wait){ fd, atoi(a1), false };
	} else if (!strcmp(verb, "stats")) {
		char lat[256] = "";
		size_t used = 0;
		unsigned n = g_nlat < 16 ? g_nlat : 16;
		for (unsigned i = 0; i < n; i++)
			used += (size_t)snprintf(lat + used, sizeof lat - used, "%s%.1f", i ? "," : "",
			                         g_lat[(g_nlat - n + i) % 16]);
		reply(fd, "{\"ok\":true,\"fps\":%.0f,\"frames\":%u,\"press_to_present_ms\":[%s]}",
		      fps_last_second(), g_presents, lat);
	} else if (!strcmp(verb, "zoom") && atof(a1) > 0) {
		zoom_to((float)atof(a1));
		reply(fd, "{\"ok\":true,\"zoom\":%.2f}", g_zoom);
	} else if (!strcmp(verb, "bar") && (*a1 == '0' || *a1 == '1')) {
		g_bar = *a1 == '1';
		window_fit();
		g_dirty = true;
		reply(fd, "{\"ok\":true}");
	} else if (!strcmp(verb, "help")) {
		g_help = *a1 ? *a1 == '1' : !g_help;
		g_dirty = true;
		reply(fd, "{\"ok\":true,\"help\":%s}", g_help ? "true" : "false");
	} else if (!strcmp(verb, "quit")) {
		SDL_Event q;
		SDL_zero(q);
		q.type = SDL_QUIT;
		SDL_PushEvent(&q);
		reply(fd, "{\"ok\":true}");
	} else {
		reply(fd, "{\"ok\":false,\"error\":\"state | press|release|tap BUTTON[+BUTTON] [MS] | latency BUTTON | "
		          "shot FILE.png | wait-frames N | stats | zoom N | bar 0|1 | help [0|1] | quit\"}");
	}
	free(line);
}

/* A command that could not be kept: answered, never left hanging */
static void ctl_drop(const SDL_Event *e)
{
	if (e->user.code != CTL_LINE) return;
	reply((int)(intptr_t)e->user.data2, "{\"ok\":false,\"error\":\"busy: try again\"}");
	free(e->user.data1);
}

/* After each present: the waits for frames count down, a timed press says
 * how long its frame took */
static void deferred_frame(void)
{
	for (int i = 0; i < g_nwaits; i++) {
		ctl_wait *w = &g_waits[i];
		bool done = false;

		if (w->latency) {
			if (g_press_t0 < 0 && g_nlat) {
				reply(w->fd, "{\"ok\":true,\"press_to_present_ms\":%.1f}", g_lat[(g_nlat - 1) % 16]);
				done = true;
			}
		} else if (--w->frames_left <= 0) {
			reply(w->fd, "{\"ok\":true}");
			done = true;
		}
		if (done) {
			g_waits[i--] = g_waits[--g_nwaits];
		}
	}
}

/* ---- a game ----------------------------------------------------------------- */

static bool fb_map(void)
{
	const char *path = getenv("ZYPEROS_DESK_FB");
	struct stat sb;
	void *m;
	int fd;

	if (g_fb) return true;
	if (!path || !*path || (fd = open(path, O_RDWR | O_CLOEXEC)) < 0) return false;
	if (fstat(fd, &sb) != 0 || (size_t)sb.st_size < sizeof(diatom_deskfb)) { close(fd); return false; }
	m = mmap(NULL, (size_t)sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (m == MAP_FAILED) return false;
	g_fb = m;
	g_fb_bytes = (size_t)sb.st_size;
	if (__atomic_load_n(&g_fb->magic, __ATOMIC_ACQUIRE) != DIATOM_DESKFB_MAGIC ||
	    g_fb->version != DIATOM_DESKFB_VERSION || !g_fb->w || !g_fb->h ||
	    DIATOM_DESKFB_BYTES(g_fb->w, g_fb->h) > g_fb_bytes) {
		fprintf(stderr, "desk: %s is no desk frame file of Diatom's\n", path);
		munmap(m, g_fb_bytes);
		g_fb = NULL;
		return false;
	}
	return true;
}

void desk_game_begin(void)
{
	if (!g_ren || !fb_map()) {
		/* No Diatom of desk mode's (tools/desk runs one): the window keeps
		 * the launcher's last frame, as it always did */
		return;
	}
	if (!g_game_tex) {
		g_game_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		                               (int)g_fb->w, (int)g_fb->h);
		if (!g_game_tex) return;
		SDL_SetTextureBlendMode(g_game_tex, SDL_BLENDMODE_NONE);
	}
	g_game = true;
	/* The frame already there is the last game's: wait for a new one */
	g_fb_seq = __atomic_load_n(&g_fb->seq, __ATOMIC_ACQUIRE);
	fb_buttons();
}

void desk_game_end(void)
{
	if (!g_game) return;
	g_game = false;
	g_game_press = 0;
	fb_buttons();
	g_dirty = true;
}

int desk_power_edge(void)
{
	int v;

	if (!g_npower) return -1;
	v = (int)(g_power_edges & 3u);
	g_power_edges >>= 2;
	g_npower--;
	return v == 1;
}

unsigned desk_game_wait(unsigned ms)
{
	static in_state none;    /* the launcher reads no buttons in a game */
	SDL_Event e;
	uint32_t seq;

	if (!g_game) return ms;
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT) {
			/* The window closed: as POWER, the game saved on its way out */
			if (g_npower < 15) {
				g_power_edges |= (1u | 2u << 2) << (2 * g_npower);
				g_npower += 2;
			}
			continue;
		}
		desk_event(&e, &none);
	}
	seq = __atomic_load_n(&g_fb->seq, __ATOMIC_ACQUIRE);
	if (!(seq & 1u) && seq != g_fb_seq) {
		SDL_UpdateTexture(g_game_tex, NULL, g_fb + 1, (int)g_fb->w * 4);
		/* A frame half copied over meanwhile is shown as it is, once:
		 * the next is whole */
		if (g_game_press && (g_fb->polled & g_game_press)) {
			g_press_read = true;
			g_game_press = 0;
		}
		g_fb_seq = seq;
		compose(g_ren, true);
		return 0;
	}
	if (g_dirty) compose(g_ren, false);
	return ms < 4 ? ms : 4;
}

void desk_flush(void)
{
	SDL_Event e, keep[32];
	int n = 0;

	SDL_PumpEvents();
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_KEYUP:
			if (!(e.key.keysym.mod & (KMOD_GUI | KMOD_CTRL))) {
				in_button b = keymap_button(e.key.keysym.scancode);
				if (b != IN_NONE) g_held &= ~(1u << b);
			}
			break;
		case SDL_MOUSEBUTTONUP:
			if (g_mouse != IN_NONE) g_held &= ~(1u << g_mouse);
			g_mouse = IN_NONE;
			break;
		case SDL_CONTROLLERBUTTONUP: {
			in_button b = pad_button(e.cbutton.button);
			if (b != IN_NONE) g_held &= ~(1u << b);
			break;
		}
		case SDL_QUIT:
		case SDL_CONTROLLERDEVICEADDED:
		case SDL_CONTROLLERDEVICEREMOVED:
		case SDL_WINDOWEVENT:
			if (n < 32) keep[n++] = e;
			break;
		default:
			if (e.type == g_ctl_event && n < 32) keep[n++] = e;
			else if (e.type == g_ctl_event) ctl_drop(&e);
			break;
		}
	}
	g_dirty = true;
	for (int i = 0; i < n; i++) SDL_PushEvent(&keep[i]);
}
