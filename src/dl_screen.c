/* SPDX-License-Identifier: GPL-3.0-only */
/* MENU > Downloads: src/dl_screen.h */
#include "dl_screen.h"

#include "aout_route.h"
#include "artscrape.h"
#include "db.h"
#include "dlcount.h"
#include "dlq.h"
#include "ds/flex.h"
#include "ds/layout.h"
#include "ds/mark.h"
#include "ds/page.h"
#include "home.h"
#include "media_screen.h"
#include "mediatex.h"
#include "menu_pick.h"
#include "net.h"
#include "radl.h"
#include "radl_screen.h"
#include "rafetch.h"
#include "scraper_screen.h"
#include "sgdb.h"
#include "signin_screen.h"
#include "snaps.h"
#include "ss.h"
#include "sys_menu.h"
#include "uisound.h"
#include "usbdrive_screen.h"
#include "zyperdraw.h"

#include <SDL.h>
#include <SDL_image.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- MENU > Downloads: one page (DESIGN.md 11.53) ----------------------
 *
 * The page in the downloads lab, as I picked it (2026-11-19): a page drawn on
 * the ground with Zyper beside its heading (the Flash Drive pages' frame,
 * src/ds/page.h), not a MENU panel. Its views, one over another:
 *
 *   the hub      Download on top, a rule, then each kind with its mark and
 *                what it lacks; Left and Right take a kind in or out of
 *                Download and its size and time change at once; A opens a
 *                kind; over the keys the room a download takes, or where a
 *                kind comes from and its room on the card
 *   a kind       its systems' icons over their ticks and two tiles, what
 *                the card has and what a download adds (Achievements is its
 *                own 1.0.6 page, radl_screen.c)
 *   Systems      a row per system, a column per kind: a count, a tick or a
 *                dash; A switches a cell, X deletes it, Y opens its games
 *   a system's   its games, each with a mark per kind: lit, dim, struck
 *   games        out; A opens a game's panel
 *   the run      one stream for every lane: the game being asked about
 *                beside Zyper, the newest arrival landing big, the six
 *                before it moving along, each with its kind's mark; the
 *                counts per kind and the time left over the keys
 *   done         what came as a wall, the counts per kind, what is left
 *
 * Settings (START), its accounts, Video Snaps, Names and Details and On the
 * Card, and a game's panel are panels over the page.
 *
 * The page counts the card a system a frame (src/dlcount.h): it draws at
 * once, says Counting, and fills in; a count it made before stays up while
 * a new one goes. */

#define DL_ROW      DS_LINE_MENU   /* a row of the hub's list */
#define DL_RULE_H   14             /* the rule between Download and the kinds */
#define DL_MARK     40             /* a kind's mark before its name */
#define DL_ROW_PAD  20             /* a row's words in from its fill */
#define DL_COUNT_MS 6              /* the count's share of a frame */
#define DL_RING     14             /* arrivals kept: the run shows 7, done 14 */
#define DL_LAND     520            /* an arrival's landing, ms (the Scraper's) */
#define DL_BIG      360            /* the newest's square */
#define DL_CELL     168            /* the six before it */
#define DL_CELL_GAP 24
#define DL_WALL     120            /* done's wall, two rows of seven */
#define DL_SLOT_W   62             /* a kind page's system slot (the Scraper's) */
#define DL_BAND     56
#define DL_TILE_H   220
#define DL_SYS_ROWS 5              /* Systems' rows on the screen */
#define DL_CELL_W   96             /* a Systems column */
#define DL_GAME_ROWS 5             /* a system's games on the screen */

static const ui_glyph KIND_MARK[DLK_KINDS] = { UI_GLYPH_COVER, UI_GLYPH_FILM, UI_GLYPH_BOOK, UI_GLYPH_TROPHY };

static const char *kind_name(dl_kind k)
{
	switch (k) {
	case DLK_COVERS: return T(S_SCRAPER_PAGE_COVERS);
	case DLK_VIDEOS: return T(S_SCRAPER_PAGE_VIDEOS);
	case DLK_NAMES:  return T(S_SCRAPER_NAMES_DETAILS);
	default:         return T(S_SYSMENU_CHEEVOS);
	}
}

enum { V_HUB, V_KIND, V_SYSTEMS, V_GAMES, V_RUN, V_DONE };
enum { END_DONE, END_STOPPED, END_SIGN_IN };

typedef struct {
	int v;
	int sel;                 /* the hub's row (0 Download, 1 + a kind), a games row */
	int kind;                /* a kind page's */
	int row, col, win;       /* Systems' cell and window; a games list's window */
	char sys[CFG_STR];       /* a system's games: its folder */
	int fill_from;           /* the focus fill's glide: from this y, since t0 */
	unsigned fill_t0;
} dl_frame;

typedef struct {
	SDL_Texture *tex;
	int w, h;
	unsigned key;
	int kind;                /* dl_kind */
	int count;               /* a set's achievements */
	char game[128];
} dl_arrival;

/* A game of a system's games list, as the card has it */
enum { GS_HAVE, GS_MISS, GS_NONE, GS_OFF };
typedef struct {
	char name[LIB_NAME], title[LIB_NAME], file[LIB_PATH];
	unsigned char st[DLK_KINDS];
	int year;
	int sets;                /* its set's achievements, 0 not known */
	int gi;                  /* its place in the shelf's list */
} dl_game;

typedef struct {
	app *a;
	char only[CFG_STR];      /* a console's page: its folder */
	bool fixture;
	dl_frame st[8];
	int depth;
	const dl_counts *c;
	unsigned gen;
	struct { char key[CFG_STR]; SDL_Texture *t; } icon[CFG_MAX_SYSTEMS + 4];
	int nicon;
	/* the run this page shows */
	int run_kind;            /* -1 every kind that is in, else the one */
	bool art, ra;            /* its lanes */
	unsigned art_seen, ra_seen;
	art_progress ap;
	radl_progress rp;
	int art_total, art_looks;
	dl_arrival ring[DL_RING];
	int nring;
	unsigned land_at, next_key;
	unsigned t0;
	int done0;
	int end;
	char why[160];
	int pending;             /* a download to start once counted: -2 none */
	/* a system's games */
	dl_game *games;
	int ngames;
	unsigned shot_now;
} dl_view;

/* The lanes' ends this page (or the news) has already said */
static unsigned g_shown_art, g_shown_ra;

static dl_frame *top_of(dl_view *v) { return &v->st[v->depth - 1]; }

static void push(dl_view *v, int view)
{
	if (v->depth >= (int)(sizeof v->st / sizeof v->st[0])) return;
	memset(&v->st[v->depth], 0, sizeof v->st[0]);
	v->st[v->depth].v = view;
	v->st[v->depth].fill_from = -1;
	v->depth++;
}

/* A shot's network, lanes and progress (downloads_shot), -1 the real ones */
static int g_fx_net = -1, g_fx_art = -1, g_fx_ra = -1, g_fx_pct = -1;
static const char *g_fx_user;

static bool online(void)
{
	return g_fx_net >= 0 ? g_fx_net != 0 : online();
}

static bool lane_on(dlq_lane l)
{
	int fx = l == DLQ_ART ? g_fx_art : g_fx_ra;

	return fx >= 0 ? fx != 0 : dlq_running(l);
}

static bool dl_live(void)
{
	return lane_on(DLQ_ART) || lane_on(DLQ_RA);
}

/* ---- the systems' icons --------------------------------------------------------- */

static SDL_Texture *icon_of(dl_view *v, const char *key)
{
	char path[CFG_STR * 2];
	int i;

	if (!key || !key[0]) return NULL;
	for (i = 0; i < v->nicon; i++) if (!strcmp(v->icon[i].key, key)) return v->icon[i].t;
	if (v->nicon >= (int)(sizeof v->icon / sizeof v->icon[0])) return NULL;
	snprintf(path, sizeof path, "%s/system-icons/%s-still.png", v->fixture ? "res" : P_ROOT, key);
	snprintf(v->icon[v->nicon].key, sizeof v->icon[0].key, "%s", key);
	if ((v->icon[v->nicon].t = IMG_LoadTexture(v->a->r, path))) {
		SDL_SetTextureScaleMode(v->icon[v->nicon].t, SDL_ScaleModeNearest);
		SDL_SetTextureBlendMode(v->icon[v->nicon].t, SDL_BLENDMODE_BLEND);
	}
	return v->icon[v->nicon++].t;
}

static void icon_draw(dl_view *v, const char *key, int cx, int cy, int size, Uint8 al)
{
	SDL_Texture *t = icon_of(v, key);
	int w, h;

	if (!t) return;
	SDL_QueryTexture(t, NULL, NULL, &w, &h);
	if (w >= h) { h = h * size / w; w = size; } else { w = w * size / h; h = size; }
	SDL_SetTextureAlphaMod(t, al);
	SDL_RenderCopy(v->a->r, t, NULL, &(SDL_Rect){ cx - w / 2, cy - h / 2, w, h });
	SDL_SetTextureAlphaMod(t, 255);
}

static void icons_free(dl_view *v)
{
	int i;

	for (i = 0; i < v->nicon; i++) if (v->icon[i].t) SDL_DestroyTexture(v->icon[i].t);
	v->nicon = 0;
}

static const dl_sys *sys_of(const dl_view *v, const char *folder)
{
	int i;

	for (i = 0; v->c && folder && i < v->c->n; i++)
		if (!strcmp(v->c->s[i].folder, folder)) return &v->c->s[i];
	return NULL;
}

/* ---- words ------------------------------------------------------------------------ */

static void about_time(double secs, char *out, size_t n)
{
	char t[32];
	int m = (int)((secs + 59.0) / 60.0);

	if (m < 1) m = 1;
	if (m < 60) snprintf(t, sizeof t, T(S_CORNER_TIME_MIN), m);
	else if (m % 60 == 0) snprintf(t, sizeof t, T(S_CORNER_TIME_H), m / 60);
	else snprintf(t, sizeof t, T(S_CORNER_TIME_H_MIN), m / 60, m % 60);
	snprintf(out, n, T(S_SCRAPER_ABOUT_TIME), t);
}

static void time_left(double secs, char *out, size_t n)
{
	int m = (int)((secs + 59.0) / 60.0);

	if (m < 1) m = 1;
	if (m < 60) snprintf(out, n, T(S_CORNER_TIME_MIN), m);
	else if (m % 60 == 0) snprintf(out, n, T(S_CORNER_TIME_H), m / 60);
	else snprintf(out, n, T(S_CORNER_TIME_H_MIN), m / 60, m % 60);
}

static void bytes_text(double b, char *out, size_t n)
{
	sys_menu_bytes((unsigned long long)(b > 0 ? b : 0), out, n);
}

/* How far the running lanes are, together */
static int run_pct(void)
{
	return g_fx_pct >= 0 ? g_fx_pct : dlq_pct();
}

static int lane_pct(dlq_lane l)
{
	return g_fx_pct >= 0 ? (l == DLQ_ART ? 37 : 52) : dlq_lane_pct(l);
}

/* The Download row's value and MENU's (downloads_menu_value) */
static void download_value(const dl_counts *c, char *out, size_t n)
{
	dl_plan pl;
	char sz[24], tm[48];

	if (dl_live()) { snprintf(out, n, T(S_DOWNLOADS_RUNNING), run_pct()); return; }
	if (!c) { snprintf(out, n, "%s", T(S_DOWNLOADS_COUNTING)); return; }
	if (!online()) { snprintf(out, n, "%s", T(S_MENU_NEEDS_WIFI)); return; }
	dl_plan_of(c, -1, &pl);
	if (!pl.n) { snprintf(out, n, "%s", T(S_DOWNLOADS_NOTHING)); return; }
	bytes_text(pl.bytes, sz, sizeof sz);
	about_time(pl.secs, tm, sizeof tm);
	snprintf(out, n, "%s \xc2\xb7 %s", sz, tm);
}

/* The lane a kind goes on, and whether it runs */
static bool kind_running(dl_kind k)
{
	return lane_on(k == DLK_ACH ? DLQ_RA : DLQ_ART);
}

static void kind_value(const dl_counts *c, dl_kind k, char *out, size_t n)
{
	dl_sum s;
	const char *b;

	if (!c) { snprintf(out, n, "%s", T(S_DOWNLOADS_COUNTING)); return; }
	if (!dl_kind_in(k)) { snprintf(out, n, "%s", T(S_SCRAPER_OFF)); return; }
	if ((b = dl_kind_blocked(c, k))) { snprintf(out, n, "%s", b); return; }
	dl_sum_of(c, k, &s);
	if (!s.on) { snprintf(out, n, "%s", T(S_SCRAPER_OFF)); return; }
	if (kind_running(k)) {
		/* the number alone: Download's row over it says what goes on */
		snprintf(out, n, "%d%%", lane_pct(k == DLK_ACH ? DLQ_RA : DLQ_ART));
		return;
	}
	if (!s.todo) {
		snprintf(out, n, "%s", k == DLK_ACH ? T(S_DOWNLOADS_READY) : T(S_SCRAPER_COMPLETE));
		return;
	}
	switch (k) {
	case DLK_COVERS: snprintf(out, n, TN(S_DOWNLOADS_MISSING_N, s.todo), s.todo); break;
	case DLK_VIDEOS: snprintf(out, n, TN(S_DOWNLOADS_TO_FETCH_N, s.todo), s.todo); break;
	case DLK_NAMES:  snprintf(out, n, TN(S_DOWNLOADS_TO_LOOK_UP_N, s.todo), s.todo); break;
	default:         snprintf(out, n, TN(S_DOWNLOADS_TO_DOWNLOAD_N, s.todo), s.todo); break;
	}
}

/* Where a kind comes from and its room on the card: the line over the keys
 * on its row */
static void kind_facts(const dl_counts *c, dl_kind k, char *out, size_t n)
{
	dl_sum s;
	char sz[24], oc[64];

	dl_sum_of(c, k, &s);
	bytes_text((double)s.bytes, sz, sizeof sz);
	snprintf(oc, sizeof oc, T(S_DOWNLOADS_ON_CARD), sz);
	switch (k) {
	case DLK_COVERS:
		snprintf(out, n, "%s \xc2\xb7 %s", oc, T(S_DOWNLOADS_LIBRETRO_FIRST));
		break;
	case DLK_VIDEOS: {
		static const str_id FOR[SNAPS_FOR_COUNT] = { S_SCRAPER_ALL_GAMES, S_LIBRARY_FAVORITES, S_SCRAPER_PLAYED };
		char on[96];

		if (!s.on) { snprintf(out, n, "%s", oc); break; }
		snprintf(on, sizeof on, TN(S_DOWNLOADS_FOR_ON_N, s.on), T(FOR[snaps_for()]), s.on);
		snprintf(out, n, "%s \xc2\xb7 %s", on, oc);
		break;
	}
	case DLK_NAMES:
		snprintf(out, n, "%s", T(S_DOWNLOADS_ONE_REQUEST));
		break;
	default:
		snprintf(out, n, "%s \xc2\xb7 %s", T(S_DOWNLOADS_FROM_RA), oc);
		break;
	}
}

/* The line a Pixel 2 with no network adds: it has no Wi-Fi inside */
static const char *no_net_line(void)
{
	return plat_profile()->wifi == DEV_WIFI_BUILTIN ? T(S_DOWNLOADS_NO_NET_BUILTIN) : T(S_DOWNLOADS_NO_NET_ADAPTER);
}

/* ---- drawing: the parts ------------------------------------------------------------ */

static float ease_out(float t)
{
	t = t < 0 ? 0 : t > 1 ? 1 : t;
	return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

static void foot(dl_view *v, const ds_page_frame *pf, const char *const *k, const char *const *w, int n)
{
	(void)v;
	ds_page_foot(v->a->r, pf, k, w, n, DS_PAGE_FOOT_GAP, DS_PAGE_FOOT_GAP_TIGHT);
}

/* A row of the hub's list (and a system's games): its fill when it is the
 * cursor's, a mark, its words and its value */
typedef struct {
	ui_glyph mark;
	const char *label, *value;
	bool off;
} dl_row;

static void rows_draw(dl_view *v, dl_frame *f, const dl_row *rows, int n, int rule_after, int y0, int sel,
                      unsigned now)
{
	app *a = v->a;
	const ds_theme *t = theme();
	TTF_Font *fm = ui_font(UI_F_MENU), *fs = ui_font(UI_F_META);
	int x0 = DS_INSET + 8, x1 = ZYPEROS_SCREEN_W - DS_INSET - 8;
	int i, y = y0, fy = y0;
	char lab[160], val[96];

	for (i = 0; i < n; i++) {
		if (i == sel) fy = y;
		y += DL_ROW + (i == rule_after ? DL_RULE_H : 0);
	}
	/* the fill glides to its row over quick */
	if (sel >= 0) {
		float e = f->fill_from >= 0 ? ease_out((float)(now - f->fill_t0) / DS_MS_QUICK) : 1.0f;
		int yy = f->fill_from >= 0 ? (int)(f->fill_from + (fy - f->fill_from) * e) : fy;

		ui_round_rect(a->r, &(SDL_Rect){ x0, yy, x1 - x0, DL_ROW }, DS_RADIUS_ROW, ui_col(t->accent, 255));
		if (e >= 1.0f) f->fill_from = -1;
	}
	for (y = y0, i = 0; i < n; i++) {
		bool on = i == sel;
		SDL_Color ink = on ? ui_col(t->on_accent, 255) : rows[i].off ? UI_TEXT_DIM : UI_TEXT_SOFT;
		SDL_Color vk = on ? ui_col(t->on_accent, 255) : UI_TEXT_DIM;
		int cy = y + DL_ROW / 2, lx = x0 + DL_ROW_PAD, vw = 0;

		if (rows[i].value && rows[i].value[0]) {
			ui_fit_text(fs, rows[i].value, val, sizeof val, (x1 - x0) / 2);
			vw = ui_text_width(fs, val);
			ui_text(a->r, fs, val, x1 - DL_ROW_PAD, ui_cap_y(fs, cy), 1, vk);
		}
		if (rows[i].mark != UI_GLYPH_COUNT) {
			ui_glyph_draw(a->r, rows[i].mark, lx + DL_MARK / 2, cy, DL_MARK, ink);
			lx += DL_MARK + 14;
		}
		ui_fit_text(fm, rows[i].label, lab, sizeof lab, x1 - DL_ROW_PAD - vw - 16 - lx);
		ui_text(a->r, fm, lab, lx, ui_cap_y(fm, cy), -1, ink);
		y += DL_ROW;
		if (i == rule_after) {
			SDL_SetRenderDrawColor(a->r, t->edge.r, t->edge.g, t->edge.b, 255);
			SDL_RenderFillRect(a->r, &(SDL_Rect){ x0 + DL_ROW_PAD, y + DL_RULE_H / 2 - 1, x1 - x0 - 2 * DL_ROW_PAD, 2 });
			y += DL_RULE_H;
		}
	}
}

static void glide(dl_frame *f, int from_y, unsigned now)
{
	f->fill_from = from_y;
	f->fill_t0 = now;
}

/* A tile's colour for a game with no picture: a hue of its name, the run's
 * stand-in and done's */
static SDL_Color hue_of(const char *s, int light)
{
	unsigned h = 7;
	float hue, r, g, b, c, x;
	int i;

	for (; s && *s; s++) h = h * 31 + (unsigned char)*s;
	hue = (float)(h % 360) / 60.0f;
	c = 0.42f;
	x = c * (1.0f - (float)fabs(fmodf(hue, 2.0f) - 1.0f));
	i = (int)hue;
	r = g = b = 0;
	switch (i) {
	case 0: r = c; g = x; break;
	case 1: r = x; g = c; break;
	case 2: g = c; b = x; break;
	case 3: g = x; b = c; break;
	case 4: r = x; b = c; break;
	default: r = c; b = x; break;
	}
	return (SDL_Color){ (Uint8)((r + 0.18f + light * 0.04f) * 255), (Uint8)((g + 0.18f) * 255),
	                    (Uint8)((b + 0.18f) * 255), 255 };
}

/* An arrival in `box`: its cover whole (never cut, never stretched), or a
 * tile of its name's hue; its kind's mark on the top right corner, with the
 * set's count on the big one */
static void arrival_draw(dl_view *v, const dl_arrival *it, SDL_FRect box, Uint8 al, bool lit)
{
	app *a = v->a;
	const ds_theme *t = theme();
	TTF_Font *fc = ui_font(UI_F_META);
	SDL_Rect q;
	float sw = box.w, sh = box.h, sc;
	bool big = box.w >= 200;

	if (!al) return;
	if (it->tex && it->w > 0 && it->h > 0) {
		sc = box.w / (float)it->w;
		if ((float)it->h * sc > box.h) sc = box.h / (float)it->h;
		sw = it->w * sc;
		sh = it->h * sc;
	}
	q = (SDL_Rect){ (int)(box.x + (box.w - sw) / 2), (int)(box.y + (box.h - sh) / 2), (int)sw, (int)sh };
	if (lit) ui_round_rect(a->r, &(SDL_Rect){ q.x - 7, q.y - 7, q.w + 14, q.h + 14 }, 16, ui_col(t->accent, al));
	if (it->tex) {
		SDL_SetTextureAlphaMod(it->tex, al);
		SDL_RenderCopy(a->r, it->tex, NULL, &q);
		SDL_SetTextureAlphaMod(it->tex, 255);
	} else {
		SDL_Color c = hue_of(it->game, 0);
		char nm[96];

		c.a = al;
		ui_round_rect(a->r, &q, DS_RADIUS_ROW, c);
		ui_fit_text(fc, it->game, nm, sizeof nm, q.w - 32);
		if (big) ui_text(a->r, fc, nm, q.x + 24, ui_cap_y(fc, q.y + q.h - 40), -1, ui_fade(ui_col((ds_rgb){ 255, 255, 255 }, 255), al / 255.0f));
	}
	if (it->kind == DLK_COVERS) return;
	if (big && it->count > 0) {
		/* the trophy and the count, a lime pill */
		char n[16];
		int tw, pw, px, py = q.y + 14;

		snprintf(n, sizeof n, "%d", it->count);
		tw = ui_text_width(fc, n);
		pw = 36 + 8 + tw + 28;
		px = q.x + q.w - 14 - pw;
		ui_round_rect(a->r, &(SDL_Rect){ px, py, pw, 56 }, 28, ui_col(t->accent, al));
		ui_glyph_draw(a->r, KIND_MARK[it->kind], px + 14 + 18, py + 28, 36, ui_col(t->on_accent, al));
		ui_text(a->r, fc, n, px + 14 + 36 + 8, ui_cap_y(fc, py + 28), -1, ui_col(t->on_accent, al));
	} else {
		int d = big ? DS_BADGE : DS_BADGE_SMALL;

		ds_badge(a->r, q.x + q.w - d / 2 - 8, q.y + d / 2 + 8, d, KIND_MARK[it->kind], true, al);
	}
}

/* Marks and numbers in a line centred on `cx`: the counts per kind, and the
 * time left beside a clock when `eta` is given */
static void stats_draw(dl_view *v, int cx, int cy, const int got[DLK_KINDS], const bool shown[DLK_KINDS],
                       const char *eta)
{
	app *a = v->a;
	TTF_Font *f = ui_font(UI_F_MENU);
	char num[DLK_KINDS][16];
	int i, w = 0, x;

	for (i = 0; i < DLK_KINDS; i++) {
		snprintf(num[i], sizeof num[i], "%d", got[i]);
		if (shown[i]) w += DS_MARK_STAT + 8 + ui_text_width(f, num[i]) + 32;
	}
	if (eta) w += DS_MARK_STAT + 8 + ui_text_width(f, eta);
	else w -= 32;
	x = cx - w / 2;
	for (i = 0; i < DLK_KINDS; i++) {
		if (!shown[i]) continue;
		ui_glyph_draw(a->r, KIND_MARK[i], x + DS_MARK_STAT / 2, cy, DS_MARK_STAT, UI_TEXT_SOFT);
		x += DS_MARK_STAT + 8;
		ui_text(a->r, f, num[i], x, ui_cap_y(f, cy), -1, UI_TEXT);
		ui_fit_drawn(num[i]);
		x += ui_text_width(f, num[i]) + 32;
	}
	if (eta) {
		ui_glyph_draw(a->r, UI_GLYPH_CLOCK, x + DS_MARK_STAT / 2, cy, DS_MARK_STAT, UI_TEXT_SOFT);
		x += DS_MARK_STAT + 8;
		ui_text(a->r, f, eta, x, ui_cap_y(f, cy), -1, UI_TEXT);
	}
}

/* ---- the hub ------------------------------------------------------------------------ */

static const char *page_title(dl_view *v)
{
	const dl_sys *s = v->only[0] ? sys_of(v, v->only) : NULL;
	int i;

	if (!v->only[0]) return T(S_DOWNLOADS_HEADING);
	if (s) return s->name;
	for (i = 0; i < v->a->sys.count; i++)
		if (!strcmp(v->a->sys.systems[i].folder, v->only)) return v->a->sys.systems[i].name;
	return v->only;
}

/* The heading's second line, as every view of the page says it */
static void sub_of(dl_view *v, int only, char *out, size_t n, ui_glyph *mark)
{
	dl_plan pl;

	*mark = UI_GLYPH_COUNT;
	if (dl_live()) {
		*mark = UI_GLYPH_TO_CARD;
		snprintf(out, n, T(S_DOWNLOADS_RUNNING), run_pct());
		return;
	}
	if (!v->c) {
		int d, of;

		dl_count_progress(&d, &of);
		snprintf(out, n, "%s", T(S_DOWNLOADS_COUNTING));
		return;
	}
	if (!online()) {
		*mark = UI_GLYPH_WIFI;
		snprintf(out, n, "%s", T(S_MENU_NEEDS_WIFI));
		return;
	}
	dl_plan_of(v->c, only, &pl);
	if (!pl.n) {
		*mark = UI_GLYPH_CHECK;
		snprintf(out, n, "%s", T(S_DOWNLOADS_EVERYTHING_HERE));
		return;
	}
	*mark = UI_GLYPH_CLOCK;
	about_time(pl.secs, out, n);
}

static void hub_draw(dl_view *v, dl_frame *f, unsigned now)
{
	app *a = v->a;
	ds_page_frame pf = page_top(a, 1.0f);
	dl_row rows[1 + DLK_KINDS];
	char vals[1 + DLK_KINDS][96], sub[96], line[192];
	ui_glyph mark;
	int k, sel = f->sel;

	sub_of(v, -1, sub, sizeof sub, &mark);
	ds_page_head(a->r, &pf, DS_HEAD_SUB, page_title(v), sub, mark, now);
	download_value(v->c, vals[0], sizeof vals[0]);
	rows[0] = (dl_row){ UI_GLYPH_COUNT, T(S_RADL_KEY_DOWNLOAD), vals[0], false };
	for (k = 0; k < DLK_KINDS; k++) {
		kind_value(v->c, (dl_kind)k, vals[1 + k], sizeof vals[0]);
		rows[1 + k] = (dl_row){ KIND_MARK[k], kind_name((dl_kind)k), vals[1 + k],
		                        v->c && !strcmp(vals[1 + k], T(S_SCRAPER_OFF)) };
	}
	rows_draw(v, f, rows, 1 + DLK_KINDS, 0, pf.body_y, sel, now);
	/* Over the keys: why it cannot, the room a download takes, or where the
	 * kind under the cursor comes from */
	if (v->c && !online()) {
		ds_line_item it = { UI_GLYPH_COUNT, no_net_line(), DS_TONE_INFO };

		ds_page_line(a->r, &pf, &it, 1);
	} else if (v->c && sel == 0) {
		dl_plan pl;

		dl_plan_of(v->c, -1, &pl);
		if (pl.n) {
			char sz[24], fr[24], tf[64], fr2[64];
			ds_line_item it;

			bytes_text(pl.bytes, sz, sizeof sz);
			snprintf(tf, sizeof tf, T(S_SCRAPER_TO_FETCH), sz);
			if (v->c->free) {
				bytes_text((double)v->c->free, fr, sizeof fr);
				snprintf(fr2, sizeof fr2, T(S_STORAGE_FREE), fr);
				snprintf(line, sizeof line, "%s \xc2\xb7 %s", tf, fr2);
			} else {
				snprintf(line, sizeof line, "%s", tf);
			}
			it = (ds_line_item){ UI_GLYPH_TO_CARD, line, pl.fits ? DS_TONE_INFO : DS_TONE_WARN };
			ds_page_line(a->r, &pf, &it, 1);
		} else {
			ds_line_item it = { UI_GLYPH_CHECK, T(S_DOWNLOADS_OFFLINE_NOW), DS_TONE_INFO };

			ds_page_line(a->r, &pf, &it, 1);
		}
	} else if (v->c && sel > 0) {
		ds_line_item it = { UI_GLYPH_COUNT, line, DS_TONE_INFO };

		kind_facts(v->c, (dl_kind)(sel - 1), line, sizeof line);
		ds_page_line(a->r, &pf, &it, 1);
	}
	if (sel == 0) {
		const char *K[4] = { "A", "Y", "START", "B" };
		const char *W[4] = { dl_live() ? T(S_DOWNLOADS_KEY_OPEN) : T(S_RADL_KEY_DOWNLOAD), T(S_DOWNLOADS_KEY_SYSTEMS),
		                     T(S_HOME_SETTINGS), T(S_SCRAPER_KEY_CLOSE) };

		foot(v, &pf, K, W, 4);
	} else {
		const char *K[5] = { "A", "< >", "Y", "START", "B" };
		const char *W[5] = { T(S_DOWNLOADS_KEY_OPEN), T(S_DOWNLOADS_KEY_ON_OFF), T(S_DOWNLOADS_KEY_SYSTEMS),
		                     T(S_HOME_SETTINGS), T(S_SCRAPER_KEY_CLOSE) };

		foot(v, &pf, K, W, 5);
	}
}

/* ---- a kind's page: its systems and two tiles ------------------------------------ */

static void kind_draw(dl_view *v, dl_frame *f, unsigned now)
{
	app *a = v->a;
	const ds_theme *t = theme();
	TTF_Font *ft = ui_font(UI_F_TITLE), *fm = ui_font(UI_F_MENU), *fs = ui_font(UI_F_META);
	ds_page_frame pf = page_top(a, 1.0f);
	dl_kind k = (dl_kind)f->kind;
	static ds_flex F;
	SDL_Rect sr[9], tq[2];
	dl_sum s = { 0 };
	dl_plan pl = { 0 };
	char sub[96], line[192], big[2][24], sz[2][32], ts[2][96];
	ui_glyph mark;
	int i, shown, more, slot0 = 0, tile0 = 0, n = v->c ? v->c->n : 0;
	const char *blocked = v->c ? dl_kind_blocked(v->c, k) : NULL;
	static const str_id LEFT[DLK_KINDS] = { S_DOWNLOADS_TILE_ON_CARD, S_DOWNLOADS_TILE_ON_CARD,
	                                        S_DOWNLOADS_TILE_NAMED, S_RADL_READY };
	static const str_id RIGHT[DLK_KINDS] = { S_DOWNLOADS_TILE_MISSING, S_RADL_TO_DOWNLOAD,
	                                         S_DOWNLOADS_TILE_TO_LOOK_UP, S_RADL_TO_DOWNLOAD };

	sub_of(v, k, sub, sizeof sub, &mark);
	if (blocked && !dl_live()) { snprintf(sub, sizeof sub, "%s", blocked); mark = UI_GLYPH_COUNT; }
	ds_page_head(a->r, &pf, DS_HEAD_SUB, kind_name(k), sub, mark, now);
	if (v->c) {
		dl_sum_of(v->c, k, &s);
		dl_plan_of(v->c, k, &pl);
	}
	{
		const int root = ds_flex_begin(&F, ZYPEROS_SCREEN_W, ZYPEROS_SCREEN_H,
		                               &(ds_box){ .dir = DS_COL, .pad = { pf.body.x, pf.body.y,
		                                          ZYPEROS_SCREEN_W - pf.body.x - pf.body.w,
		                                          ZYPEROS_SCREEN_H - pf.body.y - pf.body.h } });
		const int cell = (pf.body.w - DL_SLOT_W) / 8;
		const int slots = ds_flex_add(&F, root, &(ds_box){ .dir = DS_ROW, .gap = cell - DL_SLOT_W, .h = 2 * DL_BAND });
		int tiles;

		for (i = 0; i < 9; i++) {
			int id = ds_flex_add(&F, slots, &(ds_box){ .w = DL_SLOT_W, .h = 2 * DL_BAND });

			if (!i) slot0 = id;
		}
		tiles = ds_flex_add(&F, root, &(ds_box){ .dir = DS_ROW, .justify = DS_J_BETWEEN, .pad = { 0, 8, 0, 0 },
		                                         .h = 8 + DL_TILE_H });
		tile0 = ds_flex_add(&F, tiles, &(ds_box){ .w = DS_PAGE_TILE_W, .h = DL_TILE_H });
		ds_flex_add(&F, tiles, &(ds_box){ .w = DS_PAGE_TILE_W, .h = DL_TILE_H });
		ds_flex_solve(&F);
		for (i = 0; i < 9; i++) sr[i] = ds_flex_rect(&F, slot0 + i);
		tq[0] = ds_flex_rect(&F, tile0);
		tq[1] = ds_flex_rect(&F, tile0 + 1);
	}
	/* The systems: each icon over a tick, lit when it lacks nothing; dim
	 * when its switch is off. Nine at most, and how many more. */
	{
		int idx[CFG_MAX_SYSTEMS], m = 0;

		for (i = 0; i < n; i++) {
			const dl_sys *d = &v->c->s[i];

			if (k == DLK_ACH && !d->ach_sys) continue;
			if (k == DLK_NAMES && !d->names_ok && v->c->ss_in) continue;
			idx[m++] = i;
		}
		shown = m > 9 ? 8 : m;
		more = m - shown;
		for (i = 0; i < shown; i++) {
			const dl_sys *d = &v->c->s[idx[i]];
			bool on;
			int todo = dl_sys_todo(v->c, idx[i], k, &on);
			int cx = sr[i].x + DL_SLOT_W / 2, iy = sr[i].y + DL_BAND / 2, my = iy + DL_BAND;

			icon_draw(v, d->key, cx, iy, 60, on ? 255 : 90);
			ds_switch(a->r, cx, my, 28, UI_GLYPH_CHECK, on && !todo);
		}
		if (more > 0) {
			char mm[16];

			snprintf(mm, sizeof mm, "+%d", more);
			ui_text(a->r, fm, mm, sr[8].x + DL_SLOT_W / 2, ui_cap_y(fm, sr[8].y + DL_BAND / 2), 0, UI_TEXT_SOFT);
		}
	}
	/* The two tiles: what the card has, and what a download adds */
	snprintf(big[0], sizeof big[0], "%d", s.have);
	snprintf(big[1], sizeof big[1], "%d", s.todo);
	sz[0][0] = sz[1][0] = '\0';
	if (k != DLK_NAMES) bytes_text((double)s.bytes, sz[0], sizeof sz[0]);
	if (k != DLK_NAMES && s.todo && pl.bytes > 0) {
		char b[24];

		bytes_text(pl.bytes, b, sizeof b);
		snprintf(sz[1], sizeof sz[1], "+%s", b);
	}
	if (s.have_sys) snprintf(ts[0], sizeof ts[0], TN(S_SCRAPER_ON_N_SYSTEMS, s.have_sys), s.have_sys);
	else snprintf(ts[0], sizeof ts[0], "%s", T(S_DOWNLOADS_NONE_YET));
	if (!s.on) snprintf(ts[1], sizeof ts[1], "%s", T(S_DOWNLOADS_ALL_OFF));
	else if (!s.todo) snprintf(ts[1], sizeof ts[1], "%s", k == DLK_ACH ? T(S_RADL_ALL_CHECKED) : T(S_DOWNLOADS_COMPLETE_LOW));
	else if (k == DLK_VIDEOS) {
		static const str_id FOR[SNAPS_FOR_COUNT] = { S_SCRAPER_ALL_GAMES, S_LIBRARY_FAVORITES, S_SCRAPER_PLAYED };

		snprintf(ts[1], sizeof ts[1], "%s", T(FOR[snaps_for()]));
	} else if (k == DLK_ACH) snprintf(ts[1], sizeof ts[1], "%s", T(S_RADL_NEVER_CHECKED));
	else snprintf(ts[1], sizeof ts[1], TN(S_SCRAPER_ON_N_SYSTEMS, s.todo_sys), s.todo_sys);
	for (i = 0; i < 2; i++) {
		SDL_Rect q = tq[i];
		ui_glyph g = i == 0 ? UI_GLYPH_CHECK : k == DLK_NAMES ? UI_GLYPH_BOOK : UI_GLYPH_TO_CARD;
		bool lit = i == 0 ? s.have > 0 : s.todo > 0;
		char nm[96];

		ui_round_rect(a->r, &q, DS_RADIUS_CARD, ui_col(t->raised, 255));
		ui_glyph_draw(a->r, g, q.x + UD_PAD + 28, q.y + 50, 56, lit ? ui_col(t->accent_ink, 255) : UI_TEXT_DIM);
		ui_fit_text(fm, T(i == 0 ? LEFT[k] : RIGHT[k]), nm, sizeof nm, q.w - 2 * UD_PAD - 72);
		ui_text(a->r, fm, nm, q.x + UD_PAD + 72, ui_cap_y(fm, q.y + 50), -1, UI_TEXT);
		ui_text(a->r, ft, v->c ? big[i] : "\xe2\x80\xa6", q.x + UD_PAD, ui_cap_y(ft, q.y + 128), -1, lit ? UI_TEXT : UI_TEXT_DIM);
		if (v->c && sz[i][0]) ui_text(a->r, fm, sz[i], q.x + q.w - UD_PAD, ui_cap_y(fm, q.y + 128), 1, UI_TEXT_SOFT);
		if (v->c) {
			ui_fit_text(fs, ts[i], nm, sizeof nm, q.w - 2 * UD_PAD);
			ui_text(a->r, fs, nm, q.x + UD_PAD, ui_cap_y(fs, q.y + 180), -1, UI_TEXT_SOFT);
		}
	}
	/* Over the keys: what it takes, and where from */
	if (v->c && !online()) {
		ds_line_item it = { UI_GLYPH_COUNT, no_net_line(), DS_TONE_INFO };

		ds_page_line(a->r, &pf, &it, 1);
	} else if (v->c) {
		char b[24], tf[64];
		ds_line_item it;

		bytes_text(pl.bytes, b, sizeof b);
		snprintf(tf, sizeof tf, T(S_SCRAPER_TO_FETCH), pl.bytes > 0 ? b : "0 MB");
		if (k == DLK_COVERS) {
			snprintf(line, sizeof line, "%s \xc2\xb7 %s", tf, T(S_DOWNLOADS_LIBRETRO_FIRST));
			it = (ds_line_item){ UI_GLYPH_TO_CARD, line, DS_TONE_INFO };
		} else if (k == DLK_VIDEOS) {
			char fr[24], f2[48];

			bytes_text((double)v->c->free, fr, sizeof fr);
			snprintf(f2, sizeof f2, T(S_STORAGE_FREE), fr);
			snprintf(line, sizeof line, "%s \xc2\xb7 %s", tf, f2);
			it = (ds_line_item){ UI_GLYPH_TO_CARD, line, pl.fits ? DS_TONE_INFO : DS_TONE_WARN };
		} else {
			snprintf(line, sizeof line, TN(S_DOWNLOADS_NAMES_REQUESTS, s.todo), s.todo);
			it = (ds_line_item){ UI_GLYPH_BOOK, line, DS_TONE_INFO };
		}
		ds_page_line(a->r, &pf, &it, 1);
	}
	{
		const char *K[4] = { "A", "X", "Y", "B" };
		const char *W[4] = { dl_live() ? T(S_DOWNLOADS_KEY_OPEN) : T(S_RADL_KEY_DOWNLOAD), T(S_SCRAPER_VIDEOS_FOR),
		                     T(S_DOWNLOADS_KEY_SYSTEMS), T(S_DOWNLOADS_KEY_BACK) };

		if (k == DLK_VIDEOS) foot(v, &pf, K, W, 4);
		else {
			const char *K3[3] = { "A", "Y", "B" };
			const char *W3[3] = { W[0], W[2], W[3] };

			foot(v, &pf, K3, W3, 3);
		}
	}
}

/* ---- Systems: a row per system, a column per kind -------------------------------- */

static void cell_words(dl_view *v, int i, dl_kind k, char *out, size_t n)
{
	bool on;
	int todo = dl_sys_todo(v->c, i, k, &on);
	const char *b = dl_kind_blocked(v->c, k);

	if (b) { snprintf(out, n, "%s", b); return; }
	if (!on || !dl_kind_in(k)) { snprintf(out, n, "%s", T(S_SCRAPER_OFF)); return; }
	if (k == DLK_ACH && !v->c->s[i].ach_sys) { snprintf(out, n, "%s", T(S_SCRAPER_OFF)); return; }
	if (!todo) { snprintf(out, n, "%s", k == DLK_ACH ? T(S_DOWNLOADS_READY) : T(S_SCRAPER_COMPLETE)); return; }
	switch (k) {
	case DLK_COVERS: snprintf(out, n, TN(S_DOWNLOADS_MISSING_N, todo), todo); break;
	case DLK_VIDEOS: snprintf(out, n, TN(S_DOWNLOADS_TO_FETCH_N, todo), todo); break;
	case DLK_NAMES:  snprintf(out, n, TN(S_DOWNLOADS_TO_LOOK_UP_N, todo), todo); break;
	default:         snprintf(out, n, TN(S_DOWNLOADS_TO_DOWNLOAD_N, todo), todo); break;
	}
}

static bool cell_lit(dl_view *v, int i, dl_kind k, int *todo)
{
	bool on;

	*todo = dl_sys_todo(v->c, i, k, &on);
	if (dl_kind_blocked(v->c, k) || !dl_kind_in(k) || !on) return false;
	if (k == DLK_ACH && !v->c->s[i].ach_sys) return false;
	if (k == DLK_NAMES && !v->c->s[i].names_ok) return false;
	return true;
}

static void systems_draw(dl_view *v, dl_frame *f, unsigned now)
{
	app *a = v->a;
	const ds_theme *t = theme();
	TTF_Font *fm = ui_font(UI_F_MENU), *fs = ui_font(UI_F_META);
	ds_page_frame pf = page_top(a, 1.0f);
	int n = v->c ? v->c->n : 0, r, k, cx = pf.x1 - 8 - DLK_KINDS * DL_CELL_W, y0 = pf.body_y + 52;
	const dl_sys *cur = n ? &v->c->s[f->row] : NULL;
	char line[192], w[96], nm[96];

	ds_page_head(a->r, &pf, DS_HEAD_SUB, T(S_DOWNLOADS_KEY_SYSTEMS), cur ? cur->name : "", UI_GLYPH_COUNT, now);
	if (f->row < f->win) f->win = f->row;
	if (f->row >= f->win + DL_SYS_ROWS) f->win = f->row - DL_SYS_ROWS + 1;
	/* the kinds' marks over their columns, the cursor's in text */
	for (k = 0; k < DLK_KINDS; k++)
		ui_glyph_draw(a->r, KIND_MARK[k], cx + k * DL_CELL_W + DL_CELL_W / 2, pf.body_y + 18, 40,
		              k == f->col ? UI_TEXT : UI_TEXT_DIM);
	for (r = f->win; r < n && r < f->win + DL_SYS_ROWS; r++) {
		const dl_sys *s = &v->c->s[r];
		int y = y0 + (r - f->win) * DL_ROW, cy = y + DL_ROW / 2;

		icon_draw(v, s->key, pf.x0 + 48, cy, 52, 255);
		ui_fit_text(fs, s->name, nm, sizeof nm, cx - pf.x0 - 96 - 16);
		ui_text(a->r, fs, nm, pf.x0 + 96, ui_cap_y(fs, cy), -1, UI_TEXT_SOFT);
		for (k = 0; k < DLK_KINDS; k++) {
			int todo, x = cx + k * DL_CELL_W + DL_CELL_W / 2;

			if (!cell_lit(v, r, (dl_kind)k, &todo)) {
				ui_text(a->r, fm, "\xe2\x80\x93", x, ui_cap_y(fm, cy), 0, UI_TEXT_DIM);
			} else if (!todo) {
				ui_glyph_draw(a->r, UI_GLYPH_CHECK, x, cy, 34, ui_col(t->accent_ink, 255));
			} else {
				char c[16];

				snprintf(c, sizeof c, "%d", todo);
				ui_text(a->r, fs, c, x, ui_cap_y(fs, cy), 0, UI_TEXT);
			}
		}
	}
	if (f->win > 0) ds_more(a->r, cx + DLK_KINDS * DL_CELL_W / 2.0f, (float)(y0 - DS_MORE_H - 4), true, UI_TEXT_DIM);
	if (f->win + DL_SYS_ROWS < n)
		ds_more(a->r, cx + DLK_KINDS * DL_CELL_W / 2.0f, (float)(y0 + DL_SYS_ROWS * DL_ROW + 4), false, UI_TEXT_DIM);
	/* the cell under the cursor: a ring, and its words over the keys */
	if (cur) {
		SDL_Rect q = { cx + f->col * DL_CELL_W + 4, y0 + (f->row - f->win) * DL_ROW + 2, DL_CELL_W - 8, DL_ROW - 4 };
		int e = DS_FOCUS_RING;

		ui_round_rect(a->r, &(SDL_Rect){ q.x - e, q.y - e, q.w + 2 * e, q.h + 2 * e }, 16 + e, ui_col(t->accent_ink, 255));
		ui_round_rect(a->r, &q, 16, ui_col(t->ground, 255));
		{
			int todo, x = q.x + q.w / 2, cy = q.y + q.h / 2;

			if (!cell_lit(v, f->row, (dl_kind)f->col, &todo))
				ui_text(a->r, fm, "\xe2\x80\x93", x, ui_cap_y(fm, cy), 0, UI_TEXT_DIM);
			else if (!todo) ui_glyph_draw(a->r, UI_GLYPH_CHECK, x, cy, 34, ui_col(t->accent_ink, 255));
			else {
				char c[16];

				snprintf(c, sizeof c, "%d", todo);
				ui_text(a->r, fs, c, x, ui_cap_y(fs, cy), 0, UI_TEXT);
			}
		}
		cell_words(v, f->row, (dl_kind)f->col, w, sizeof w);
		snprintf(line, sizeof line, "%s \xc2\xb7 %s \xc2\xb7 %s", cur->name, kind_name((dl_kind)f->col), w);
		{
			ds_line_item it = { UI_GLYPH_COUNT, line, DS_TONE_INFO };

			ds_page_line(a->r, &pf, &it, 1);
		}
	}
	{
		bool del = cur && (f->col == DLK_VIDEOS ? cur->vid_have > 0 : f->col == DLK_ACH ? cur->ach_ready > 0 : false);
		const char *K[4] = { "A", "X", "Y", "B" };
		const char *W[4] = { T(S_DOWNLOADS_KEY_ON_OFF), T(S_DOWNLOADS_KEY_DELETE), T(S_DOWNLOADS_KEY_GAMES),
		                     T(S_DOWNLOADS_KEY_BACK) };

		if (del) foot(v, &pf, K, W, 4);
		else {
			const char *K3[3] = { "A", "Y", "B" };
			const char *W3[3] = { W[0], W[2], W[3] };

			foot(v, &pf, K3, W3, 3);
		}
	}
}

/* ---- a system's games: each with a mark per kind --------------------------------- */

static void sysgames_free(dl_view *v)
{
	free(v->games);
	v->games = NULL;
	v->ngames = 0;
}

static int name_cmp(const void *x, const void *y)
{
	return strcmp(*(char *const *)x, *(char *const *)y);
}

static char **list_dir(const char *dir, int *n)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	char **v = NULL;
	int cap = 0;

	*n = 0;
	if (!d) return NULL;
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		if (*n == cap) {
			char **nv = realloc(v, (size_t)(cap = cap ? cap * 2 : 64) * sizeof *v);

			if (!nv) break;
			v = nv;
		}
		if (!(v[*n] = strdup(e->d_name))) break;
		(*n)++;
	}
	closedir(d);
	if (*n) qsort(v, (size_t)*n, sizeof *v, name_cmp);
	return v;
}

static void list_free(char **v, int n)
{
	int i;

	for (i = 0; i < n; i++) free(v[i]);
	free(v);
}

static bool listed(char **v, int n, const char *name, const char *ext)
{
	char k[LIB_NAME + 16];
	const char *p = k;

	snprintf(k, sizeof k, "%s%s", name, ext);
	return n && bsearch(&p, v, (size_t)n, sizeof *v, name_cmp);
}

/* The system's games and what each has: its folder's list read once, its
 * covers', its videos' and its sets' folders read once, its titles once */
static void sysgames_load(dl_view *v, const char *folder)
{
	app *a = v->a;
	int i, si = -1, nset = 0, nmp4 = 0, npng = 0;
	const game_list *gl;
	char dir[LIB_PATH * 2], **sets, **mp4, **png;
	db_titles titles = { 0 };
	const dl_sys *s = sys_of(v, folder);

	sysgames_free(v);
	for (i = 0; i < a->sys.count; i++) if (!strcmp(a->sys.systems[i].folder, folder)) si = i;
	if (si < 0) return;
	gl = sys_games(&a->view[si]);
	if (!gl->count || !(v->games = calloc((size_t)gl->count, sizeof *v->games))) return;
	snprintf(dir, sizeof dir, "%s/%s/.cheevos", P_ROMS, folder);
	sets = list_dir(dir, &nset);
	mp4 = snaps_stems(P_ROMS, folder, &nmp4);
	png = snaps_cover_stems(P_ROMS, folder, &npng);
	db_titles_load(db_lib(), folder, &titles);
	snprintf(dir, sizeof dir, "%s/%s", P_ROMS, folder);
	for (i = 0; i < gl->count; i++) {
		const game_entry *g = &gl->items[i];
		dl_game *d = &v->games[v->ngames++];
		const char *stem = g->name, *t;

		snprintf(d->name, sizeof d->name, "%s", g->name);
		snprintf(d->title, sizeof d->title, "%s", g->title[0] ? g->title : g->name);
		snprintf(d->file, sizeof d->file, "%s", g->file);
		d->gi = i;
		d->st[DLK_COVERS] = (npng && bsearch(&stem, png, (size_t)npng, sizeof *png, name_cmp)) ||
		                    es_art_cover(dir, g->file, g->name) ? GS_HAVE : GS_MISS;
		if ((nmp4 && bsearch(&stem, mp4, (size_t)nmp4, sizeof *mp4, name_cmp)) || es_art_video(dir, g->file, g->name))
			d->st[DLK_VIDEOS] = GS_HAVE;
		else d->st[DLK_VIDEOS] = dl_sys_on(DLK_VIDEOS, folder) ? GS_MISS : GS_OFF;
		t = db_titles_find(&titles, g->file);
		d->st[DLK_NAMES] = t && *t ? GS_HAVE : GS_MISS;
		if (!s || !s->ach_sys) d->st[DLK_ACH] = GS_OFF;
		else if (listed(sets, nset, g->name, ".set")) d->st[DLK_ACH] = GS_HAVE;
		else if (listed(sets, nset, g->name, ".none")) d->st[DLK_ACH] = GS_NONE;
		else d->st[DLK_ACH] = GS_MISS;
	}
	db_titles_free(&titles);
	snaps_stems_free(mp4, nmp4);
	snaps_stems_free(png, npng);
	list_free(sets, nset);
}

/* What a game has and lacks, in two lines over the keys */
static void game_facts(const dl_game *g, char *l1, size_t n1, char *l2, size_t n2)
{
	static const str_id MISS[DLK_KINDS] = { S_DOWNLOADS_W_COVER, S_DOWNLOADS_W_VIDEO, S_DOWNLOADS_W_NAMES,
	                                        S_DOWNLOADS_W_ACHIEVEMENTS };
	char have[192] = "", miss[160] = "";
	size_t o = 0, m = 0;
	int k;

	if (g->st[DLK_COVERS] == GS_HAVE) o += (size_t)snprintf(have + o, sizeof have - o, "%s", T(S_DOWNLOADS_HAS_COVER));
	if (g->st[DLK_VIDEOS] == GS_HAVE && o < sizeof have)
		o += (size_t)snprintf(have + o, sizeof have - o, "%s%s", o ? " \xc2\xb7 " : "", T(S_DOWNLOADS_HAS_VIDEO));
	if (g->st[DLK_NAMES] == GS_HAVE && o < sizeof have)
		o += (size_t)snprintf(have + o, sizeof have - o, "%s%s", o ? " \xc2\xb7 " : "", T(S_DOWNLOADS_HAS_NAMES));
	if (g->st[DLK_ACH] == GS_HAVE && o < sizeof have)
		o += (size_t)snprintf(have + o, sizeof have - o, "%s%s", o ? " \xc2\xb7 " : "", T(S_DOWNLOADS_HAS_ACHIEVEMENTS));
	for (k = 0; k < DLK_KINDS; k++)
		if (g->st[k] == GS_MISS && m < sizeof miss)
			m += (size_t)snprintf(miss + m, sizeof miss - m, "%s%s", m ? ", " : "", T(MISS[k]));
	snprintf(l1, n1, "%s", o ? have : T(S_DOWNLOADS_NOTHING_YET));
	if (m) snprintf(l2, n2, T(S_DOWNLOADS_MISSING_LIST), miss);
	else if (g->st[DLK_ACH] == GS_NONE) snprintf(l2, n2, "%s", T(S_DOWNLOADS_NO_ACHIEVEMENTS));
	else snprintf(l2, n2, "%s", T(S_DOWNLOADS_EVERYTHING_HERE));
}

static void sysgames_draw(dl_view *v, dl_frame *f, unsigned now)
{
	app *a = v->a;
	const ds_theme *t = theme();
	TTF_Font *fm = ui_font(UI_F_MENU), *fs = ui_font(UI_F_META);
	ds_page_frame pf = page_top(a, 1.0f);
	const dl_sys *s = sys_of(v, f->sys);
	int x0 = DS_INSET + 8, x1 = ZYPEROS_SCREEN_W - DS_INSET - 8, i, k, y0 = pf.body_y;
	char sub[48], nm[160], l1[192], l2[192];

	snprintf(sub, sizeof sub, TN(S_DOWNLOADS_N_GAMES, v->ngames), v->ngames);
	ds_page_head(a->r, &pf, DS_HEAD_SUB, s ? s->name : f->sys, sub, UI_GLYPH_COUNT, now);
	if (f->sel < f->win) f->win = f->sel;
	if (f->sel >= f->win + DL_GAME_ROWS) f->win = f->sel - DL_GAME_ROWS + 1;
	if (v->ngames) {
		float e = f->fill_from >= 0 ? ease_out((float)(now - f->fill_t0) / DS_MS_QUICK) : 1.0f;
		int fy = y0 + (f->sel - f->win) * DL_ROW;
		int yy = f->fill_from >= 0 ? (int)(f->fill_from + (fy - f->fill_from) * e) : fy;

		ui_round_rect(a->r, &(SDL_Rect){ x0, yy, x1 - x0, DL_ROW }, DS_RADIUS_ROW, ui_col(t->accent, 255));
		if (e >= 1.0f) f->fill_from = -1;
	}
	for (i = f->win; i < v->ngames && i < f->win + DL_GAME_ROWS; i++) {
		const dl_game *g = &v->games[i];
		bool on = i == f->sel;
		int cy = y0 + (i - f->win) * DL_ROW + DL_ROW / 2, mx = x1 - DL_ROW_PAD - DLK_KINDS * 44 + 17;
		SDL_Color ink = on ? ui_col(t->on_accent, 255) : UI_TEXT_SOFT;

		ui_fit_text(fm, g->title, nm, sizeof nm, mx - 24 - x0 - DL_ROW_PAD);
		ui_text(a->r, fm, nm, x0 + DL_ROW_PAD, ui_cap_y(fm, cy), -1, ink);
		for (k = 0; k < DLK_KINDS; k++) {
			int st = g->st[k], x = mx + k * 44;
			Uint8 al = st == GS_HAVE ? 255 : st == GS_OFF ? 30 : 82;

			ui_glyph_draw(a->r, KIND_MARK[k], x, cy, 34, ui_fade(ink, al / 255.0f));
			if (st == GS_NONE) {
				/* struck: the source has none */
				SDL_SetRenderDrawColor(a->r, ink.r, ink.g, ink.b, 200);
				SDL_RenderDrawLine(a->r, x - 18, cy + 12, x + 18, cy - 12);
				SDL_RenderDrawLine(a->r, x - 18, cy + 13, x + 18, cy - 11);
			}
		}
	}
	if (f->win > 0) ds_more(a->r, ZYPEROS_SCREEN_W / 2.0f, (float)(y0 - DS_MORE_H - 4), true, UI_TEXT_DIM);
	if (f->win + DL_GAME_ROWS < v->ngames)
		ds_more(a->r, ZYPEROS_SCREEN_W / 2.0f, (float)(y0 + DL_GAME_ROWS * DL_ROW + 4), false, UI_TEXT_DIM);
	if (v->ngames) {
		game_facts(&v->games[f->sel], l1, sizeof l1, l2, sizeof l2);
		ui_fit_text(fs, l1, nm, sizeof nm, pf.x1 - pf.x0);
		ui_text(a->r, fs, nm, ZYPEROS_SCREEN_W / 2, ui_cap_y(fs, pf.line_y - DS_LINE_META), 0, UI_TEXT_DIM);
		ui_fit_text(fs, l2, nm, sizeof nm, pf.x1 - pf.x0);
		ui_text(a->r, fs, nm, ZYPEROS_SCREEN_W / 2, ui_cap_y(fs, pf.line_y), 0, UI_TEXT_DIM);
	}
	{
		const char *K[2] = { "A", "B" }, *W[2] = { T(S_DOWNLOADS_KEY_OPEN), T(S_DOWNLOADS_KEY_BACK) };

		foot(v, &pf, K, W, 2);
	}
}

/* ---- the run: one stream for every lane ------------------------------------------- */

static void ring_free(dl_view *v)
{
	int i;

	for (i = 0; i < v->nring; i++) if (v->ring[i].tex) SDL_DestroyTexture(v->ring[i].tex);
	v->nring = 0;
}

static void ring_add(dl_view *v, int kind, const char *path, const char *game, int count)
{
	dl_arrival *it;

	if (v->nring == DL_RING) {
		if (v->ring[DL_RING - 1].tex) SDL_DestroyTexture(v->ring[DL_RING - 1].tex);
		v->nring--;
	}
	memmove(&v->ring[1], &v->ring[0], (size_t)v->nring * sizeof v->ring[0]);
	v->nring++;
	it = &v->ring[0];
	memset(it, 0, sizeof *it);
	it->kind = kind;
	it->count = count;
	it->key = ++v->next_key;
	snprintf(it->game, sizeof it->game, "%s", game ? game : "");
	if (path && path[0] && !v->fixture) mt_ask((int)it->key, path, DL_BIG);
	else v->land_at = anim_now();
}

static bool take_pictures(dl_view *v)
{
	SDL_Surface *s;
	int key, i;
	bool any = false;

	while (mt_take(&key, &s)) {
		for (i = 0; i < v->nring; i++) {
			if (v->ring[i].key != (unsigned)key || v->ring[i].tex || !s) continue;
			v->ring[i].tex = SDL_CreateTextureFromSurface(v->a->r, s);
			v->ring[i].w = s->w;
			v->ring[i].h = s->h;
			if (i == 0) v->land_at = anim_now();
			any = true;
		}
		if (s) SDL_FreeSurface(s);
	}
	if (!any) {
		/* a picture that never comes: land the tile anyway */
		if (v->nring && !v->ring[0].tex && anim_now() - v->land_at > 1500 && v->land_at) any = false;
	}
	return any;
}

/* What came since the last look, from each lane */
static bool take_arrivals(dl_view *v)
{
	bool any = false;

	if (v->art && dlq_running(DLQ_ART)) art_status(&v->ap);
	if (v->ra && dlq_running(DLQ_RA)) radl_status(&v->rp);
	if (v->ap.found_seq != v->art_seen) {
		int kind = v->ap.found_video ? DLK_VIDEOS : v->ap.found_names ? DLK_NAMES : DLK_COVERS;

		v->art_seen = v->ap.found_seq;
		ring_add(v, kind, v->ap.found_path, v->ap.found_game, 0);
		any = true;
	}
	if (v->rp.set_seq != v->ra_seen) {
		char path[LIB_PATH * 3] = "", name[LIB_NAME];
		const char *b = strrchr(v->rp.set_path, '/');
		size_t ln;

		v->ra_seen = v->rp.set_seq;
		snprintf(name, sizeof name, "%s", b ? b + 1 : v->rp.set_path);
		ln = strlen(name);
		if (ln > 4 && !strcmp(name + ln - 4, ".set")) name[ln - 4] = '\0';
		if (has_box_art(v->rp.folder_now, name)) box_art_path(v->rp.folder_now, name, path, sizeof path);
		ring_add(v, DLK_ACH, path, v->rp.set_title[0] ? v->rp.set_title : name, v->rp.set_count);
		any = true;
	}
	return any;
}

static void run_counts(const dl_view *v, int got[DLK_KINDS])
{
	got[DLK_COVERS] = v->ap.found;
	got[DLK_VIDEOS] = v->ap.snaps;
	got[DLK_NAMES] = v->ap.named;
	got[DLK_ACH] = v->rp.fetched + v->rp.updated;
}

static void run_shown(const dl_view *v, bool shown[DLK_KINDS])
{
	int k;

	for (k = 0; k < DLK_KINDS; k++) shown[k] = v->run_kind < 0 ? (k == DLK_ACH ? v->ra : v->art) : v->run_kind == k;
	if (v->run_kind < 0 && v->art) {
		shown[DLK_COVERS] = dl_kind_in(DLK_COVERS);
		shown[DLK_VIDEOS] = dl_kind_in(DLK_VIDEOS) && v->c && v->c->ss_in;
		shown[DLK_NAMES] = dl_kind_in(DLK_NAMES) && v->c && v->c->ss_in;
	}
}

/* Done and to do, across the lanes */
static void run_progress(const dl_view *v, int *done, int *total)
{
	int ad = 0, at = 0, rd = 0, rt = 0;

	if (v->art) {
		ad = v->ap.looked + v->ap.dlooked;
		at = v->art_total + (v->ap.details ? v->ap.dtotal : v->art_looks);
		if (at < ad) at = ad;
	}
	if (v->ra) {
		rd = v->rp.games_done;
		rt = v->rp.games > rd ? v->rp.games : rd;
	}
	*done = ad + rd;
	*total = at + rt;
}

static double run_eta(const dl_view *v, unsigned now)
{
	int done, total;
	double art = 0, ra = 0, el = (now - v->t0) / 1000.0;

	run_progress(v, &done, &total);
	if (v->art) {
		int left = v->art_total + (v->ap.details ? v->ap.dtotal : v->art_looks) - v->ap.looked - v->ap.dlooked;

		art = (left > 0 ? left : 0) * (v->c && v->c->ss_in ? DL_S_SS : DL_S_LR);
	}
	if (v->ra) {
		int left = v->rp.games - v->rp.games_done;

		ra = (left > 0 ? left : 0) * DL_S_SET;
	}
	/* the run's own pace once it has one */
	if (done - v->done0 >= 6 && el >= 15 && total > done)
		return el / (done - v->done0) * (total - done);
	return art > ra ? art : ra;
}

static void run_draw(dl_view *v, unsigned now)
{
	app *a = v->a;
	TTF_Font *ft = ui_font(UI_F_TITLE), *fs = ui_font(UI_F_META);
	ds_page_frame pf = page_top(a, 1.0f);
	int hx = pf.head_x, m, done, total, cell0 = 0, sq;
	float e = v->nring ? ease_out((float)(now - v->land_at) / DL_LAND) : 1.0f;
	char fit[160], cnt[48], eta[48];
	static ds_flex F;
	SDL_FRect cell[6], big;
	const char *title, *folder;
	ui_glyph km = UI_GLYPH_COVER, src = UI_GLYPH_CLOUD;
	const dl_sys *s;
	const char *K[2] = { "B", "Y" }, *W[2] = { T(S_UPDATE_BACK_GOES_ON), T(S_SCRAPER_KEY_STOP) };
	bool art_now = v->art && lane_on(DLQ_ART) && v->ap.now[0];

	/* The game being asked about: the art lane's, the longer, else the
	 * achievements lane's; its system, its kind and its source */
	if (art_now) {
		title = v->ap.now;
		folder = v->ap.folder_now;
		km = v->ap.details ? (dl_kind_in(DLK_NAMES) ? UI_GLYPH_BOOK : UI_GLYPH_FILM) : UI_GLYPH_COVER;
		src = v->ap.src == ART_SRC_SG ? UI_GLYPH_GRID : v->ap.src == ART_SRC_LR ? UI_GLYPH_SHELF : UI_GLYPH_CLOUD;
	} else {
		title = v->rp.now[0] ? v->rp.now : v->rp.folder_now;
		folder = v->rp.folder_now;
		km = UI_GLYPH_TROPHY;
		src = UI_GLYPH_TROPHY;
	}
	s = sys_of(v, folder);
	ui_fit_text(ft, title, fit, sizeof fit, pf.x1 - hx);
	ui_text(a->r, ft, fit, hx, ui_cap_y(ft, DS_PAGE_TITLE_Y), -1, UI_TEXT);
	if (s) icon_draw(v, s->key, hx + 26, DS_PAGE_SUB_Y, 52, 255);
	ui_glyph_draw(a->r, km, hx + 84, DS_PAGE_SUB_Y, 36, UI_TEXT_SOFT);
	ui_glyph_draw(a->r, src, hx + 132, DS_PAGE_SUB_Y, 36, UI_TEXT_SOFT);
	run_progress(v, &done, &total);
	snprintf(cnt, sizeof cnt, "%d / %d", done, total);
	{
		int bx = hx + 168, w = pf.x1 - bx - ui_text_width(fs, cnt) - 24;

		ds_bar(a->r, bx, DS_PAGE_SUB_Y - UI_BAR_H / 2, w, total > 0 ? (int)((long long)w * (done > total ? total : done) / total) : 0, 255);
	}
	ui_text(a->r, fs, cnt, pf.x1, ui_cap_y(fs, DS_PAGE_SUB_Y), 1, UI_TEXT_DIM);
	/* The body: the newest's square at the left, the six before it three by
	 * two in what is left (the Scraper's run) */
	{
		const int root = ds_flex_begin(&F, ZYPEROS_SCREEN_W, ZYPEROS_SCREEN_H,
		                               &(ds_box){ .dir = DS_ROW, .gap = DL_CELL_GAP,
		                                          .pad = { pf.body.x, pf.body.y, ZYPEROS_SCREEN_W - pf.body.x - pf.body.w,
		                                                   ZYPEROS_SCREEN_H - pf.body.y - pf.body.h } });
		const int grid_w = pf.body.w - DL_BIG - DL_CELL_GAP;
		int grid, k;

		sq = ds_flex_add(&F, root, &(ds_box){ .w = DL_BIG, .h = DL_BIG });
		grid = ds_flex_add(&F, root, &(ds_box){ .dir = DS_ROW, .wrap = true, .w = grid_w,
		                                        .gap = (grid_w - 3 * DL_CELL) / 2, .line_gap = DL_CELL_GAP });
		for (k = 0; k < 6; k++) {
			int id = ds_flex_add(&F, grid, &(ds_box){ .w = DL_CELL, .h = DL_CELL });

			if (!k) cell0 = id;
		}
		ds_flex_solve(&F);
		for (k = 0; k < 6; k++) {
			SDL_Rect q = ds_flex_rect(&F, cell0 + k);

			cell[k] = (SDL_FRect){ (float)q.x, (float)q.y, (float)q.w, (float)q.h };
		}
		{
			SDL_Rect q = ds_flex_rect(&F, sq);

			big = (SDL_FRect){ (float)(q.x + 8), (float)(q.y + 8), (float)(q.w - 16), (float)(q.h - 16) };
		}
	}
	if (!v->nring) {
		const ds_theme *t = theme();
		int k;

		ui_round_rect(a->r, &(SDL_Rect){ (int)big.x, (int)big.y, (int)big.w, (int)big.h }, DS_RADIUS_ROW, ui_col(t->raised, 255));
		for (k = 0; k < 6; k++)
			ui_round_rect(a->r, &(SDL_Rect){ (int)cell[k].x, (int)cell[k].y, (int)cell[k].w, (int)cell[k].h }, DS_RADIUS_ROW,
			              ui_col(t->raised, 160));
	}
	for (m = 6; m >= 0; m--) {
		int k = m + 1;
		SDL_FRect to = cell[m < 6 ? m : 5], from = m == 0 ? big : cell[m - 1];

		if (k >= v->nring) continue;
		if (m == 6) arrival_draw(v, &v->ring[k], from, (Uint8)(255 * (1.0f - e)), false);
		else arrival_draw(v, &v->ring[k], (SDL_FRect){ from.x + (to.x - from.x) * e, from.y + (to.y - from.y) * e,
		                                               from.w + (to.w - from.w) * e, from.h + (to.h - from.h) * e },
		                  255, false);
	}
	if (v->nring) {
		SDL_FRect b = { big.x + big.w * 0.06f * (1 - e), big.y + 40 * (1 - e) + big.h * 0.06f * (1 - e),
		                big.w * (0.88f + 0.12f * e), big.h * (0.88f + 0.12f * e) };

		arrival_draw(v, &v->ring[0], b, (Uint8)(255 * e), true);
	}
	/* Over the keys: why it waits, or the counts per kind and the time left */
	if (!online() || (v->ra && lane_on(DLQ_RA) && v->rp.state == RADL_NO_NETWORK)) {
		ds_line_item it = { UI_GLYPH_WIFI, T(S_RADL_WAITING_WIFI), DS_TONE_WARN };

		ds_page_line(a->r, &pf, &it, 1);
	} else if (v->ra && lane_on(DLQ_RA) && (v->rp.state == RADL_SLOWED || v->rp.state == RADL_RETRYING)) {
		unsigned pn = plat_now_ms(), left = v->rp.resume_at > pn ? v->rp.resume_at - pn : 0, sec = (left + 999) / 1000;
		char w[24], line[160];
		ds_line_item it;

		snprintf(w, sizeof w, "%u:%02u", sec / 60, sec % 60);
		snprintf(line, sizeof line, v->rp.state == RADL_SLOWED ? T(S_RADL_PAUSED) : T(S_RADL_RETRYING), w);
		it = (ds_line_item){ UI_GLYPH_CLOCK, line, DS_TONE_WARN };
		ds_page_line(a->r, &pf, &it, 1);
	} else {
		int got[DLK_KINDS];
		bool shown[DLK_KINDS];

		run_counts(v, got);
		run_shown(v, shown);
		time_left(run_eta(v, now), eta, sizeof eta);
		stats_draw(v, ZYPEROS_SCREEN_W / 2, pf.line_y, got, shown, eta);
	}
	foot(v, &pf, K, W, 2);
}

/* Done: what came as a wall, the counts per kind, what is left */
static void done_draw(dl_view *v, unsigned now)
{
	app *a = v->a;
	TTF_Font *fs = ui_font(UI_F_META);
	ds_page_frame pf = page_top(a, 1.0f);
	const char *head = v->end == END_SIGN_IN ? T(S_RADL_SIGN_IN_AGAIN) : v->end == END_STOPPED ? T(S_RADL_STOPPED)
	                 : T(S_SCRAPER_ALL_DONE);
	int m, n = v->nring > 14 ? 14 : v->nring, got[DLK_KINDS];
	float gap = (pf.body.w - 7.0f * DL_WALL) / 6;
	bool shown[DLK_KINDS];
	const char *K[2] = { "A", "B" };
	const char *W[2] = { v->end == END_SIGN_IN ? T(S_RADL_KEY_SIGN_IN) : T(S_SCRAPER_KEY_AGAIN), T(S_SCRAPER_KEY_CLOSE) };

	ds_page_head(a->r, &pf, DS_HEAD_SUB, head, v->why[0] ? v->why : NULL, UI_GLYPH_COUNT, now);
	for (m = 0; m < n; m++) {
		int k = n - 1 - m;     /* oldest first */

		arrival_draw(v, &v->ring[k], (SDL_FRect){ pf.body.x + (m % 7) * (DL_WALL + gap),
		                                         (float)(pf.body_y + (m / 7) * (DL_WALL + 16)), DL_WALL, DL_WALL },
		             255, false);
	}
	run_counts(v, got);
	run_shown(v, shown);
	stats_draw(v, ZYPEROS_SCREEN_W / 2, pf.body_y + 2 * DL_WALL + 16 + 48, got, shown, NULL);
	{
		ds_line_item it;

		if (v->end == END_DONE) it = (ds_line_item){ UI_GLYPH_CHECK, T(S_DOWNLOADS_OFFLINE_NOW), DS_TONE_INFO };
		else it = (ds_line_item){ UI_GLYPH_COUNT, T(S_DOWNLOADS_NEXT_FROM_MISSING), DS_TONE_INFO };
		ds_page_line(a->r, &pf, &it, 1);
	}
	(void)fs;
	foot(v, &pf, K, W, 2);
}

static void dl_draw(dl_view *v, unsigned now)
{
	dl_frame *f = top_of(v);

	switch (f->v) {
	case V_KIND:    kind_draw(v, f, now); break;
	case V_SYSTEMS: systems_draw(v, f, now); break;
	case V_GAMES:   sysgames_draw(v, f, now); break;
	case V_RUN:     run_draw(v, now); break;
	case V_DONE:    done_draw(v, now); break;
	default:        hub_draw(v, f, now); break;
	}
}

/* The page under a panel opened from it (g_base_page) */
static void dl_base(app *a, void *ctx)
{
	(void)a;
	dl_draw(ctx, anim_now());
}

/* ---- starting and ending a run ------------------------------------------------------- */

/* Download: each lane with what it has to do, side by side (src/dlq.h).
 * `only` one kind, or -1 for every kind that is in. */
static bool run_start(dl_view *v, int only)
{
	const dl_counts *c = v->c;
	dl_plan pl;
	systems_cfg art_cfg, ra_cfg;
	scraper_kinds k;
	bool art = false, ra = false;
	int i, j, covers = 0;

	if (!c) return false;
	if (!online()) {
		result_message(v->a, T(S_DOWNLOADS_HEADING), MSG_NEEDS_WIFI);
		return false;
	}
	dl_plan_of(c, only, &pl);
	if (!pl.n) return false;
	if (!pl.fits) {
		result_message(v->a, T(S_DOWNLOADS_HEADING), T(S_SCRAPER_NO_ROOM));
		return false;
	}
	k.covers = (only < 0 || only == DLK_COVERS) && dl_kind_in(DLK_COVERS);
	k.videos = (only < 0 || only == DLK_VIDEOS) && dl_kind_in(DLK_VIDEOS) && c->ss_in;
	k.names = (only < 0 || only == DLK_NAMES) && dl_kind_in(DLK_NAMES) && c->ss_in;
	if (only >= 0 && only != DLK_ACH) {
		/* a kind's own page: that kind, whatever Download has */
		k.covers = only == DLK_COVERS;
		k.videos = only == DLK_VIDEOS && c->ss_in;
		k.names = only == DLK_NAMES && c->ss_in;
	}
	memset(&art_cfg, 0, sizeof art_cfg);
	memset(&ra_cfg, 0, sizeof ra_cfg);
	for (i = 0; i < c->n; i++) {
		const dl_sys *s = &c->s[i];
		const system_cfg *sc = NULL;

		for (j = 0; j < v->a->sys.count; j++)
			if (!strcmp(v->a->sys.systems[j].folder, s->folder)) sc = &v->a->sys.systems[j];
		if (!sc) continue;
		if ((k.covers && dl_sys_on(DLK_COVERS, s->folder)) || (k.names && dl_sys_on(DLK_NAMES, s->folder) && s->names_ok) ||
		    (k.videos && dl_sys_on(DLK_VIDEOS, s->folder))) {
			if (art_cfg.count < CFG_MAX_SYSTEMS) art_cfg.systems[art_cfg.count++] = *sc;
			if (k.covers && dl_sys_on(DLK_COVERS, s->folder)) covers += s->cov_miss;
		}
		if (s->ach_sys && dl_sys_on(DLK_ACH, s->folder) && ra_cfg.count < CFG_MAX_SYSTEMS)
			ra_cfg.systems[ra_cfg.count++] = *sc;
	}
	if (pl.covers + pl.videos + pl.looks > 0 && art_cfg.count)
		art = scraper_start_run(v->a, k, &art_cfg, v->only[0] ? v->only : NULL, covers);
	if ((only < 0 || only == DLK_ACH) && pl.ach > 0 && ra_cfg.count)
		ra = radl_start_run(v->a, &ra_cfg, v->only[0] ? v->only : NULL);
	if (!art && !ra) return false;
	ring_free(v);
	memset(&v->ap, 0, sizeof v->ap);
	memset(&v->rp, 0, sizeof v->rp);
	v->art = art;
	v->ra = ra;
	v->art_seen = v->ra_seen = 0;
	v->art_total = covers;
	v->art_looks = pl.looks;
	v->run_kind = only;
	v->t0 = anim_now();
	v->done0 = 0;
	v->why[0] = '\0';
	return true;
}

/* Back on a run that went on behind the screens */
static void run_rejoin(dl_view *v)
{
	ring_free(v);
	v->art = dlq_running(DLQ_ART);
	v->ra = dlq_running(DLQ_RA);
	if (v->art) art_status(&v->ap);
	if (v->ra) radl_status(&v->rp);
	v->art_seen = v->ap.found_seq;
	v->ra_seen = v->rp.set_seq;
	v->art_total = scraper_run_total();
	v->art_looks = v->ap.dtotal;
	v->run_kind = -1;
	v->t0 = anim_now();
	{
		int d, t;

		run_progress(v, &d, &t);
		v->done0 = d;
	}
}

/* The run is over, here or behind the screens: how it ended, from what the
 * lanes kept as they ended */
static void run_finish(dl_view *v)
{
	dlq_end ea = v->art ? dlq_last(DLQ_ART) : DLQ_NONE, er = v->ra ? dlq_last(DLQ_RA) : DLQ_NONE;

	if (v->art) scraper_last(&v->ap);
	if (v->ra) radl_last(&v->rp);
	v->end = END_DONE;
	if (v->ra && v->rp.problem == RADL_SIGN_IN) {
		v->end = END_SIGN_IN;
		snprintf(v->why, sizeof v->why, "%s", T(S_RADL_REFUSED));
	} else if (ea == DLQ_FULL || er == DLQ_FULL || (v->ra && v->rp.problem == RADL_NO_ROOM)) {
		v->end = END_STOPPED;
		snprintf(v->why, sizeof v->why, "%s", T(S_SCRAPER_NO_ROOM));
	} else if (ea == DLQ_STOPPED || er == DLQ_STOPPED) {
		v->end = END_STOPPED;
		if (!v->why[0]) snprintf(v->why, sizeof v->why, "%s", T(S_DOWNLOADS_YOU_STOPPED));
	} else if (v->ra && v->rp.problem == RADL_NO_ANSWER) {
		v->end = END_STOPPED;
		snprintf(v->why, sizeof v->why, "%s", T(S_RADL_NO_ANSWER));
	} else if (v->art && v->ap.problem[0]) {
		snprintf(v->why, sizeof v->why, "%s", v->ap.problem);
	}
	g_shown_art = dlq_ends(DLQ_ART);
	g_shown_ra = dlq_ends(DLQ_RA);
	dl_count_stale();
	dl_count_again(v->a, v->only);
}

/* ---- panels over the page ---------------------------------------------------------------- */

/* Videos For: which games' video snaps a download gets */
typedef struct { char val[SNAPS_FOR_COUNT][64]; } vfor_ctx;

static int vfor_build(void *ctx, menu_row *rows, int max, const char **heading)
{
	static const str_id FOR[SNAPS_FOR_COUNT] = { S_SCRAPER_ALL_GAMES, S_LIBRARY_FAVORITES, S_SCRAPER_PLAYED };
	vfor_ctx *x = ctx;
	const dl_counts *c = dl_count_shown(NULL);
	int w, i;

	(void)max;
	*heading = T(S_SCRAPER_VIDEOS_FOR);
	for (w = 0; w < SNAPS_FOR_COUNT; w++) {
		int n = 0;
		char sz[24];

		for (i = 0; c && i < c->n; i++) if (dl_sys_on(DLK_VIDEOS, c->s[i].folder)) n += c->s[i].vid_need[w];
		bytes_text(n * (c ? c->mb_video : 2.5) * 1e6, sz, sizeof sz);
		snprintf(x->val[w], sizeof x->val[w], "%d \xc2\xb7 %s", n, sz);
		rows[w] = (menu_row){ T(FOR[w]), x->val[w], true };
	}
	return SNAPS_FOR_COUNT;
}

static menu_result vfor_key(app *a, void *ctx, in_button key, int sel)
{
	(void)a;
	(void)ctx;
	if (key != IN_ACCEPT || sel < 0 || sel >= SNAPS_FOR_COUNT) return MENU_STAY;
	snaps_set_for(sel);
	return MENU_DONE;
}

static void videos_for(dl_view *v)
{
	static vfor_ctx x;

	memset(&x, 0, sizeof x);
	menu_run(v->a, &(menu_style){ .accent = MENU_ZYPEROS, .fixed_w = menu_std_width(v->a), .start_sel = snaps_for() },
	         vfor_build, vfor_key, &x);
}

/* RetroAchievements' account: who is signed in, a sign-in again, and a
 * sign-out that asks first (there was none before, anywhere) */
enum { RA_WHO, RA_AGAIN, RA_OUT, RA_ROWS };
typedef struct { char who[96]; bool done; } ra_ctx;

static int ra_build(void *ctx, menu_row *rows, int max, const char **heading)
{
	ra_ctx *x = ctx;

	(void)max;
	*heading = "RetroAchievements";
	if (!g_fx_user && !ra_signed_in()) {
		rows[0] = (menu_row){ T(S_SYSMENU_SIGN_IN), NULL, true };
		return 1;
	}
	snprintf(x->who, sizeof x->who, T(S_DOWNLOADS_SIGNED_IN_AS), g_fx_user ? g_fx_user : ra_user());
	rows[RA_WHO] = (menu_row){ x->who, NULL, false };
	rows[RA_AGAIN] = (menu_row){ T(S_DOWNLOADS_SIGN_IN_AGAIN), NULL, true };
	rows[RA_OUT] = (menu_row){ T(S_DOWNLOADS_SIGN_OUT), NULL, true };
	return RA_ROWS;
}

static menu_result ra_key(app *a, void *ctx, in_button key, int sel)
{
	(void)ctx;
	if (key != IN_ACCEPT) return MENU_STAY;
	if (!ra_signed_in()) {
		ra_signin_screen(a);
		return MENU_CHANGED;
	}
	if (sel == RA_AGAIN) {
		ra_signin_screen(a);
		return MENU_CHANGED;
	}
	if (sel == RA_OUT && confirm_panel(a, T(S_DOWNLOADS_SIGN_OUT_ASK), T(S_DOWNLOADS_SIGN_OUT_BODY), T(S_DOWNLOADS_SIGN_OUT))) {
		ra_sign_out();
		return MENU_CHANGED;
	}
	return MENU_STAY;
}

static void ra_account(dl_view *v)
{
	static ra_ctx x;

	memset(&x, 0, sizeof x);
	menu_run(v->a, &(menu_style){ .accent = MENU_ZYPEROS, .fixed_w = menu_std_width(v->a), .start_sel = 1 },
	         ra_build, ra_key, &x);
}

/* On the Card: each kind's room, and a delete that asks first for the kinds
 * a download can get again */
typedef struct { char val[DLK_KINDS][32]; } room_ctx;

static int room_build(void *ctx, menu_row *rows, int max, const char **heading)
{
	room_ctx *x = ctx;
	const dl_counts *c = dl_count_shown(NULL);
	int k;

	(void)max;
	*heading = T(S_DOWNLOADS_TILE_ON_CARD);
	for (k = 0; k < DLK_KINDS; k++) {
		dl_sum s = { 0 };

		if (c) dl_sum_of(c, (dl_kind)k, &s);
		if (k == DLK_NAMES) snprintf(x->val[k], sizeof x->val[k], "%s", T(S_DOWNLOADS_UNDER_1MB));
		else bytes_text((double)s.bytes, x->val[k], sizeof x->val[k]);
		rows[k] = (menu_row){ kind_name((dl_kind)k), x->val[k], true };
	}
	rows[DLK_KINDS] = MENU_RULE;
	rows[DLK_KINDS + 1] = (menu_row){ T(S_DOWNLOADS_STORAGE_TOO), MENU_NOTE_MARK, false };
	return DLK_KINDS + 2;
}

static void delete_sets(const dl_counts *c)
{
	char dir[LIB_PATH * 2], p[LIB_PATH * 3];
	int i, j, n;
	char **v;

	for (i = 0; c && i < c->n; i++) {
		if (!c->s[i].ach_sys) continue;
		snprintf(dir, sizeof dir, "%s/%s/.cheevos", P_ROMS, c->s[i].folder);
		v = list_dir(dir, &n);
		for (j = 0; j < n; j++) {
			size_t l = strlen(v[j]);

			/* the sets and the nones; the hashes stay, so a download does
			 * not hash the card again */
			if ((l > 4 && !strcmp(v[j] + l - 4, ".set")) || (l > 5 && !strcmp(v[j] + l - 5, ".none"))) {
				snprintf(p, sizeof p, "%s/%s", dir, v[j]);
				remove(p);
			}
		}
		list_free(v, n);
	}
}

static menu_result room_key(app *a, void *ctx, in_button key, int sel)
{
	const dl_counts *c = dl_count_shown(NULL);
	char body[192], sz[24];
	dl_sum s = { 0 };

	(void)ctx;
	if (key != IN_ACCEPT || sel < 0 || sel >= DLK_KINDS) return MENU_STAY;
	if (sel == DLK_COVERS || sel == DLK_NAMES) {
		result_message(a, kind_name((dl_kind)sel), sel == DLK_COVERS ? T(S_DOWNLOADS_COVER_WHERE) : T(S_DOWNLOADS_NAMES_STAY));
		return MENU_STAY;
	}
	if (c) dl_sum_of(c, (dl_kind)sel, &s);
	if (!s.have) return MENU_FAILED;
	bytes_text((double)s.bytes, sz, sizeof sz);
	if (sel == DLK_VIDEOS) {
		snprintf(body, sizeof body, T(S_DOWNLOADS_DELETE_VIDEOS_BODY), sz);
		if (confirm_panel(a, T(S_DOWNLOADS_DELETE_VIDEOS_ASK), body, T(S_DOWNLOADS_KEY_DELETE))) {
			for (int i = 0; i < c->n; i++) snaps_delete(P_ROMS, c->s[i].folder);
			dl_count_stale();
			dl_count_again(a, NULL);
			return MENU_CHANGED;
		}
	} else {
		snprintf(body, sizeof body, T(S_DOWNLOADS_DELETE_SETS_BODY), sz);
		if (confirm_panel(a, T(S_DOWNLOADS_DELETE_SETS_ASK), body, T(S_DOWNLOADS_KEY_DELETE))) {
			delete_sets(c);
			dl_count_stale();
			dl_count_again(a, NULL);
			return MENU_CHANGED;
		}
	}
	return MENU_STAY;
}

static void on_the_card(dl_view *v)
{
	static room_ctx x;

	memset(&x, 0, sizeof x);
	menu_run(v->a, &(menu_style){ .accent = MENU_ZYPEROS }, room_build, room_key, &x);
}

/* Settings (START): the three accounts, Video Snaps, Names and Details, and
 * On the Card */
enum { SET_SS, SET_RA, SET_SG, SET_RULE, SET_SNAPS, SET_NAMES, SET_ROOM, SET_ROWS };
typedef struct {
	dl_view *v;
	char ss[64], ra[64], sg[32], room[32];
	unsigned char sound[SET_ROWS];
	menu_choice choice[SET_ROWS];
	const char *onoff[2];
} set_ctx;

static int set_build(void *ctx, menu_row *rows, int max, const char **heading)
{
	set_ctx *s = ctx;
	const dl_counts *c = dl_count_shown(NULL);
	unsigned long long room = 0;
	int k;

	(void)max;
	*heading = T(S_HOME_SETTINGS);
	if (g_fx_user) {
		/* a shot's accounts, linked */
		snprintf(s->ss, sizeof s->ss, "%s", g_fx_user);
		snprintf(s->ra, sizeof s->ra, "%s", g_fx_user);
		snprintf(s->sg, sizeof s->sg, "\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2" "1a2b");
	} else {
		if (!ss_have_dev()) snprintf(s->ss, sizeof s->ss, "%s", T(S_SYSMENU_NOT_IN_BUILD));
		else if (ss_signed_in()) snprintf(s->ss, sizeof s->ss, "%s", ss_user());
		else snprintf(s->ss, sizeof s->ss, "%s", T(S_SYSMENU_SC_LINK));
		if (ra_signed_in()) snprintf(s->ra, sizeof s->ra, "%s", ra_user());
		else snprintf(s->ra, sizeof s->ra, "%s", T(S_SYSMENU_SIGN_IN));
		if (!sgdb_have_key()) snprintf(s->sg, sizeof s->sg, "%s", T(S_SYSMENU_SC_LINK));
		else if (sgdb_key_state() == SGDB_REFUSED) snprintf(s->sg, sizeof s->sg, "%s", T(S_SCRAPER_KEY_REFUSED));
		else sgdb_key_masked(s->sg, sizeof s->sg);
	}
	for (k = 0; c && k < DLK_KINDS; k++) {
		dl_sum sm;

		dl_sum_of(c, (dl_kind)k, &sm);
		room += sm.bytes;
	}
	if (c) bytes_text((double)room, s->room, sizeof s->room);
	else snprintf(s->room, sizeof s->room, "%s", T(S_DOWNLOADS_COUNTING));
	rows[SET_SS] = (menu_row){ "ScreenScraper", s->ss, true };
	rows[SET_RA] = (menu_row){ "RetroAchievements", s->ra, true };
	rows[SET_SG] = (menu_row){ "SteamGridDB", s->sg, true };
	rows[SET_RULE] = MENU_RULE;
	rows[SET_SNAPS] = (menu_row){ T(S_SNAPS_HEADING), NULL, true };
	s->onoff[0] = T(S_SCRAPER_ON);
	s->onoff[1] = T(S_SCRAPER_OFF);
	rows[SET_NAMES] = (menu_row){ T(S_SCRAPER_NAMES_DETAILS), scraper_names_on() ? s->onoff[0] : s->onoff[1], true };
	rows[SET_ROOM] = (menu_row){ T(S_DOWNLOADS_TILE_ON_CARD), s->room, true };
	memset(s->sound, 0, sizeof s->sound);
	memset(s->choice, 0, sizeof s->choice);
	s->sound[SET_NAMES] = MENU_SND_RING;
	s->choice[SET_NAMES] = (menu_choice){ .n = 2, .cur = scraper_names_on() ? 0 : 1, .labels = s->onoff };
	return SET_ROWS;
}

static menu_result set_key(app *a, void *ctx, in_button key, int sel)
{
	set_ctx *s = ctx;

	if (sel == SET_NAMES) {
		if (key != IN_LEFT && key != IN_RIGHT) return MENU_STAY;
		scraper_names_set(!scraper_names_on());
		return MENU_CHANGED;
	}
	if (key != IN_ACCEPT) return MENU_STAY;
	switch (sel) {
	case SET_SS:    ss_page(a); break;
	case SET_RA:    ra_account(s->v); break;
	case SET_SG:    sgdb_screen(a); break;
	case SET_SNAPS: snaps_screen(a); break;
	case SET_ROOM:  on_the_card(s->v); break;
	default:        return MENU_STAY;
	}
	return MENU_CHANGED;
}

static void settings(dl_view *v)
{
	static set_ctx s;

	memset(&s, 0, sizeof s);
	s.v = v;
	menu_run(v->a, &(menu_style){ .accent = MENU_ZYPEROS, .fixed_w = menu_std_width(v->a), .sound = s.sound,
	                               .choices = s.choice },
	         set_build, set_key, &s);
}

/* A game's panel: what it has of each kind; A on its cover or video opens
 * its Cover and Video page */
typedef struct {
	dl_view *v;
	const dl_game *g;
	char val[DLK_KINDS][64];
} game_ctx;

static int game_build(void *ctx, menu_row *rows, int max, const char **heading)
{
	game_ctx *x = ctx;
	const dl_game *g = x->g;
	static const str_id ROW[DLK_KINDS] = { S_DOWNLOADS_HAS_COVER, S_DOWNLOADS_HAS_VIDEO, S_SCRAPER_NAMES_DETAILS,
	                                       S_SYSMENU_CHEEVOS };
	int k;

	(void)max;
	*heading = g->title;
	for (k = 0; k < DLK_KINDS; k++) {
		switch (g->st[k]) {
		case GS_HAVE: snprintf(x->val[k], sizeof x->val[k], "%s", k == DLK_ACH ? T(S_DOWNLOADS_READY) : T(S_DOWNLOADS_ON_CARD_SHORT)); break;
		case GS_NONE: snprintf(x->val[k], sizeof x->val[k], "%s", T(S_DOWNLOADS_NONE)); break;
		case GS_OFF:  snprintf(x->val[k], sizeof x->val[k], "%s", T(S_SCRAPER_OFF)); break;
		default:      snprintf(x->val[k], sizeof x->val[k], "%s", k == DLK_ACH ? T(S_DOWNLOADS_NOT_CHECKED) : T(S_DOWNLOADS_MISSING)); break;
		}
		rows[k] = (menu_row){ T(ROW[k]), x->val[k], true };
	}
	return DLK_KINDS;
}

static menu_result game_key(app *a, void *ctx, in_button key, int sel)
{
	game_ctx *x = ctx;
	int si;

	if (key != IN_ACCEPT) return MENU_STAY;
	if (sel != DLK_COVERS && sel != DLK_VIDEOS) return MENU_STAY;
	for (si = 0; si < a->sys.count; si++) {
		const game_list *gl;

		if (strcmp(a->sys.systems[si].folder, top_of(x->v)->sys)) continue;
		gl = sys_games(&a->view[si]);
		if (x->g->gi < gl->count) media_screen(a, si, &gl->items[x->g->gi]);
		return MENU_CHANGED;
	}
	return MENU_STAY;
}

static void game_panel(dl_view *v, const dl_game *g)
{
	static game_ctx x;

	memset(&x, 0, sizeof x);
	x.v = v;
	x.g = g;
	menu_run(v->a, &(menu_style){ .accent = MENU_ZYPEROS, .fixed_w = menu_std_width(v->a) }, game_build, game_key, &x);
}

/* ---- keys ---------------------------------------------------------------------------- */

/* Up and Down on a list of `n` rows: round on a press, stopping at the ends
 * while held */
static bool move_list(app *a, int *sel, int n, bool rep_up, bool rep_dn)
{
	int to = *sel;

	if (rep_up) to = to > 0 ? to - 1 : a->in.pressed[IN_UP] ? n - 1 : to;
	if (rep_dn) to = to < n - 1 ? to + 1 : a->in.pressed[IN_DOWN] ? 0 : to;
	if (to == *sel) {
		if (rep_up || rep_dn) uisound_play(DS_SOUND_EDGE);
		return false;
	}
	*sel = to;
	uisound_play(DS_SOUND_MOVE);
	return true;
}

static int hub_row_y(int sel)
{
	return DS_PAGE_BODY_Y + sel * DL_ROW + (sel > 0 ? DL_RULE_H : 0);
}

static void open_run(dl_view *v)
{
	push(v, V_RUN);
	dlq_page(DLQ_ART, true);
	dlq_page(DLQ_RA, true);
}

static void close_run_view(void)
{
	dlq_page(DLQ_ART, false);
	dlq_page(DLQ_RA, false);
}

/* A: Download, or the run that goes on */
static void download(dl_view *v, int only)
{
	if (dl_live()) {
		uisound_play(DS_SOUND_ACCEPT);
		if (!(v->art || v->ra)) run_rejoin(v);
		open_run(v);
		return;
	}
	if (!v->c) {
		/* still counting: it starts as the count ends */
		uisound_play(DS_SOUND_ACCEPT);
		v->pending = only;
		return;
	}
	if (run_start(v, only)) {
		uisound_play(DS_SOUND_ACCEPT);
		open_run(v);
	} else {
		uisound_play(DS_SOUND_DENY);
	}
}

static void hub_keys(dl_view *v, dl_frame *f, unsigned now)
{
	app *a = v->a;
	bool up = in_repeat(&a->in, IN_UP), dn = in_repeat(&a->in, IN_DOWN);
	int from = f->sel;

	if ((up || dn) && move_list(a, &f->sel, 1 + DLK_KINDS, up, dn)) glide(f, hub_row_y(from), now);
	if ((a->in.pressed[IN_LEFT] || a->in.pressed[IN_RIGHT]) && f->sel > 0) {
		dl_kind k = (dl_kind)(f->sel - 1);

		if (!v->c || dl_kind_blocked(v->c, k)) {
			uisound_play(DS_SOUND_DENY);
		} else {
			bool in = !dl_kind_in(k);
			dl_sum s;

			dl_kind_set(k, in);
			/* In, with no system's switch on: every system on */
			dl_sum_of(v->c, k, &s);
			if (in && !s.on)
				for (int i = 0; i < v->c->n; i++) dl_sys_set(k, v->c->s[i].folder, true);
			uisound_play(DS_SOUND_MOVE);
		}
	} else if (a->in.pressed[IN_LEFT] || a->in.pressed[IN_RIGHT]) {
		uisound_play(DS_SOUND_EDGE);
	}
	if (a->in.pressed[IN_ACCEPT]) {
		if (f->sel == 0) download(v, -1);
		else if (f->sel - 1 == DLK_ACH) {
			uisound_play(DS_SOUND_ACCEPT);
			radl_screen_from_downloads(a);
			dl_count_again(a, v->only);
		} else {
			uisound_play(DS_SOUND_ACCEPT);
			push(v, V_KIND);
			top_of(v)->kind = f->sel - 1;
		}
	} else if (a->in.pressed[IN_Y]) {
		int col = f->sel > 0 ? f->sel - 1 : 0;

		uisound_play(DS_SOUND_ACCEPT);
		push(v, V_SYSTEMS);
		top_of(v)->col = col;
	} else if (a->in.pressed[IN_START]) {
		uisound_play(DS_SOUND_ACCEPT);
		settings(v);
		dl_count_again(a, v->only);
	}
}

static void kind_keys(dl_view *v, dl_frame *f)
{
	app *a = v->a;

	if (a->in.pressed[IN_ACCEPT]) download(v, f->kind);
	else if (a->in.pressed[IN_X] && f->kind == DLK_VIDEOS) {
		uisound_play(DS_SOUND_ACCEPT);
		videos_for(v);
		dl_count_again(a, v->only);
	} else if (a->in.pressed[IN_Y]) {
		int col = f->kind;

		uisound_play(DS_SOUND_ACCEPT);
		push(v, V_SYSTEMS);
		top_of(v)->col = col;
	}
}

static void systems_keys(dl_view *v, dl_frame *f)
{
	app *a = v->a;
	int n = v->c ? v->c->n : 0;
	bool up = in_repeat(&a->in, IN_UP), dn = in_repeat(&a->in, IN_DOWN);

	if (!n) return;
	if (up || dn) move_list(a, &f->row, n, up, dn);
	if (in_repeat(&a->in, IN_LEFT)) {
		if (f->col > 0) { f->col--; uisound_play(DS_SOUND_MOVE); } else uisound_play(DS_SOUND_EDGE);
	}
	if (in_repeat(&a->in, IN_RIGHT)) {
		if (f->col < DLK_KINDS - 1) { f->col++; uisound_play(DS_SOUND_MOVE); } else uisound_play(DS_SOUND_EDGE);
	}
	if (a->in.pressed[IN_ACCEPT]) {
		dl_kind k = (dl_kind)f->col;
		const dl_sys *s = &v->c->s[f->row];

		if (dl_kind_blocked(v->c, k) || (k == DLK_ACH && !s->ach_sys) || (k == DLK_NAMES && !s->names_ok)) {
			uisound_play(DS_SOUND_DENY);
		} else if (!dl_kind_in(k)) {
			/* the kind was out: in, for this system alone */
			int i;

			dl_kind_set(k, true);
			for (i = 0; i < n; i++) dl_sys_set(k, v->c->s[i].folder, i == f->row);
			uisound_play(DS_SOUND_MOVE);
		} else {
			dl_sys_set(k, s->folder, !dl_sys_on(k, s->folder));
			uisound_play(DS_SOUND_MOVE);
		}
	} else if (a->in.pressed[IN_X]) {
		const dl_sys *s = &v->c->s[f->row];
		char head[160], body[192], sz[24];

		if (f->col == DLK_VIDEOS && s->vid_have) {
			uisound_play(DS_SOUND_ACCEPT);
			bytes_text((double)s->vid_bytes, sz, sizeof sz);
			snprintf(head, sizeof head, T(S_DOWNLOADS_DELETE_SYS_VIDEOS_ASK), s->name);
			snprintf(body, sizeof body, T(S_DOWNLOADS_DELETE_VIDEOS_BODY), sz);
			if (confirm_panel(a, head, body, T(S_DOWNLOADS_KEY_DELETE))) {
				snaps_delete(P_ROMS, s->folder);
				dl_count_stale();
				dl_count_again(a, v->only);
			}
		} else if (f->col == DLK_ACH && s->ach_ready) {
			dl_counts one;

			uisound_play(DS_SOUND_ACCEPT);
			bytes_text(s->ach_ready * DL_MB_SET * 1e6, sz, sizeof sz);
			snprintf(head, sizeof head, T(S_DOWNLOADS_DELETE_SYS_SETS_ASK), s->name);
			snprintf(body, sizeof body, T(S_DOWNLOADS_DELETE_SETS_BODY), sz);
			if (confirm_panel(a, head, body, T(S_DOWNLOADS_KEY_DELETE))) {
				memset(&one, 0, sizeof one);
				one.s[0] = *s;
				one.n = 1;
				delete_sets(&one);
				dl_count_stale();
				dl_count_again(a, v->only);
			}
		} else {
			uisound_play(DS_SOUND_DENY);
		}
	} else if (a->in.pressed[IN_Y]) {
		char folder[CFG_STR];

		snprintf(folder, sizeof folder, "%s", v->c->s[f->row].folder);
		uisound_play(DS_SOUND_ACCEPT);
		push(v, V_GAMES);
		snprintf(top_of(v)->sys, sizeof top_of(v)->sys, "%s", folder);
		sysgames_load(v, folder);
	}
}

static void sysgames_keys(dl_view *v, dl_frame *f, unsigned now)
{
	app *a = v->a;
	bool up = in_repeat(&a->in, IN_UP), dn = in_repeat(&a->in, IN_DOWN);
	int from = f->sel - f->win;

	if (!v->ngames) return;
	if ((up || dn) && move_list(a, &f->sel, v->ngames, up, dn)) {
		if (f->sel >= f->win && f->sel < f->win + DL_GAME_ROWS) glide(f, DS_PAGE_BODY_Y + from * DL_ROW, now);
	}
	if (a->in.pressed[IN_ACCEPT]) {
		uisound_play(DS_SOUND_ACCEPT);
		game_panel(v, &v->games[f->sel]);
		sysgames_load(v, f->sys);
	}
}

/* ---- the page ------------------------------------------------------------------------- */

static void dl_page(app *a, const char *only)
{
	dl_view *v = calloc(1, sizeof *v);
	int base;
	unsigned beat = 0;
	bool dirty = true;

	if (!v) return;
	v->a = a;
	v->pending = -2;
	snprintf(v->only, sizeof v->only, "%s", only ? only : "");
	/* The last count shows at once; a new one goes behind it */
	v->c = dl_count_shown(v->only);
	dl_count_again(a, v->only);
	v->gen = dl_count_gen();
	push(v, V_HUB);
	/* A run behind the screens is shown where it is; one that ended while
	 * the page was closed, as done */
	if (dl_live()) {
		run_rejoin(v);
		open_run(v);
	} else if (dlq_ends(DLQ_ART) != g_shown_art || dlq_ends(DLQ_RA) != g_shown_ra) {
		v->art = dlq_ends(DLQ_ART) != g_shown_art;
		v->ra = dlq_ends(DLQ_RA) != g_shown_ra;
		v->run_kind = -1;
		run_finish(v);
		push(v, V_DONE);
	}
	base = panel_base();
	g_base_page = dl_base;
	g_base_ctx = v;
	while (!want_quit && a->running) {
		unsigned now = anim_now();
		dl_frame *f = top_of(v);
		bool anim;

		plat_input_poll(&a->in);
		muse_poll();
		aout_apply(false);
		if (a->in.quit_requested) { a->running = false; break; }
		if (a->in.pressed[IN_POWER] || idle_due(a)) {
			power_off(a);              /* the run with everything else */
			break;
		}
		if (in_repeat(&a->in, IN_VOLUP))    plat_volume_nudge(+1);
		if (in_repeat(&a->in, IN_VOLDN))    plat_volume_nudge(-1);
		if (in_repeat(&a->in, IN_BRIGHTUP)) plat_brightness_nudge(+1);
		if (in_repeat(&a->in, IN_BRIGHTDN)) plat_brightness_nudge(-1);
		for (int b = 0; b < IN_COUNT; b++) if (a->in.pressed[b] || a->in.down[b]) dirty = true;
		if (a->in.pressed[IN_MENU]) {
			menu_leaving(a);           /* a run goes on behind the screens */
			break;
		}
		/* The count goes on a system at a time, a frame's share */
		if (dl_counting()) {
			dl_count_step(a, DL_COUNT_MS);
			dirty = true;
		}
		if (dl_count_gen() != v->gen) {
			v->gen = dl_count_gen();
			v->c = dl_count_shown(v->only);
			if (f->v == V_SYSTEMS && v->c && f->row >= v->c->n) f->row = v->c->n ? v->c->n - 1 : 0;
			if (v->pending != -2 && v->c) {
				int p = v->pending;

				v->pending = -2;
				if (f->v == V_DONE) v->depth--;
				if (run_start(v, p)) open_run(v);
				else uisound_play(DS_SOUND_DENY);
				f = top_of(v);
			}
			dirty = true;
		}
		if (f->v == V_RUN) {
			dlq_tick();
			if (take_arrivals(v)) dirty = true;
			if (take_pictures(v)) dirty = true;
			if (!dl_live()) {
				run_finish(v);
				close_run_view();
				f->v = V_DONE;
				dirty = true;
			} else if (a->in.pressed[IN_BACK]) {
				/* Continue in background: the page closes, the run goes on,
				 * the status corner's ring showing it */
				uisound_play(DS_SOUND_BACK);
				close_run_view();
				break;
			} else if (a->in.pressed[IN_Y]) {
				uisound_play(DS_SOUND_BACK);
				snprintf(v->why, sizeof v->why, "%s", T(S_DOWNLOADS_YOU_STOPPED));
				dlq_stop(DLQ_ART);
				dlq_stop(DLQ_RA);
				run_finish(v);
				close_run_view();
				f->v = V_DONE;
				dirty = true;
			}
		} else if (f->v == V_DONE) {
			if (a->in.pressed[IN_BACK]) {
				uisound_play(DS_SOUND_BACK);
				if (v->depth <= 1) break;
				v->depth--;
			} else if (a->in.pressed[IN_ACCEPT]) {
				uisound_play(DS_SOUND_ACCEPT);
				if (v->end == END_SIGN_IN) {
					ra_signin_screen(a);
					dl_count_again(a, v->only);
					v->depth--;
				} else {
					/* Again: from what is missing, once the card is counted */
					v->pending = v->run_kind;
					if (!dl_counting()) dl_count_again(a, v->only);
				}
			}
		} else if (a->in.pressed[IN_BACK]) {
			uisound_play(DS_SOUND_BACK);
			if (f->v == V_GAMES) sysgames_free(v);
			if (v->depth <= 1) break;
			v->depth--;
		} else {
			switch (f->v) {
			case V_KIND:    kind_keys(v, f); break;
			case V_SYSTEMS: systems_keys(v, f); break;
			case V_GAMES:   sysgames_keys(v, f, now); break;
			default:        hub_keys(v, f, now); break;
			}
		}
		if (g_menu_closing) break;
		f = top_of(v);
		anim = (f->v == V_RUN && now - v->land_at < DL_LAND + 40) || f->fill_from >= 0 ||
		       (f->v == V_RUN && v->rp.state != RADL_WORKING);
		if (now - beat >= 1000) { beat = now; dirty = true; }
		if (!dirty && !anim) {
			plat_input_wait(f->v == V_RUN || dl_counting() ? 16 : 80);
			continue;
		}
		dirty = false;
		tick_tint(a);
		tick_theme(a);
		dl_draw(v, now);
		panel_leave_draw(a, 0);
		plat_draw_osd(a->r);
		plat_present();
		ui_frame_rest(8);
	}
	close_run_view();
	mm_close(base, anim_now(), 0, g_menu_closing);
	ring_free(v);
	icons_free(v);
	sysgames_free(v);
	g_base_page = NULL;
	g_base_ctx = NULL;
	free(v);
}

menu_result downloads_screen(app *a)
{
	dl_page(a, NULL);
	return MENU_CHANGED;
}

void downloads_page(app *a, const char *only)
{
	dl_page(a, only);
}

bool downloads_kind_in(const char *kind)
{
	if (!strcmp(kind, "covers")) return dl_kind_in(DLK_COVERS);
	if (!strcmp(kind, "videos")) return dl_kind_in(DLK_VIDEOS);
	if (!strcmp(kind, "ach")) return dl_kind_in(DLK_ACH);
	return dl_kind_in(DLK_NAMES);
}

/* MENU's row: how far a run behind the screens has got, else what the last
 * count left to fetch; NULL before any count */
const char *downloads_menu_value(const char *only, char *out, size_t n)
{
	const dl_counts *c = dl_count_shown(only);
	dl_plan pl;

	if (dl_live()) { snprintf(out, n, T(S_DOWNLOADS_RUNNING), run_pct()); return out; }
	if (!c) return NULL;
	if (!online()) { snprintf(out, n, "%s", T(S_MENU_NEEDS_WIFI)); return out; }
	dl_plan_of(c, -1, &pl);
	if (pl.n) snprintf(out, n, TN(S_DOWNLOADS_TO_FETCH_N, pl.n), pl.n);
	else snprintf(out, n, "%s", T(S_SCRAPER_COMPLETE));
	return out;
}

/* The news on the shelf when a run ended behind the screens: false when
 * there is none to say */
bool downloads_news(char *l1, size_t n1, char *l2, size_t n2)
{
	static bool was;
	art_progress ap;
	radl_progress rp;
	bool art, ra, busy = dlq_busy();
	char parts[2][48];
	int np = 0;

	if (busy) { was = true; return false; }
	art = dlq_ends(DLQ_ART) != g_shown_art;
	ra = dlq_ends(DLQ_RA) != g_shown_ra;
	if (!was || (!art && !ra)) return false;
	was = false;
	if ((art && dlq_last(DLQ_ART) != DLQ_ENDED) || (ra && dlq_last(DLQ_RA) != DLQ_ENDED)) return false;
	scraper_last(&ap);
	radl_last(&rp);
	if (art && ap.found && np < 2) snprintf(parts[np++], sizeof parts[0], TN(S_DOWNLOADS_N_COVERS, ap.found), ap.found);
	if (ra && rp.fetched + rp.updated && np < 2)
		snprintf(parts[np++], sizeof parts[0], TN(S_DOWNLOADS_N_SETS, rp.fetched + rp.updated), rp.fetched + rp.updated);
	if (art && ap.snaps && np < 2) snprintf(parts[np++], sizeof parts[0], TN(S_DOWNLOADS_N_VIDEOS, ap.snaps), ap.snaps);
	if (art && ap.named && np < 2) snprintf(parts[np++], sizeof parts[0], TN(S_DOWNLOADS_N_NAMES, ap.named), ap.named);
	snprintf(l1, n1, "%s", T(S_DOWNLOADS_DONE_NEWS));
	if (np == 2) snprintf(l2, n2, "%s \xc2\xb7 %s", parts[0], parts[1]);
	else if (np == 1) snprintf(l2, n2, "%s", parts[0]);
	else snprintf(l2, n2, "%s", T(S_DOWNLOADS_OFFLINE_NOW));
	return true;
}

/* ---- --rows-report: the panels' rows ---------------------------------------------------- */

void downloads_rows_report(app *a)
{
	static set_ctx st;
	menu_row rows[SET_ROWS];
	const char *h;
	int n;

	memset(&st, 0, sizeof st);
	(void)a;
	n = set_build(&st, rows, SET_ROWS, &h);
	menu_rows_print("downloads-settings", rows, n, st.sound, st.choice);
}

/* ---- a shot (--downloads STATE) ----------------------------------------------------------
 * The page on a stand-in library, nothing read (the lab's: 640 games on 9
 * systems):
 *   0 the hub          1 a run going on (the hub)  2 every kind done
 *   3 no network, nobody signed in or linked        4 Settings
 *   5 counting, nothing counted yet                 6 the hub on Covers
 *   7 Covers out (Left)                             8 Covers' page
 *   9 Videos' page    10 Names and Details' page   11 Systems
 *  12 a system's games 13 a game's panel           14 the run
 *  15 the run waiting for Wi-Fi                     16 the run paused
 *  17 done            18 stopped                   19 On the Card
 *  20 RetroAchievements' account                    21 a console's page
 *  22 Videos For */
int shot_downloads = -1;

static void fixture(dl_counts *c, int st)
{
	static const struct {
		const char *folder, *name, *tag;
		int games, cov, names, vneed, vhave, ach_todo, ach_ready, ach_none;
		bool vid;
	} SYS[9] = {
		{ "NES", "NES", "NES", 96, 3, 30, 29, 0, 20, 68, 8, false },
		{ "SNES", "SNES", "SFC", 88, 5, 18, 22, 70, 18, 64, 6, true },
		{ "Game Boy", "Game Boy", "GB", 64, 2, 25, 19, 0, 12, 46, 6, false },
		{ "Game Boy Color", "Game Boy Color", "GBC", 52, 4, 12, 16, 0, 10, 38, 4, false },
		{ "Game Boy Advance", "Game Boy Advance", "GBA", 110, 9, 40, 25, 85, 30, 72, 8, true },
		{ "Genesis", "Genesis", "MD", 72, 6, 20, 16, 55, 15, 52, 5, true },
		{ "PlayStation", "PlayStation", "PS1", 58, 7, 24, 17, 0, 20, 30, 8, false },
		{ "Nintendo 64", "Nintendo 64", "N64", 40, 1, 10, 12, 0, 10, 26, 4, false },
		{ "PC Engine", "PC Engine", "PCE", 60, 4, 20, 18, 0, 15, 34, 11, false },
	};
	int i, k, w;
	bool done = st == 2;

	memset(c, 0, sizeof *c);
	c->free = 11200000000ULL;
	c->mb_cover = 0.25;
	c->mb_video = 2.5;
	c->ss_in = st != 3;
	c->ra_in = st != 3;
	for (k = 0; k < DLK_KINDS; k++) c->fx_in[k] = true;
	if (st == 7) c->fx_in[DLK_COVERS] = false;
	for (i = 0; i < 9; i++) {
		dl_sys *s = &c->s[c->n++];

		snprintf(s->folder, sizeof s->folder, "%s", SYS[i].folder);
		snprintf(s->name, sizeof s->name, "%s", SYS[i].name);
		snprintf(s->tag, sizeof s->tag, "%s", SYS[i].tag);
		snprintf(s->key, sizeof s->key, "%s", home_art_key(SYS[i].tag));
		s->games = SYS[i].games;
		s->cov_miss = done ? 0 : SYS[i].cov;
		for (w = 0; w < SNAPS_FOR_COUNT; w++)
			s->vid_need[w] = done ? 0 : w == SNAPS_FOR_ALL ? SYS[i].games - SYS[i].vhave
			               : w == SNAPS_FOR_PLAYED ? SYS[i].vneed * 19 / 10 : SYS[i].vneed;
		s->vid_have = SYS[i].vhave;
		s->vid_bytes = (unsigned long long)(SYS[i].vhave * 2.5e6);
		s->names_todo = done ? 0 : SYS[i].names;
		s->names_have = SYS[i].games - SYS[i].names - SYS[i].cov;
		s->names_ok = true;
		s->ach_sys = true;
		s->ach_todo = done ? 0 : SYS[i].ach_todo;
		s->ach_ready = SYS[i].ach_ready + (done ? SYS[i].ach_todo : 0);
		s->ach_none = SYS[i].ach_none;
		s->ach_games = s->ach_todo + s->ach_ready + s->ach_none;
		s->fx_on[DLK_COVERS] = s->fx_on[DLK_NAMES] = s->fx_on[DLK_ACH] = true;
		s->fx_on[DLK_VIDEOS] = SYS[i].vid;
	}
	if (st == 21) {
		/* a console's page: SNES alone */
		c->s[0] = c->s[1];
		c->n = 1;
	}
}

static const char *const SHOT_GAMES[] = { "Kingdom Run", "Pocket Quest", "Night Drive", "Moon Base", "Sky Fort",
                                          "Turbo Tails", "Crystal Bay", "Iron Comet", "Lucky Lantern",
                                          "Pixel Knights", "Star Miner", "Frog Harbor", "Neon Alley", "Rocket Pals" };

void downloads_shot(app *a)
{
	static dl_counts c;
	static dl_view v;
	int st = shot_downloads, i;
	unsigned now = 100000;

	fixture(&c, st);
	dl_count_fixture(st == 5 ? NULL : &c);
	g_fx_net = st != 3;
	g_fx_art = g_fx_ra = st == 1 || (st >= 14 && st <= 16);
	g_fx_pct = st == 1 ? 41 : -1;
	g_fx_user = st == 3 ? NULL : "tarik";
	if (st == 20) g_fx_user = "tarik";
	memset(&v, 0, sizeof v);
	v.a = a;
	v.fixture = true;
	v.c = st == 5 ? NULL : &c;
	v.pending = -2;
	push(&v, V_HUB);
	if (st == 6 || st == 7) top_of(&v)->sel = 1;
	if (st == 21) snprintf(v.only, sizeof v.only, "SNES");
	if (st == 8 || st == 9 || st == 10) {
		push(&v, V_KIND);
		top_of(&v)->kind = st == 8 ? DLK_COVERS : st == 9 ? DLK_VIDEOS : DLK_NAMES;
	}
	if (st == 11) {
		push(&v, V_SYSTEMS);
		top_of(&v)->col = DLK_NAMES;
	}
	if (st == 12 || st == 13) {
		push(&v, V_SYSTEMS);
		push(&v, V_GAMES);
		snprintf(top_of(&v)->sys, sizeof top_of(&v)->sys, "NES");
		v.games = calloc(14, sizeof *v.games);
		for (i = 0; v.games && i < 14; i++) {
			dl_game *g = &v.games[v.ngames++];

			snprintf(g->title, sizeof g->title, "%s", SHOT_GAMES[i]);
			g->st[DLK_COVERS] = i < 3 ? GS_MISS : GS_HAVE;
			g->st[DLK_VIDEOS] = GS_OFF;
			g->st[DLK_NAMES] = i % 3 ? GS_HAVE : GS_MISS;
			g->st[DLK_ACH] = i % 7 == 6 ? GS_NONE : i < 4 ? GS_MISS : GS_HAVE;
		}
	}
	if (st == 1 || (st >= 14 && st <= 18)) {
		v.art = v.ra = true;
		v.art_total = 41;
		v.art_looks = 262;
		v.ap.looked = 33;
		v.ap.found = 16;
		v.ap.named = 12;
		snprintf(v.ap.now, sizeof v.ap.now, "Retro Racer X");
		snprintf(v.ap.folder_now, sizeof v.ap.folder_now, "Game Boy Advance");
		v.ap.src = ART_SRC_SS;
		v.rp.games = 150;
		v.rp.games_done = 22;
		v.rp.fetched = 22;
		v.rp.state = st == 15 ? RADL_NO_NETWORK : st == 16 ? RADL_SLOWED : RADL_WORKING;
		v.rp.resume_at = plat_now_ms() + 42000;
		v.run_kind = -1;
		v.t0 = now;
		for (i = 6; i >= 0; i--)
			ring_add(&v, i % 3 == 0 ? DLK_ACH : i % 3 == 1 ? DLK_COVERS : DLK_NAMES, NULL, SHOT_GAMES[i], i % 3 ? 0 : 40 + i * 13);
		v.land_at = now - DL_LAND;
	}
	if (st >= 14 && st <= 16) push(&v, V_RUN);
	if (st == 17 || st == 18) {
		for (i = 13; i >= 7; i--) ring_add(&v, i % 4 == 0 ? DLK_VIDEOS : i % 3 == 0 ? DLK_ACH : DLK_COVERS, NULL, SHOT_GAMES[i], 0);
		v.end = st == 17 ? END_DONE : END_STOPPED;
		if (st == 18) snprintf(v.why, sizeof v.why, "%s", T(S_DOWNLOADS_YOU_STOPPED));
		v.ap.found = 41;
		v.ap.snaps = 63;
		v.ap.named = 199;
		v.rp.fetched = 137;
		push(&v, V_DONE);
	}
	g_base_page = dl_base;
	g_base_ctx = &v;
	if (st == 4 || st == 19 || st == 20 || st == 22 || st == 13) {
		menu_row rows[16];
		const char *h;
		int n = 0, sel = 0;

		dl_draw(&v, now);
		if (st == 4) {
			static set_ctx s;

			memset(&s, 0, sizeof s);
			n = set_build(&s, rows, SET_ROWS, &h);
		} else if (st == 19) {
			static room_ctx r;

			memset(&r, 0, sizeof r);
			n = room_build(&r, rows, 16, &h);
		} else if (st == 20) {
			static ra_ctx r;

			memset(&r, 0, sizeof r);
			n = ra_build(&r, rows, 16, &h);
			sel = n > 1 ? 1 : 0;
		} else if (st == 22) {
			static vfor_ctx r;

			memset(&r, 0, sizeof r);
			n = vfor_build(&r, rows, 16, &h);
			sel = 1;
		} else {
			static game_ctx g;

			memset(&g, 0, sizeof g);
			g.v = &v;
			g.g = &v.games[0];
			n = game_build(&g, rows, 16, &h);
		}
		ui_scrim(a->r, 0);
		menu_draw(a, h, rows, n, sel, st == 19 ? 0 : menu_std_width(a), MENU_ZYPEROS);
	} else {
		dl_draw(&v, now);
	}
	g_base_page = NULL;
	g_base_ctx = NULL;
	dl_count_fixture(NULL);
	g_fx_net = g_fx_art = g_fx_ra = g_fx_pct = -1;
	g_fx_user = NULL;
	ring_free(&v);
	icons_free(&v);
	sysgames_free(&v);
}
