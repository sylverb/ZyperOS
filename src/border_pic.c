/* SPDX-License-Identifier: GPL-3.0-only */
/* The Border's pictures. See src/border_pic.h. */
#include "border_pic.h"

#include <SDL_image.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "db.h"
#include "i18n.h"
#include "dmodepreview.h"
#include "handframe.h"
#include "platform.h"

/* The first PNG by name in `dir`, or false */
static bool first_png(const char *dir, char *out, size_t n)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	char best[256] = "";

	if (!d) return false;
	while ((e = readdir(d))) {
		size_t l = strlen(e->d_name);

		if (e->d_name[0] == '.' || l < 5 || strcasecmp(e->d_name + l - 4, ".png")) continue;
		if (!best[0] || strcmp(e->d_name, best) < 0) snprintf(best, sizeof best, "%s", e->d_name);
	}
	closedir(d);
	if (!best[0]) return false;
	snprintf(out, n, "%s/%s", dir, best);
	return true;
}

bool border_own_path(const system_cfg *s, char *out, size_t n)
{
	char dir[CFG_STR * 2];
	const char *nt = border_nextui_tag(s->tag);

	out[0] = '\0';
	snprintf(dir, sizeof dir, "%s/Borders/%s", P_CARD, s->folder);
	if (first_png(dir, out, n)) return true;
	snprintf(dir, sizeof dir, "%s/Overlays/%s", P_CARD, s->tag);
	if (first_png(dir, out, n)) return true;
	if (nt) {
		snprintf(dir, sizeof dir, "%s/Overlays/%s", P_CARD, nt);
		if (first_png(dir, out, n)) return true;
	}
	out[0] = '\0';
	return false;
}

bool border_blur_here(void)
{
	/* Where Diatom draws every frame with the GPU, as it draws the old
	 * screens' shaders (the profile's gpu_shaders: the Pixel 2, the host, a
	 * Brick test kit's GPU path); a CPU path would paint every page again
	 * ten times a second */
	return plat_gpu_shaders();
}

bool border_room_kept(const app *a, int sys)
{
	const system_cfg *s = &a->sys.systems[sys];
	int sw, sh, fw, fh;
	bool known;
	double aspect;

	dmode_source_of(s->tag, &sw, &sh);
	aspect = dmode_shape_of(s->tag, &known);
	dmode_surface(&fw, &fh);
	return border_room(DMODES[a->view[sys].dmode].name, sw, sh, known ? aspect : 0.0, fw, fh);
}

border_offer border_offer_room(const app *a, int sys, bool room)
{
	const system_cfg *s = &a->sys.systems[sys];
	const hf_model *m = hf_model_for(s->tag);
	char own[CFG_STR * 3];
	int w, h, sc, gx, gy, gw, gh;
	bool frame, whole;

	dmode_surface(&w, &h);
	frame = m && hf_place(m, HF_CUT, w, h, &sc, &gx, &gy, &gw, &gh);
	whole = frame && hf_place(m, HF_WHOLE, w, h, &sc, &gx, &gy, &gw, &gh);
	return border_offer_make(room, border_blur_here(), room && border_own_path(s, own, sizeof own), frame, whole);
}

border_offer border_offer_for(const app *a, int sys)
{
	return border_offer_room(a, sys, border_room_kept(a, sys));
}

border_kind border_load_room(const app *a, int sys, bool room)
{
	const system_cfg *s = &a->sys.systems[sys];
	char key[CFG_STR + 16], v[16];

	border_key(s->tag, key, sizeof key);
	db_get_str(db_dev(), key, v, sizeof v, border_id(BORDER_OFF));
	return border_in_mode(border_shown(border_parse(v), border_offer_room(a, sys, room)),
	                      DMODES[a->view[sys].dmode].name);
}

border_kind border_load(const app *a, int sys)
{
	return border_load_room(a, sys, border_room_kept(a, sys));
}

bool border_choose(app *a, int sys, int d, bool room)
{
	const system_cfg *s = &a->sys.systems[sys];
	const border_kind k = border_step(border_load_room(a, sys, room), d, border_offer_room(a, sys, room));
	char key[CFG_STR + 16];
	int i;

	border_key(s->tag, key, sizeof key);
	db_set_str(db_dev(), key, border_id(k));
	if (!border_is_frame(k) || !strcmp(DMODES[a->view[sys].dmode].name, "integer")) return false;
	for (i = 0; i < DMODE_COUNT; i++)
		if (!strcmp(DMODES[i].name, "integer")) a->view[sys].dmode = i;
	return true;
}

SDL_Surface *border_own_surface(const char *png, int w, int h)
{
	SDL_Surface *raw = png ? IMG_Load(png) : NULL, *src, *dst;
	int cx, cy, cw, ch, x, y;

	if (!raw || w <= 0 || h <= 0) { if (raw) SDL_FreeSurface(raw); return NULL; }
	src = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ARGB8888, 0);
	SDL_FreeSurface(raw);
	if (!src) return NULL;
	dst = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
	if (!dst) { SDL_FreeSurface(src); return NULL; }
	/* The picture's middle in the surface's shape, never stretched out of
	 * its own: a 16:9 overlay made for another handheld loses its sides */
	if ((long)src->w * h > (long)src->h * w) {
		ch = src->h;
		cw = (int)((long)src->h * w / h);
	} else {
		cw = src->w;
		ch = (int)((long)src->w * h / w);
	}
	cx = (src->w - cw) / 2;
	cy = (src->h - ch) / 2;
	/* Bilinear, each channel, alpha made opaque over black (a transparent
	 * window in an overlay is where the game goes, under it here) */
	for (y = 0; y < h; y++) {
		Uint32 *drow = (Uint32 *)((Uint8 *)dst->pixels + (size_t)y * dst->pitch);
		const float fy = cy + ((float)y + 0.5f) * ch / h - 0.5f;
		const int y0 = fy < 0 ? 0 : (int)fy, y1 = y0 + 1 < src->h ? y0 + 1 : src->h - 1;
		const float ty = fy < 0 ? 0 : fy - y0;
		const Uint32 *r0 = (const Uint32 *)((const Uint8 *)src->pixels + (size_t)y0 * src->pitch);
		const Uint32 *r1 = (const Uint32 *)((const Uint8 *)src->pixels + (size_t)y1 * src->pitch);

		for (x = 0; x < w; x++) {
			const float fx = cx + ((float)x + 0.5f) * cw / w - 0.5f;
			const int x0 = fx < 0 ? 0 : (int)fx, x1 = x0 + 1 < src->w ? x0 + 1 : src->w - 1;
			const float tx = fx < 0 ? 0 : fx - x0;
			const Uint32 p[4] = { r0[x0], r0[x1], r1[x0], r1[x1] };
			float c[3] = { 0, 0, 0 };
			int k, sh;

			for (k = 0, sh = 16; k < 3; k++, sh -= 8) {
				/* each texel times its own alpha: straight alpha over black */
				float v[4];
				int q;

				for (q = 0; q < 4; q++)
					v[q] = (float)((p[q] >> sh) & 255) * (float)(p[q] >> 24) / 255.0f;
				c[k] = (v[0] + (v[1] - v[0]) * tx) * (1 - ty) + (v[2] + (v[3] - v[2]) * tx) * ty;
			}
			drow[x] = 0xff000000u | ((Uint32)(c[0] + .5f) << 16) | ((Uint32)(c[1] + .5f) << 8) |
			          (Uint32)(c[2] + .5f);
		}
	}
	SDL_FreeSurface(src);
	return dst;
}

SDL_Surface *border_own_cached(const system_cfg *s, char *path, size_t n)
{
	static SDL_Surface *kept;
	static char of[CFG_STR * 3];
	int w, h;

	if (!border_own_path(s, path, n)) return NULL;
	if (kept && !strcmp(of, path)) return kept;
	if (kept) SDL_FreeSurface(kept);
	dmode_surface(&w, &h);
	kept = border_own_surface(path, w, h);
	snprintf(of, sizeof of, "%s", kept ? path : "");
	return kept;
}

const char *border_color_label(const char *word)
{
	static const struct { const char *word; str_id id; } NAMES[] = {
		{ "yellow", S_GAMEMENU_COLOR_YELLOW }, { "red", S_GAMEMENU_COLOR_RED },
		{ "purple", S_GAMEMENU_COLOR_PURPLE }, { "green", S_GAMEMENU_COLOR_GREEN },
		{ "teal", S_GAMEMENU_COLOR_TEAL }, { "clear_purple", S_GAMEMENU_COLOR_CLEAR_PURPLE },
	};
	size_t i;

	for (i = 0; word && i < sizeof NAMES / sizeof NAMES[0]; i++)
		if (!strcmp(word, NAMES[i].word)) return T(NAMES[i].id);
	return word ? word : "";
}

const char *border_color_value(const app *a, int sys)
{
	const hf_model *m = hf_model_for(a->sys.systems[sys].tag);
	border_kind k = border_load(a, sys);

	if (!m || m->ncolors < 2 || (k != BORDER_HANDHELD && k != BORDER_WHOLE)) return NULL;
	return border_color_label(m->color_name[border_color_load(a, sys)]);
}

void border_color_step(const app *a, int sys, int d)
{
	const hf_model *m = hf_model_for(a->sys.systems[sys].tag);
	char key[CFG_STR + 16];
	int c;

	if (!m || m->ncolors < 2) return;
	c = ((border_color_load(a, sys) + (d < 0 ? -1 : 1)) % m->ncolors + m->ncolors) % m->ncolors;
	snprintf(key, sizeof key, "border_color.%s", a->sys.systems[sys].tag);
	db_set_int(db_dev(), key, c);
}

border_kind border_color_kind(const app *a, int sys)
{
	return border_offer_for(a, sys).whole ? BORDER_WHOLE : border_load(a, sys);
}

int border_color_load(const app *a, int sys)
{
	const system_cfg *s = &a->sys.systems[sys];
	const hf_model *m = hf_model_for(s->tag);
	char key[CFG_STR + 16];
	int c;

	if (!m) return 0;
	snprintf(key, sizeof key, "border_color.%s", s->tag);
	c = db_get_int(db_dev(), key, 0);
	return c >= 0 && c < m->ncolors ? c : 0;
}

SDL_Surface *border_frame_surface(const app *a, int sys, border_kind kind, int color,
                                  int *gx, int *gy, int *gw, int *gh)
{
	const hf_model *m = hf_model_for(a->sys.systems[sys].tag);
	SDL_Surface *s;
	int w, h, scale;

	if (!m || (kind != BORDER_HANDHELD && kind != BORDER_WHOLE)) return NULL;
	dmode_surface(&w, &h);
	if (!hf_place(m, kind == BORDER_WHOLE ? HF_WHOLE : HF_CUT, w, h, &scale, gx, gy, gw, gh)) return NULL;
	s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
	if (!s) return NULL;
	/* ARGB8888 on a little-endian CPU is B, G, R, A in memory: hf's BGRA,
	 * rows packed (pitch w * 4 for 32-bit surfaces SDL makes) */
	if (s->pitch != w * 4) { SDL_FreeSurface(s); return NULL; }
	hf_render(m, color, w, h, *gx, *gy, *gw, *gh, (uint8_t *)s->pixels);
	return s;
}

SDL_Surface *border_frame_cached(const app *a, int sys, border_kind kind, int color, char *key, size_t n,
                                 SDL_Rect *at)
{
	static SDL_Surface *kept;
	static char of[96];
	static SDL_Rect kept_at;
	int gx, gy, gw, gh;

	if (color < 0) color = border_color_load(a, sys);

	snprintf(key, n, "frame:%s:%s:%d", a->sys.systems[sys].tag, border_id(kind), color);
	if (kept && !strcmp(of, key)) {
		*at = kept_at;
		return kept;
	}
	if (kept) SDL_FreeSurface(kept);
	kept = border_frame_surface(a, sys, kind, color, &gx, &gy, &gw, &gh);
	kept_at = (SDL_Rect){ gx, gy, gw, gh };
	snprintf(of, sizeof of, "%s", kept ? key : "");
	*at = kept_at;
	return kept;
}

/* A surface as DTOV: "DTOV", width, height (16-bit LE), then BGRA rows */
bool border_write_dtov(SDL_Surface *s, const char *path)
{
	FILE *f = fopen(path, "wb");
	unsigned char hdr[8] = { 'D', 'T', 'O', 'V', (unsigned char)s->w, (unsigned char)(s->w >> 8),
	                         (unsigned char)s->h, (unsigned char)(s->h >> 8) };
	bool ok;
	int y;

	if (!f) return false;
	ok = fwrite(hdr, 1, sizeof hdr, f) == sizeof hdr;
	/* ARGB8888 in memory on a little-endian CPU is B, G, R, A: DTOV's */
	for (y = 0; ok && y < s->h; y++)
		ok = fwrite((const Uint8 *)s->pixels + (size_t)y * s->pitch, 4, (size_t)s->w, f) == (size_t)s->w;
	return fclose(f) == 0 && ok;
}

void border_send(app *a, int sys)
{
	const system_cfg *s = &a->sys.systems[sys];
	border_kind k = border_load(a, sys);

	if (k == BORDER_BLUR) {
		plat_resident_line("SETBORDER\tblur=1");
		return;
	}
	if (k == BORDER_HANDHELD || k == BORDER_WHOLE) {
		int gx, gy, gw, gh;
		SDL_Surface *pic = border_frame_surface(a, sys, k, border_color_load(a, sys), &gx, &gy, &gw, &gh);

		if (pic) {
			bool ok = border_write_dtov(pic, BORDER_FILE);

			SDL_FreeSurface(pic);
			/* The window with it: the game at the frame's own scale */
			if (ok) {
				plat_resident_line("SETBORDER\tpath=%s\trect=%dx%d+%d+%d", BORDER_FILE, gw, gh, gx, gy);
				return;
			}
		}
		fprintf(stderr, "border: %s: no frame drawn\n", s->name);
	}
	if (k == BORDER_OWN) {
		char png[CFG_STR * 3];
		int w, h;
		SDL_Surface *pic;

		dmode_surface(&w, &h);
		if (border_own_path(s, png, sizeof png) && (pic = border_own_surface(png, w, h))) {
			bool ok = border_write_dtov(pic, BORDER_FILE);

			SDL_FreeSurface(pic);
			if (ok) {
				plat_resident_line("SETBORDER\tpath=%s", BORDER_FILE);
				return;
			}
		}
		fprintf(stderr, "border: %s: cannot use %s\n", s->name, png[0] ? png : "(no picture)");
	}
	plat_resident_line("SETBORDER");
}
