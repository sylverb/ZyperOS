/* SPDX-License-Identifier: GPL-3.0-only */
/* Desk mode's handheld, drawn (deskface.h). */
#include "deskface.h"

#include "../platform.h"

#include <SDL_ttf.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---- the two faces, in millimetres from the middle of the body, up positive */

enum { K_ROUND, K_PILL, K_SIDE, K_SHOULDER, K_DPAD };
/* Where a key's name goes beside it */
enum { H_BELOW, H_RIGHT, H_LEFT, H_ABOVE, H_ON };

typedef struct {
	in_button   id;
	int         kind;
	float       x, y, w, h;   /* its middle and its size */
	const char *label;        /* printed on it ("" none) */
	int         hint;
	const char *under;        /* printed on the body under it, or NULL */
} fkey;

typedef struct { const char *text; float x, y; bool turned; } flabel;

typedef struct {
	const char *id;
	float body_w, body_h, corner;
	float margin_x, margin_top, margin_bottom;
	SDL_FRect glass;          /* centre x, centre y, w, h */
	float glass_r;
	SDL_FRect screen;
	float screen_mm;          /* its lit width */
	float word_y;             /* "ZyperOS" under the picture; NAN none */
	SDL_FRect recess;         /* SELECT and START's well; w 0 none */
	float recess_r;
	float dpad_x, dpad_y, dpad_span, dpad_arm;   /* half the cross, half an arm */
	float led_x, led_w, led_h, led_y[4];         /* the side lights; led_w 0 none */
	Uint32 body_hi, body, body_lo, recess_c, cap, cap_down, marking, glass_c, led_off;
	const fkey *keys;
	int n_keys;
	const flabel *labels;
	int n_labels;
} face_def;

static const fkey pixel2_keys[] = {
	{ IN_UP,     K_DPAD, 0, 0, 0, 0, "", H_ABOVE, NULL },
	{ IN_DOWN,   K_DPAD, 0, 0, 0, 0, "", H_BELOW, NULL },
	{ IN_LEFT,   K_DPAD, 0, 0, 0, 0, "", H_LEFT,  NULL },
	{ IN_RIGHT,  K_DPAD, 0, 0, 0, 0, "", H_RIGHT, NULL },
	{ IN_X,      K_ROUND,  16.9f, -14.8f, 6.1f, 6.1f, "X", H_ABOVE, NULL },
	{ IN_Y,      K_ROUND,  10.2f, -21.5f, 6.1f, 6.1f, "Y", H_LEFT,  NULL },
	{ IN_ACCEPT, K_ROUND,  23.6f, -21.5f, 6.1f, 6.1f, "A", H_RIGHT, NULL },
	{ IN_BACK,   K_ROUND,  16.9f, -28.2f, 6.1f, 6.1f, "B", H_BELOW, NULL },
	{ IN_SELECT, K_ROUND,  -3.8f, -29.6f, 5.7f, 5.7f, "SE", H_BELOW, NULL },
	{ IN_START,  K_ROUND,   4.0f, -29.6f, 5.7f, 5.7f, "ST", H_BELOW, NULL },
	{ IN_L1,     K_SHOULDER, -23.9f, 40.9f, 15.1f, 3.6f, "L1", H_ON, NULL },
	{ IN_L2,     K_SHOULDER, -12.6f, 40.9f,  7.4f, 3.6f, "L2", H_ON, NULL },
	{ IN_R2,     K_SHOULDER,  12.6f, 40.9f,  7.4f, 3.6f, "R2", H_ON, NULL },
	{ IN_R1,     K_SHOULDER,  23.9f, 40.9f, 15.1f, 3.6f, "R1", H_ON, NULL },
	{ IN_VOLUP,  K_SIDE, -31.8f, 15.75f, 3.0f, 8.0f, "+", H_RIGHT, NULL },
	{ IN_VOLDN,  K_SIDE, -31.8f,  7.65f, 3.0f, 8.0f, "-", H_RIGHT, NULL },
	{ IN_MENU,   K_SIDE,  31.8f, 17.1f,  3.0f, 5.3f, "", H_LEFT, NULL },
	{ IN_POWER,  K_SIDE,  31.8f,  6.65f, 3.0f, 7.2f, "", H_LEFT, NULL },
};
static const flabel pixel2_labels[] = {
	{ "VOL", -34.6f, 11.7f, true }, { "MENU", 34.6f, 17.1f, true }, { "POWER", 34.6f, 6.65f, true },
};

static const fkey brick_keys[] = {
	{ IN_UP,     K_DPAD, 0, 0, 0, 0, "", H_ABOVE, NULL },
	{ IN_DOWN,   K_DPAD, 0, 0, 0, 0, "", H_BELOW, NULL },
	{ IN_LEFT,   K_DPAD, 0, 0, 0, 0, "", H_LEFT,  NULL },
	{ IN_RIGHT,  K_DPAD, 0, 0, 0, 0, "", H_RIGHT, NULL },
	{ IN_X,      K_ROUND, 19.7f,  -9.55f, 7.7f, 7.7f, "X", H_ABOVE, NULL },
	{ IN_Y,      K_ROUND, 12.1f, -17.15f, 7.7f, 7.7f, "Y", H_LEFT,  NULL },
	{ IN_ACCEPT, K_ROUND, 27.3f, -17.15f, 7.7f, 7.7f, "A", H_RIGHT, NULL },
	{ IN_BACK,   K_ROUND, 19.7f, -24.75f, 7.7f, 7.7f, "B", H_BELOW, NULL },
	{ IN_BRIGHTDN, K_PILL, -5.7f, -8.65f, 7.1f, 2.6f, "", H_ABOVE, "F1" },
	{ IN_BRIGHTUP, K_PILL,  5.0f, -8.65f, 7.1f, 2.6f, "", H_ABOVE, "F2" },
	{ IN_MENU,   K_ROUND, -10.5f, -46.45f, 5.4f, 5.4f, "MENU", H_ABOVE, NULL },
	{ IN_SELECT, K_ROUND,  -0.5f, -46.45f, 5.4f, 5.4f, "\xE2\x96\xA0", H_ABOVE, "SELECT" },
	{ IN_START,  K_ROUND,   9.6f, -46.45f, 5.4f, 5.4f, "\xE2\x96\xB6", H_ABOVE, "START" },
	{ IN_L1,     K_SHOULDER, -27.85f, 55.6f, 17.5f, 3.6f, "L1", H_ON, NULL },
	{ IN_L2,     K_SHOULDER, -12.3f,  55.6f, 11.0f, 3.6f, "L2", H_ON, NULL },
	{ IN_R2,     K_SHOULDER,  12.3f,  55.6f, 11.0f, 3.6f, "R2", H_ON, NULL },
	{ IN_R1,     K_SHOULDER,  27.85f, 55.6f, 17.5f, 3.6f, "R1", H_ON, NULL },
	{ IN_VOLUP,  K_SIDE, -37.4f, 44.65f, 3.0f, 7.7f, "+", H_RIGHT, NULL },
	{ IN_VOLDN,  K_SIDE, -37.4f, 33.55f, 3.0f, 7.7f, "-", H_RIGHT, NULL },
	{ IN_POWER,  K_SIDE,  37.4f, 15.65f, 3.0f, 7.1f, "", H_LEFT, NULL },
};
static const flabel brick_labels[] = {
	{ "VOL", -40.2f, 39.1f, true }, { "POWER", 40.2f, 15.65f, true },
};

static const face_def faces[] = {
	{
		.id = "pixel2", .body_w = 63.0f, .body_h = 81.0f, .corner = 3.9f,
		.margin_x = 6.0f, .margin_top = 4.0f, .margin_bottom = 2.5f,
		.glass = { 0, 14.2f, 55.1f, 43.8f }, .glass_r = 1.2f,
		.screen = { 0, 15.55f, 48.8f, 36.6f }, .screen_mm = 48.8f,
		.word_y = -5.5f,
		.recess = { 0.1f, -29.6f, 16.4f, 7.6f }, .recess_r = 3.8f,
		.dpad_x = -16.9f, .dpad_y = -21.0f, .dpad_span = 9.0f, .dpad_arm = 3.2f,
		.led_x = 32.0f, .led_w = 1.3f, .led_h = 3.4f, .led_y = { 1.4f, -4.03f, -9.67f, -15.3f },
		.body_hi = 0xB4CC6C, .body = 0x9CB849, .body_lo = 0x83A03A, .recess_c = 0x708B32,
		.cap = 0x3A3C3F, .cap_down = 0x1E2021, .marking = 0xA4A7AA, .glass_c = 0x0B0E0E,
		.led_off = 0x56723A,
		.keys = pixel2_keys, .n_keys = (int)(sizeof pixel2_keys / sizeof pixel2_keys[0]),
		.labels = pixel2_labels, .n_labels = (int)(sizeof pixel2_labels / sizeof pixel2_labels[0]),
	},
	{
		.id = "brick", .body_w = 73.2f, .body_h = 109.9f, .corner = 5.5f,
		.margin_x = 6.0f, .margin_top = 4.0f, .margin_bottom = 2.5f,
		.glass = { 0, 25.4f, 69.6f, 55.6f }, .glass_r = 1.6f,
		.screen = { 0, 26.4f, 65.0f, 48.8f }, .screen_mm = 65.0f,
		.word_y = -0.3f,
		.recess = { 0, 0, 0, 0 },
		.dpad_x = -20.7f, .dpad_y = -17.35f, .dpad_span = 9.65f, .dpad_arm = 3.5f,
		.body_hi = 0x4B4F56, .body = 0x3B3E44, .body_lo = 0x2C2E33, .recess_c = 0x26282C,
		.cap = 0x232427, .cap_down = 0x121315, .marking = 0xA2A6AC, .glass_c = 0x07080A,
		.keys = brick_keys, .n_keys = (int)(sizeof brick_keys / sizeof brick_keys[0]),
		.labels = brick_labels, .n_labels = (int)(sizeof brick_labels / sizeof brick_labels[0]),
	},
};

static const face_def *F;

bool face_init(const char *device_id)
{
	F = NULL;
	for (size_t i = 0; i < sizeof faces / sizeof faces[0]; i++)
		if (device_id && !strcmp(device_id, faces[i].id)) F = &faces[i];
	return F != NULL;
}

float face_screen_mm(void) { return F ? F->screen_mm : 1; }

static float face_w_mm(void) { return F->body_w + 2 * F->margin_x; }
static float face_h_mm(void) { return F->body_h + F->margin_top + F->margin_bottom; }

/* Millimetres to the window's points */
static float px_mm(float x, float s) { return (x + face_w_mm() / 2) * s; }
static float py_mm(float y, float s) { return (F->margin_top + F->body_h / 2 - y) * s; }

static SDL_FRect box_mm(float cx, float cy, float w, float h, float s)
{
	return (SDL_FRect){ px_mm(cx - w / 2, s), py_mm(cy + h / 2, s), w * s, h * s };
}

void face_size(float s, float *w, float *h, SDL_FRect *screen)
{
	*w = face_w_mm() * s;
	*h = face_h_mm() * s;
	*screen = box_mm(F->screen.x, F->screen.y, F->screen.w, F->screen.h, s);
}

/* ---- the vector kit: shapes as SDL geometry, in the renderer's pixels -------- */

static SDL_Renderer *R;
static float         RT = 1;        /* pixels a point */

static SDL_Color rgb(Uint32 c, Uint8 a)
{
	return (SDL_Color){ (Uint8)(c >> 16), (Uint8)(c >> 8), (Uint8)c, a };
}

static SDL_Color shade(Uint32 c, int d, Uint8 a)
{
	int r = (int)(c >> 16 & 255) + d, g = (int)(c >> 8 & 255) + d, b = (int)(c & 255) + d;
	r = r < 0 ? 0 : r > 255 ? 255 : r;
	g = g < 0 ? 0 : g > 255 ? 255 : g;
	b = b < 0 ? 0 : b > 255 ? 255 : b;
	return (SDL_Color){ (Uint8)r, (Uint8)g, (Uint8)b, a };
}

static SDL_Color mix(SDL_Color a, SDL_Color b, float k)
{
	if (k < 0) k = 0;
	if (k > 1) k = 1;
	return (SDL_Color){ (Uint8)(a.r + (b.r - a.r) * k), (Uint8)(a.g + (b.g - a.g) * k),
	                    (Uint8)(a.b + (b.b - a.b) * k), (Uint8)(a.a + (b.a - a.a) * k) };
}

#define OUTLINE_MAX 256

/* A rounded rectangle's outline in pixels, clockwise from the top left
 * corner's start, each point with its outward normal; radii top left, top
 * right, bottom right, bottom left */
static int outline(SDL_FRect b, const float rad[4], SDL_FPoint *p, SDL_FPoint *n)
{
	const float cx[4] = { b.x + rad[0], b.x + b.w - rad[1], b.x + b.w - rad[2], b.x + rad[3] };
	const float cy[4] = { b.y + rad[0], b.y + rad[1], b.y + b.h - rad[2], b.y + b.h - rad[3] };
	const float a0[4] = { (float)M_PI, (float)(1.5 * M_PI), 0, (float)(0.5 * M_PI) };
	int np = 0;

	for (int c = 0; c < 4; c++) {
		float r = rad[c];
		int seg = r <= 0.5f ? 1 : (int)(r / 1.5f) + 3;

		if (seg > OUTLINE_MAX / 4 - 1) seg = OUTLINE_MAX / 4 - 1;
		for (int i = 0; i <= seg && r > 0.5f; i++) {
			float t = a0[c] + (float)(0.5 * M_PI) * i / seg;
			n[np] = (SDL_FPoint){ cosf(t), sinf(t) };
			p[np] = (SDL_FPoint){ cx[c] + r * n[np].x, cy[c] + r * n[np].y };
			np++;
		}
		if (r <= 0.5f) {
			/* A square corner: the corner itself, its normal the diagonal */
			static const float dx[4] = { -1, 1, 1, -1 }, dy[4] = { -1, -1, 1, 1 };
			n[np] = (SDL_FPoint){ dx[c] * 0.7071f, dy[c] * 0.7071f };
			p[np] = (SDL_FPoint){ cx[c], cy[c] };
			np++;
		}
	}
	return np;
}

static SDL_FRect to_px(SDL_FRect b)
{
	return (SDL_FRect){ b.x * RT, b.y * RT, b.w * RT, b.h * RT };
}

/* Filled, a vertical gradient from `top` at gy0 to `bot` at gy1 (pixels),
 * its edge smoothed over one pixel */
static void fill_px(SDL_FRect b, const float rad[4], SDL_Color top, SDL_Color bot, float gy0, float gy1)
{
	SDL_FPoint p[OUTLINE_MAX], n[OUTLINE_MAX];
	SDL_Vertex v[1 + 2 * OUTLINE_MAX];
	int idx[9 * OUTLINE_MAX], ni = 0;
	int np = outline(b, rad, p, n);
	float span = gy1 - gy0 > 0.5f ? gy1 - gy0 : 0.5f;

	v[0] = (SDL_Vertex){ { b.x + b.w / 2, b.y + b.h / 2 }, mix(top, bot, (b.y + b.h / 2 - gy0) / span), { 0, 0 } };
	for (int i = 0; i < np; i++) {
		SDL_Color c = mix(top, bot, (p[i].y - gy0) / span);
		SDL_Color clear = c;

		clear.a = 0;
		v[1 + i] = (SDL_Vertex){ { p[i].x - n[i].x * 0.5f, p[i].y - n[i].y * 0.5f }, c, { 0, 0 } };
		v[1 + np + i] = (SDL_Vertex){ { p[i].x + n[i].x * 0.5f, p[i].y + n[i].y * 0.5f }, clear, { 0, 0 } };
	}
	for (int i = 0; i < np; i++) {
		int j = (i + 1) % np;
		idx[ni++] = 0; idx[ni++] = 1 + i; idx[ni++] = 1 + j;
		idx[ni++] = 1 + i; idx[ni++] = 1 + np + i; idx[ni++] = 1 + j;
		idx[ni++] = 1 + j; idx[ni++] = 1 + np + i; idx[ni++] = 1 + np + j;
	}
	SDL_RenderGeometry(R, NULL, v, 1 + 2 * np, idx, ni);
}

/* A band along the outline from d0 to d1 pixels out (in, negative), its
 * colour from c0 to c1 across it; c0's alpha follows the vertical gradient
 * from a_top to a_bot of c0's own */
static void band_px(SDL_FRect b, const float rad[4], float d0, float d1, SDL_Color c0, SDL_Color c1,
                    float a_top, float a_bot)
{
	SDL_FPoint p[OUTLINE_MAX], n[OUTLINE_MAX];
	SDL_Vertex v[2 * OUTLINE_MAX];
	int idx[6 * OUTLINE_MAX], ni = 0;
	int np = outline(b, rad, p, n);

	for (int i = 0; i < np; i++) {
		float k = b.h > 0 ? (p[i].y - b.y) / b.h : 0;
		SDL_Color a = c0, z = c1;

		a.a = (Uint8)(c0.a * (a_top + (a_bot - a_top) * k));
		z.a = (Uint8)(c1.a * (a_top + (a_bot - a_top) * k));
		v[i] = (SDL_Vertex){ { p[i].x + n[i].x * d0, p[i].y + n[i].y * d0 }, a, { 0, 0 } };
		v[np + i] = (SDL_Vertex){ { p[i].x + n[i].x * d1, p[i].y + n[i].y * d1 }, z, { 0, 0 } };
	}
	for (int i = 0; i < np; i++) {
		int j = (i + 1) % np;
		idx[ni++] = i; idx[ni++] = np + i; idx[ni++] = j;
		idx[ni++] = j; idx[ni++] = np + i; idx[ni++] = np + j;
	}
	SDL_RenderGeometry(R, NULL, v, 2 * np, idx, ni);
}

static void radii(float r, float out[4]) { out[0] = out[1] = out[2] = out[3] = r; }

/* A shadow under a shape: dark at its edge, gone `blur` pixels out */
static void shadow_px(SDL_FRect b, const float rad[4], float blur, float dy, Uint8 alpha)
{
	SDL_Color c = { 0, 0, 0, alpha }, z = { 0, 0, 0, 0 };

	b.y += dy;
	fill_px(b, rad, c, c, b.y, b.y + b.h);
	band_px(b, rad, 0, blur, c, z, 1, 1);
}

void face_rrect(SDL_Renderer *r, SDL_FRect box, float radius, SDL_Color a, SDL_Color b, float ratio)
{
	float rad[4];

	R = r;
	RT = ratio;
	box = to_px(box);
	radii(radius * ratio, rad);
	fill_px(box, rad, a, b, box.y, box.y + box.h);
}

void face_shadow(SDL_Renderer *r, SDL_FRect box, float radius, float blur, float dy, Uint8 alpha, float ratio)
{
	float rad[4];

	R = r;
	RT = ratio;
	box = to_px(box);
	radii(radius * ratio, rad);
	shadow_px(box, rad, blur * ratio, dy * ratio, alpha);
}

/* ---- text ---------------------------------------------------------------------- */

typedef struct { char text[64]; int px; bool bold; SDL_Color c; SDL_Texture *t; int w, h; } text_tex;
static text_tex  g_text[128];
static int       g_ntext;
static TTF_Font *g_font[6];
static int       g_font_px[6];

static TTF_Font *font_px(int px, bool bold)
{
	int slot = (bold ? 3 : 0) + (px <= 16 ? 0 : px <= 28 ? 1 : 2);

	if (!TTF_WasInit()) TTF_Init();
	if (!g_font[slot] || g_font_px[slot] != px) {
		if (g_font[slot]) TTF_CloseFont(g_font[slot]);
		g_font[slot] = TTF_OpenFont(P_FONT, px);
		if (g_font[slot] && bold) TTF_SetFontStyle(g_font[slot], TTF_STYLE_BOLD);
		g_font_px[slot] = px;
	}
	return g_font[slot];
}

static text_tex *text_get(SDL_Renderer *r, const char *s, int px, SDL_Color c, bool bold)
{
	SDL_Surface *surf;
	TTF_Font *f;
	text_tex *t;

	for (int i = 0; i < g_ntext; i++)
		if (g_text[i].px == px && g_text[i].bold == bold && !memcmp(&g_text[i].c, &c, sizeof c) &&
		    !strcmp(g_text[i].text, s))
			return &g_text[i];
	if (g_ntext >= (int)(sizeof g_text / sizeof g_text[0])) face_forget();
	if (!*s || !(f = font_px(px, bold)) || !(surf = TTF_RenderUTF8_Blended(f, s, c))) return NULL;
	t = &g_text[g_ntext];
	snprintf(t->text, sizeof t->text, "%s", s);
	t->px = px;
	t->bold = bold;
	t->c = c;
	t->w = surf->w;
	t->h = surf->h;
	t->t = SDL_CreateTextureFromSurface(r, surf);
	SDL_FreeSurface(surf);
	if (!t->t) return NULL;
	g_ntext++;
	return t;
}

void face_forget(void)
{
	for (int i = 0; i < g_ntext; i++) SDL_DestroyTexture(g_text[i].t);
	g_ntext = 0;
	for (int i = 0; i < 6; i++)
		if (g_font[i]) { TTF_CloseFont(g_font[i]); g_font[i] = NULL; }
}

static void text_px(SDL_Renderer *r, const char *s, float px, SDL_Color c, float cx, float cy, bool bold, double angle)
{
	text_tex *t = text_get(r, s, (int)(px + 0.5f), c, bold);

	if (!t) return;
	SDL_FRect d = { cx - t->w / 2.0f, cy - t->h / 2.0f, (float)t->w, (float)t->h };
	SDL_RenderCopyExF(r, t->t, NULL, &d, angle, NULL, SDL_FLIP_NONE);
}

void face_text(SDL_Renderer *r, const char *s, float pt, SDL_Color c, float cx, float cy, float ratio, bool bold)
{
	text_px(r, s, pt * ratio, c, cx * ratio, cy * ratio, bold, 0);
}

float face_text_w(const char *s, float pt, float ratio, bool bold)
{
	int w = 0, h = 0;
	TTF_Font *f = font_px((int)(pt * ratio + 0.5f), bold);

	if (f) TTF_SizeUTF8(f, s, &w, &h);
	return ratio > 0 ? w / ratio : (float)w;
}

/* ---- the parts -------------------------------------------------------------------- */

static void key_radii(const fkey *k, SDL_FRect b, float s, float rad[4])
{
	switch (k->kind) {
	case K_SHOULDER:
		rad[0] = rad[1] = 1.2f * s * RT;
		rad[2] = rad[3] = 0;
		break;
	case K_SIDE:
		radii(fminf(b.w, b.h) / 2, rad);
		break;
	default:
		radii(fminf(b.w, b.h) / 2, rad);
	}
}

/* A key's cap: a soft shadow under it, its face lit from above, a bright
 * rim along its top; pressed, it sits lower, darker, with no shadow */
static void cap_px(SDL_FRect b, const float rad[4], float s, bool down)
{
	float lift = 0.14f * s * RT;
	SDL_Color hi = { 255, 255, 255, 46 }, clear = { 255, 255, 255, 0 };

	if (!down) {
		shadow_px(b, rad, 0.55f * s * RT, 0.28f * s * RT, 115);
		fill_px(b, rad, shade(F->cap, 22, 255), shade(F->cap, -6, 255), b.y, b.y + b.h);
		band_px(b, rad, -0.5f, -0.35f * s * RT, hi, clear, 1, 0);
	} else {
		b.y += lift;
		fill_px(b, rad, shade(F->cap_down, -4, 255), shade(F->cap_down, 8, 255), b.y, b.y + b.h);
		band_px(b, rad, 0, -0.45f * s * RT, (SDL_Color){ 0, 0, 0, 120 }, (SDL_Color){ 0, 0, 0, 0 }, 1, 0.3f);
	}
}

static SDL_FRect key_box(const fkey *k, float s)
{
	return box_mm(k->x, k->y, k->w, k->h, s);
}

/* The d-pad's two bars and where a press of each way goes */
static SDL_FRect dpad_bar(bool across, float s)
{
	float sp = F->dpad_span, arm = F->dpad_arm;

	return across ? box_mm(F->dpad_x, F->dpad_y, 2 * sp, 2 * arm, s)
	              : box_mm(F->dpad_x, F->dpad_y, 2 * arm, 2 * sp, s);
}

static void triangle_px(float cx, float cy, float size, int way, SDL_Color c)
{
	SDL_Vertex v[3];
	float a = size, b = size * 0.62f;
	SDL_FPoint p[3];

	switch (way) {
	case 0:  p[0] = (SDL_FPoint){ cx, cy - a }; p[1] = (SDL_FPoint){ cx - b, cy + b * 0.7f }; p[2] = (SDL_FPoint){ cx + b, cy + b * 0.7f }; break;
	case 1:  p[0] = (SDL_FPoint){ cx, cy + a }; p[1] = (SDL_FPoint){ cx + b, cy - b * 0.7f }; p[2] = (SDL_FPoint){ cx - b, cy - b * 0.7f }; break;
	case 2:  p[0] = (SDL_FPoint){ cx - a, cy }; p[1] = (SDL_FPoint){ cx + b * 0.7f, cy + b }; p[2] = (SDL_FPoint){ cx + b * 0.7f, cy - b }; break;
	default: p[0] = (SDL_FPoint){ cx + a, cy }; p[1] = (SDL_FPoint){ cx - b * 0.7f, cy - b }; p[2] = (SDL_FPoint){ cx - b * 0.7f, cy + b }; break;
	}
	for (int i = 0; i < 3; i++) v[i] = (SDL_Vertex){ p[i], c, { 0, 0 } };
	SDL_RenderGeometry(R, NULL, v, 3, NULL, 0);
}

static void dpad_px(float s, Uint32 held)
{
	SDL_FRect h = to_px(dpad_bar(true, s)), v = to_px(dpad_bar(false, s));
	float rad[4], top = v.y, bot = v.y + v.h;
	SDL_Color hi = { 255, 255, 255, 40 }, clear = { 255, 255, 255, 0 };
	static const in_button ways[4] = { IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT };
	float arm = F->dpad_arm * s * RT;

	radii(0.7f * s * RT, rad);
	/* Its well, then the cross standing in it */
	{
		SDL_FRect wh = h, wv = v;
		float g = 0.45f * s * RT, wrad[4];

		wh.x -= g; wh.y -= g; wh.w += 2 * g; wh.h += 2 * g;
		wv.x -= g; wv.y -= g; wv.w += 2 * g; wv.h += 2 * g;
		radii(1.0f * s * RT, wrad);
		fill_px(wh, wrad, shade(F->body_lo, -26, 255), shade(F->body_lo, -14, 255), wv.y, wv.y + wv.h);
		fill_px(wv, wrad, shade(F->body_lo, -26, 255), shade(F->body_lo, -14, 255), wv.y, wv.y + wv.h);
	}
	shadow_px(h, rad, 0.5f * s * RT, 0.28f * s * RT, 100);
	shadow_px(v, rad, 0.5f * s * RT, 0.28f * s * RT, 100);
	fill_px(h, rad, shade(F->cap, 22, 255), shade(F->cap, -6, 255), top, bot);
	fill_px(v, rad, shade(F->cap, 22, 255), shade(F->cap, -6, 255), top, bot);
	band_px(v, rad, -0.5f, -0.3f * s * RT, hi, clear, 1, 0);
	/* A way held: its arm darker, as a thumb tips the pad */
	for (int i = 0; i < 4; i++) {
		SDL_FRect a;
		float r0[4];

		if (!(held >> ways[i] & 1)) continue;
		switch (i) {
		case 0:  a = (SDL_FRect){ v.x, v.y, v.w, v.h / 2 - arm }; break;
		case 1:  a = (SDL_FRect){ v.x, v.y + v.h / 2 + arm, v.w, v.h / 2 - arm }; break;
		case 2:  a = (SDL_FRect){ h.x, h.y, h.w / 2 - arm, h.h }; break;
		default: a = (SDL_FRect){ h.x + h.w / 2 + arm, h.y, h.w / 2 - arm, h.h }; break;
		}
		radii(0.7f * s * RT, r0);
		fill_px(a, r0, (SDL_Color){ 0, 0, 0, 120 }, (SDL_Color){ 0, 0, 0, 90 }, a.y, a.y + a.h);
	}
	/* The centre's dimple, and the four arrows */
	{
		float cx = h.x + h.w / 2, cy = v.y + v.h / 2, d = arm * 0.55f;
		SDL_FRect dim = { cx - d, cy - d, 2 * d, 2 * d };
		float dr[4];
		SDL_Color mark = rgb(F->marking, 180);

		radii(d, dr);
		fill_px(dim, dr, shade(F->cap, -10, 255), shade(F->cap, 14, 255), dim.y, dim.y + dim.h);
		float off = (F->dpad_span - F->dpad_arm * 0.9f) * s * RT, sz = arm * 0.42f;
		triangle_px(cx, cy - off, sz, 0, held >> IN_UP & 1 ? (SDL_Color){ 255, 255, 255, 220 } : mark);
		triangle_px(cx, cy + off, sz, 1, held >> IN_DOWN & 1 ? (SDL_Color){ 255, 255, 255, 220 } : mark);
		triangle_px(cx - off, cy, sz, 2, held >> IN_LEFT & 1 ? (SDL_Color){ 255, 255, 255, 220 } : mark);
		triangle_px(cx + off, cy, sz, 3, held >> IN_RIGHT & 1 ? (SDL_Color){ 255, 255, 255, 220 } : mark);
	}
}

static void key_px(const fkey *k, float s, Uint32 held)
{
	SDL_FRect b = to_px(key_box(k, s));
	bool down = held >> k->id & 1;
	float rad[4];

	key_radii(k, b, s, rad);
	cap_px(b, rad, s, down);
	if (*k->label) {
		float n = (float)strlen(k->label);
		float size = k->kind == K_ROUND ? (n == 1 ? 0.46f : n == 2 ? 0.34f : 0.2f) * k->w
		           : k->kind == K_SIDE ? 0.6f * k->w : 0.36f * k->h;
		float dx = 0, dy = down ? 0.14f * s * RT : 0;
		float cx, cy;
		SDL_Color m = rgb(F->marking, 255);

		/* The shoulders' names on their part above the body; a side key's
		 * on its part outside the body's edge */
		if (k->kind == K_SHOULDER) dy -= 0.35f * k->h * s * RT;
		if (k->kind == K_SIDE) dx = (k->x < 0 ? -0.25f : 0.25f) * k->w * s * RT;
		cx = b.x + b.w / 2 + dx;
		cy = b.y + b.h / 2 + dy;
		if (!strcmp(k->label, "\xE2\x96\xA0")) {          /* SELECT's square */
			float h = 0.17f * k->w * s * RT, r0[4];
			radii(0.2f * h, r0);
			fill_px((SDL_FRect){ cx - h, cy - h, 2 * h, 2 * h }, r0, m, m, cy - h, cy + h);
		} else if (!strcmp(k->label, "\xE2\x96\xB6")) {   /* START's triangle */
			triangle_px(cx + 0.03f * k->w * s * RT, cy, 0.2f * k->w * s * RT, 3, m);
		} else {
			text_px(R, k->label, size * s * RT, m, cx, cy, true, 0);
		}
	}
	if (k->under) {
		text_px(R, k->under, 1.25f * s * RT, rgb(F->marking, 220), b.x + b.w / 2,
		        b.y + b.h + 1.6f * s * RT, true, 0);
	}
}

void face_draw(SDL_Renderer *r, float s, float ratio, Uint32 held)
{
	float rad[4];
	SDL_FRect body, glass;

	if (!F) return;
	R = r;
	RT = ratio;
	SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
	/* What stands out of the body's edges goes first, behind it */
	for (int i = 0; i < F->n_keys; i++)
		if (F->keys[i].kind == K_SHOULDER || F->keys[i].kind == K_SIDE) key_px(&F->keys[i], s, held);

	/* The body: a soft shadow, lit from above, a bright rim along its top
	 * and a dark one along its bottom */
	body = to_px(box_mm(0, 0, F->body_w, F->body_h, s));
	radii(F->corner * s * RT, rad);
	shadow_px(body, rad, 2.6f * s * RT, 1.1f * s * RT, 70);
	fill_px(body, rad, rgb(F->body_hi, 255), rgb(F->body_lo, 255), body.y, body.y + body.h);
	band_px(body, rad, -0.4f, -0.7f * s * RT, (SDL_Color){ 255, 255, 255, 90 }, (SDL_Color){ 255, 255, 255, 0 }, 1, 0.15f);
	band_px(body, rad, 0.5f, -0.25f * s * RT, (SDL_Color){ 0, 0, 0, 60 }, (SDL_Color){ 0, 0, 0, 0 }, 0.2f, 1);

	/* The glass, its edge in shadow, a hairline of light along its top */
	glass = to_px(box_mm(F->glass.x, F->glass.y, F->glass.w, F->glass.h, s));
	radii(F->glass_r * s * RT, rad);
	band_px(glass, rad, 0, 0.35f * s * RT, (SDL_Color){ 0, 0, 0, 70 }, (SDL_Color){ 0, 0, 0, 0 }, 1, 1);
	fill_px(glass, rad, shade(F->glass_c, 14, 255), rgb(F->glass_c, 255), glass.y, glass.y + glass.h);
	band_px(glass, rad, -0.5f, -0.25f * s * RT, (SDL_Color){ 255, 255, 255, 34 }, (SDL_Color){ 255, 255, 255, 0 }, 1, 0);
	if (!isnan(F->word_y))
		text_px(r, "ZyperOS", 1.9f * s * RT, (SDL_Color){ 255, 255, 255, 205 }, glass.x + glass.w / 2,
		        py_mm(F->word_y, s) * RT, true, 0);

	/* SELECT and START's well */
	if (F->recess.w > 0) {
		SDL_FRect w = to_px(box_mm(F->recess.x, F->recess.y, F->recess.w, F->recess.h, s));
		radii(F->recess_r * s * RT, rad);
		fill_px(w, rad, shade(F->recess_c, -10, 255), shade(F->recess_c, 6, 255), w.y, w.y + w.h);
		band_px(w, rad, 0, -0.6f * s * RT, (SDL_Color){ 0, 0, 0, 70 }, (SDL_Color){ 0, 0, 0, 0 }, 1, 0.2f);
		band_px(w, rad, 0.5f, 0.1f * s * RT, (SDL_Color){ 255, 255, 255, 40 }, (SDL_Color){ 255, 255, 255, 0 }, 0, 1);
	}

	dpad_px(s, held);
	for (int i = 0; i < F->n_keys; i++)
		if (F->keys[i].kind == K_ROUND || F->keys[i].kind == K_PILL) key_px(&F->keys[i], s, held);

	/* The side lights, unlit: the host has no gauge to light them */
	for (int i = 0; F->led_w > 0 && i < 4; i++) {
		SDL_FRect l = to_px(box_mm(F->led_x, F->led_y[i], F->led_w, F->led_h, s));
		radii(l.w / 2, rad);
		fill_px(l, rad, shade(F->led_off, -14, 255), shade(F->led_off, 10, 255), l.y, l.y + l.h);
		band_px(l, rad, 0, 0.12f * s * RT, (SDL_Color){ 0, 0, 0, 90 }, (SDL_Color){ 0, 0, 0, 0 }, 1, 1);
	}
	/* The side keys' names, turned along the edges */
	for (int i = 0; i < F->n_labels; i++) {
		const flabel *l = &F->labels[i];
		text_px(r, l->text, 1.3f * s * RT, (SDL_Color){ 120, 122, 128, 255 }, px_mm(l->x, s) * RT,
		        py_mm(l->y, s) * RT, true, l->turned ? -90 : 0);
	}
}

/* ---- the keys' names ------------------------------------------------------------------ */

#include "keymap.h"

static void hint_px(const fkey *k, float s, const char *name)
{
	SDL_FRect b;
	float pt = 10, w = face_text_w(name, pt, RT, true) + 10, h = 17, x, y, gap = 5;
	float rad[4];

	if (k->kind == K_DPAD) {
		SDL_FRect bar = dpad_bar(k->id == IN_LEFT || k->id == IN_RIGHT, s);
		float cx = bar.x + bar.w / 2, cy = bar.y + bar.h / 2;
		b = k->id == IN_UP ? (SDL_FRect){ cx - 1, bar.y, 2, 2 }
		  : k->id == IN_DOWN ? (SDL_FRect){ cx - 1, bar.y + bar.h - 2, 2, 2 }
		  : k->id == IN_LEFT ? (SDL_FRect){ bar.x, cy - 1, 2, 2 }
		  : (SDL_FRect){ bar.x + bar.w - 2, cy - 1, 2, 2 };
	} else {
		b = key_box(k, s);
	}
	switch (k->hint) {
	case H_ON:    x = b.x + b.w / 2 - w / 2; y = b.y + b.h * 0.3f - h / 2; break;
	case H_RIGHT: x = b.x + b.w + gap; y = b.y + b.h / 2 - h / 2; break;
	case H_LEFT:  x = b.x - gap - w;   y = b.y + b.h / 2 - h / 2; break;
	case H_ABOVE: x = b.x + b.w / 2 - w / 2; y = b.y - gap - h; break;
	default:      x = b.x + b.w / 2 - w / 2; y = b.y + b.h + gap; break;
	}
	/* A keycap: dark, a hairline rim, its name in white */
	SDL_FRect chip = to_px((SDL_FRect){ x, y, w, h });
	radii(4 * RT, rad);
	shadow_px(chip, rad, 3 * RT, 1 * RT, 60);
	fill_px(chip, rad, (SDL_Color){ 38, 40, 46, 240 }, (SDL_Color){ 22, 23, 27, 240 }, chip.y, chip.y + chip.h);
	band_px(chip, rad, -0.5f, -1.2f * RT, (SDL_Color){ 255, 255, 255, 60 }, (SDL_Color){ 255, 255, 255, 0 }, 1, 0.2f);
	text_px(R, name, pt * RT, (SDL_Color){ 245, 245, 247, 255 }, chip.x + chip.w / 2, chip.y + chip.h / 2, true, 0);
}

void face_draw_over(SDL_Renderer *r, float s, float ratio, in_button hover, bool all)
{
	if (!F) return;
	R = r;
	RT = ratio;
	for (int i = 0; i < F->n_keys; i++) {
		const fkey *k = &F->keys[i];
		const char *name = keymap_hint(k->id);

		if (k->kind == K_DPAD) name = k->id == IN_UP ? "Up" : k->id == IN_DOWN ? "Down"
		                            : k->id == IN_LEFT ? "Left" : "Right";
		if (!*name || (!all && k->id != hover)) continue;
		hint_px(k, s, name);
	}
}

in_button face_hit(float s, float x, float y)
{
	if (!F) return IN_NONE;
	/* The d-pad: the way the point is furthest along, in the cross */
	{
		SDL_FRect h = dpad_bar(true, s), v = dpad_bar(false, s);
		bool in = (x >= h.x && x <= h.x + h.w && y >= h.y && y <= h.y + h.h) ||
		          (x >= v.x && x <= v.x + v.w && y >= v.y && y <= v.y + v.h);
		if (in) {
			float dx = x - (h.x + h.w / 2), dy = y - (v.y + v.h / 2);
			return fabsf(dx) > fabsf(dy) ? (dx > 0 ? IN_RIGHT : IN_LEFT) : (dy > 0 ? IN_DOWN : IN_UP);
		}
	}
	for (int i = F->n_keys - 1; i >= 0; i--) {
		const fkey *k = &F->keys[i];
		SDL_FRect b;
		float slop = 1.5f;

		if (k->kind == K_DPAD) continue;
		b = key_box(k, s);
		if (x >= b.x - slop && x <= b.x + b.w + slop && y >= b.y - slop && y <= b.y + b.h + slop) return k->id;
	}
	return IN_NONE;
}
