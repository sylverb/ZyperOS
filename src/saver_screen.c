/* SPDX-License-Identifier: GPL-3.0-only */
#include "saver_screen.h"

#include "app.h"
#include "ctl_channel.h"
#include "mp4video.h"
#include "musec.h"
#include "panel.h"
#include "randgame.h"
#include "saver.h"
#include "shelfbuild.h"
#include "snaps.h"
#include "snapview.h"
#include "taskhold.h"
#include "ui.h"
#include "uisound.h"
#include "zyperdraw.h"

#include <SDL.h>
#include <SDL_image.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

/* ---------- the screensaver (src/saver.h; DESIGN.md 11.37) ---------------- *
 *
 * A MODE OF THE SHELF'S LOOP, not a screen of its own: the loop goes on
 * doing everything it does (the power button and Auto Off, the dark screen on
 * the cable, a computer plugged in, px2-open's commands), and where it would
 * draw the shelf it draws this. So the screensaver never has to remember to
 * be a good citizen, and cannot outlive what ends the shelf: idle_due is
 * asked before saver_pass on every pass.
 *
 * What it plays is the video snaps the scraper keeps (src/snaps.c), one at a
 * time through the same daemon thread the shelf's focused cover uses
 * (snap_ask), muted always; with no snap on the card, the covers, one drawn
 * frame each; with neither, nothing, and it does not try again until a button
 * is pressed. */

static struct {
	bool         on;
	bool         video;              /* the pool is of snaps; else of covers */
	unsigned     started, last_pass, last_draw;
	saver_start  st;                 /* when it starts: lives while it is off too */
	saver_show   show;
	saver_recent recent;
	int         *shelf, *game;       /* the pool: a game on a shelf with a snap */
	int          n, cap;
	int          at;                 /* the pool entry on screen */
	char         title[LIB_NAME], sysname[CFG_STR];
	unsigned     accent;
	SDL_Texture *cover;
	int          cw, ch;
} g_sv;

static void saver_pool_free(void)
{
	free(g_sv.shelf);
	free(g_sv.game);
	g_sv.shelf = g_sv.game = NULL;
	g_sv.n = g_sv.cap = 0;
}

static bool saver_pool_add(int shelf, int game)
{
	if (g_sv.n == g_sv.cap) {
		int cap = g_sv.cap ? g_sv.cap * 2 : 256;
		int *s = realloc(g_sv.shelf, (size_t)cap * sizeof *s);
		int *g;

		if (!s) return false;
		g_sv.shelf = s;
		if (!(g = realloc(g_sv.game, (size_t)cap * sizeof *g))) return false;
		g_sv.game = g;
		g_sv.cap = cap;
	}
	g_sv.shelf[g_sv.n] = shelf;
	g_sv.game[g_sv.n++] = game;
	return true;
}

/* A console's covers by stem, sorted, as snap_names_for lists its snaps: one
 * listing of its .media. NULL, and 0, when there are none. The caller frees. */
static char **saver_cover_stems(const char *folder, int *count)
{
	char dir[CFG_STR * 4], **v = NULL;
	struct dirent *e;
	int n = 0, cap = 0;
	DIR *d;

	*count = 0;
	snprintf(dir, sizeof dir, "%s/%s/.media", P_ROMS, folder);
	if (!(d = opendir(dir))) return NULL;
	while ((e = readdir(d))) {
		size_t len = strlen(e->d_name);
		char *stem;

		if (e->d_name[0] == '.' || len < 5 || strcasecmp(e->d_name + len - 4, ".png")) continue;
		if (n == cap) {
			char **w = realloc(v, (size_t)(cap = cap ? cap * 2 : 64) * sizeof *w);

			if (!w) break;
			v = w;
		}
		if (!(stem = strndup(e->d_name, len - 4))) break;
		v[n++] = stem;
	}
	closedir(d);
	if (n) qsort(v, (size_t)n, sizeof *v, snap_name_cmp);
	*count = n;
	return v;
}

/* The games it may show: those Random Game picks from every system (not Muse's
 * albums, not Favorites' copies, not a Ports shelf with only PortMaster's
 * offer) that have a snap on the card and on for their console with `video`
 * (snap_names_for), else a cover. One listing of each console's .media, here
 * and once, as the screensaver begins. */
static void saver_pool_build(app *a, bool video)
{
	int i, j;

	saver_pool_free();
	for (i = 0; i < a->sys.count; i++) {
		const system_cfg *s = &a->sys.systems[i];
		/* Every game of the system, in every folder (sys_games): the index
		 * is pick_launch's, which opens the game's folder */
		const game_list *gl = sys_games(&a->view[i]);
		char **stems = NULL;
		int n = 0;

		if (is_muse(s) || a->view[i].owner || pm_only_offer(a, i) || !s->folder[0]) continue;
		if (video) {
			const snap_names *sn = snap_names_for(s->folder);

			if (!sn->on || !sn->n) continue;
			stems = sn->stems;
			n = sn->n;
		} else {
			stems = saver_cover_stems(s->folder, &n);
			if (!n) continue;
		}
		for (j = 0; j < gl->count; j++) {
			const char *name = gl->items[j].name;

			if (bsearch(&name, stems, (size_t)n, sizeof *stems, snap_name_cmp) &&
			    !saver_pool_add(i, j))
				break;
		}
		/* snap_names_for's lists are its own, kept; the covers' are ours */
		if (!video) {
			while (n > 0) free(stems[--n]);
			free(stems);
		}
	}
}

/* A cover as a texture, through the surface texload makes one from: ARGB8888,
 * the one format every GPU path of the launcher takes (a texture straight
 * from IMG_LoadTexture of an RGB file drew nothing on the Pixel 2's GL). */
static SDL_Texture *saver_cover_load(SDL_Renderer *r, const char *path, int *w, int *h)
{
	SDL_Surface *raw = IMG_Load(path), *conv;
	SDL_Texture *t;

	if (!raw) return NULL;
	conv = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ARGB8888, 0);
	SDL_FreeSurface(raw);
	if (!conv) return NULL;
	t = SDL_CreateTextureFromSurface(r, conv);
	*w = conv->w;
	*h = conv->h;
	SDL_FreeSurface(conv);
	if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
	return t;
}

/* The next one: picked, checked to be on the card, and asked for. False when
 * none could be found. */
static bool saver_next(app *a, unsigned now)
{
	int tries = 6, k;

	if (g_sv.cover) { SDL_DestroyTexture(g_sv.cover); g_sv.cover = NULL; }
	for (k = 0; k < tries; k++) {
		int i = saver_pick(g_sv.n, rg_random(), &g_sv.recent);
		const system_cfg *s;
		const game_entry *g;
		char path[LIB_PATH * 2];
		struct stat st;

		if (i < 0) return false;
		saver_recent_add(&g_sv.recent, i);
		if (g_sv.shelf[i] >= a->sys.count || g_sv.game[i] >= sys_games(&a->view[g_sv.shelf[i]])->count)
			continue;
		s = &a->sys.systems[g_sv.shelf[i]];
		g = &sys_games(&a->view[g_sv.shelf[i]])->items[g_sv.game[i]];
		if (g_sv.video) {
			if (!snaps_path(P_ROMS, s->folder, g->name, path, sizeof path) ||
			    stat(path, &st) != 0 || st.st_size <= 0)
				continue;
		} else {
			box_art_path(s->folder, g->name, path, sizeof path);
			if (stat(path, &st) != 0 || st.st_size <= 0) continue;
			if (!(g_sv.cover = saver_cover_load(a->r, path, &g_sv.cw, &g_sv.ch))) continue;
		}
		g_sv.at = i;
		snprintf(g_sv.title, sizeof g_sv.title, "%s", g->title[0] ? g->title : g->name);
		snprintf(g_sv.sysname, sizeof g_sv.sysname, "%s", s->name);
		g_sv.accent = s->accent;
		saver_show_begin(&g_sv.show, now, !g_sv.video);
		if (g_sv.video) {
			/* Muted, whatever the setting says for the shelf: a screensaver
			 * is never a sound that nobody asked for */
			g_snap_force_vol = 0;
			snap_ask(path, false);
			g_snap_force_vol = -1;
		}
		fprintf(stderr, "saver: %s/%s (%s)\n", s->folder, g->file, g_sv.video ? "video" : "cover");
		return true;
	}
	return false;
}

void saver_stop(app *a, const char *why)
{
	(void)a;
	if (!g_sv.on) return;
	g_sv.on = false;
	/* The daemon stops that same frame, and the picture is let go */
	snap_off();
	if (g_sv.cover) { SDL_DestroyTexture(g_sv.cover); g_sv.cover = NULL; }
	saver_pool_free();
	fprintf(stderr, "saver: ended, %s (%u s)\n", why, (plat_now_ms() - g_sv.started) / 1000);
	redraw_now();
}

static bool saver_begin(app *a, unsigned now)
{
	/* The shelf's own snap gives way, and the first listing is of snaps */
	snap_off();
	g_sv.video = true;
	saver_pool_build(a, true);
	if (g_sv.n == 0) {
		g_sv.video = false;
		saver_pool_build(a, false);
	}
	saver_recent_clear(&g_sv.recent);
	g_sv.started = g_sv.last_draw = now;
	if (g_sv.n == 0 || !saver_next(a, now)) {
		fprintf(stderr, "saver: nothing to show (no snaps, no covers); waits for a button\n");
		saver_pool_free();
		saver_spend(&g_sv.st);
		return false;
	}
	fprintf(stderr, "saver: starts, %d %s\n", g_sv.n, g_sv.video ? "games with a video" : "games (no snaps on the card: covers)");
	g_sv.on = true;
	return true;
}

/* The picture's rectangle: `w` by `h` as large as `box` takes, centered */
static SDL_FRect saver_fit(const SDL_Rect *box, int w, int h, float most)
{
	float k = fminf((float)box->w / w, (float)box->h / h);

	if (k > most) k = most;
	return (SDL_FRect){ box->x + (box->w - w * k) / 2, box->y + (box->h - h * k) / 2, w * k, h * k };
}

/* One frame, `alpha` of the clip faded up. The picture on the ink both
 * themes keep for content (MP4's full view does too), and the game's name and
 * system and the two keys on slabs of the theme's surface at 85%, the notice's
 * (DESIGN.md 11.7), so they read on any picture. */
static void saver_draw(app *a, float alpha)
{
	SDL_Renderer *r = a->r;
	const ds_rgb ink = DS_THEME[DS_DARK].ink;
	const int margin = 48, padx = 28, pady = 14, gap = 12;
	TTF_Font *fl = ui_font(UI_F_LABEL), *fm = ui_font(UI_F_META), *fc = page_font(PAGE_CHROME);
	SDL_Texture *t = NULL;
	SDL_Rect src = { 0, 0, 0, 0 }, box = { 0, 24, ZYPEROS_SCREEN_W, 552 };
	int w = 0, h = 0;
	Uint8 al = (Uint8)(255 * (alpha < 0 ? 0 : alpha > 1 ? 1 : alpha) + 0.5f);

	SDL_SetRenderDrawColor(r, ink.r, ink.g, ink.b, 255);
	SDL_RenderClear(r);
	SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
	if (g_sv.video) {
		t = snapv_texture(&w, &h, &src);
		/* A host shot has no daemon: a screenshot it is given, or the test
		 * picture MP4's shots use */
		if (g_shot_saver >= 0) {
			static SDL_Texture *pic;

			if (g_shot_saver_pic && !pic) pic = IMG_LoadTexture(r, g_shot_saver_pic);
			if (pic) {
				t = pic;
				SDL_QueryTexture(pic, NULL, NULL, &w, &h);
			} else {
				if (!mp4v_texture()) mp4v_test_picture(r, 640, 480);
				t = mp4v_texture();
				w = 640; h = 480;
			}
			src = (SDL_Rect){ 0, 0, 0, 0 };
		}
		if (t && w > 0 && h > 0) {
			/* The whole screen, in its own shape, never stretched */
			SDL_FRect d = saver_fit(&(SDL_Rect){ 0, 0, ZYPEROS_SCREEN_W, ZYPEROS_SCREEN_H }, w, h, 8.0f);

			SDL_RenderCopyF(r, t, src.w > 0 ? &src : NULL, &d);
		}
	} else if (g_sv.cover && g_sv.cw > 0 && g_sv.ch > 0) {
		/* A cover is small: not blown up past a third more, on a ground of
		 * its own, with the foot's room left under it */
		SDL_FRect d = saver_fit(&box, g_sv.cw, g_sv.ch, 1.34f);

		SDL_RenderCopyF(r, g_sv.cover, NULL, &d);
	}
	(void)t;
	/* Fading up from black, and to it */
	if (al < 255) {
		SDL_SetRenderDrawColor(r, ink.r, ink.g, ink.b, (Uint8)(255 - al));
		SDL_RenderFillRect(r, NULL);
	}
	if (al > 0) {
		const SDL_Color text = ui_fade(UI_TEXT, alpha), dim = ui_fade(UI_TEXT_DIM, alpha);
		const SDL_Color slab = ui_col(theme()->surface, (Uint8)(al * 0.85f + 0.5f));
		const char *back = T(S_SAVER_BACK), *play = T(S_GAMES_PLAY);
		int hint_w = page_hint_width("START", play, false);
		int back_w = ui_text_width(fc, back);
		int rw = (hint_w > back_w ? hint_w : back_w) + 2 * padx;
		int line_m = ui_font_line(UI_F_META), line_l = ui_font_line(UI_F_LABEL);
		int sh = pady * 2 + line_m + gap + line_l;           /* the game slab's height */
		int top = ZYPEROS_SCREEN_H - margin - sh;
		int maxw = ZYPEROS_SCREEN_W - 2 * margin;
		int kh = pady * 2 + 40 + 44;                          /* the keys slab's height */
		int dot = 14, dgap = 14, tw, sw, lw, x;
		unsigned mark = ui_sys_mark(g_sv.accent);
		char fit_t[LIB_NAME], fit_s[CFG_STR];
		SDL_Rect q;

		ui_fit_text(fl, g_sv.title, fit_t, sizeof fit_t, maxw - 2 * padx);
		ui_fit_text(fm, g_sv.sysname, fit_s, sizeof fit_s, maxw - 2 * padx - dot - dgap);
		tw = ui_text_width(fl, fit_t);
		sw = dot + dgap + ui_text_width(fm, fit_s);
		lw = (tw > sw ? tw : sw) + 2 * padx;
		/* The game, at the foot: its system in the quiet line over its name,
		 * as a notice says its heading over its body */
		q = (SDL_Rect){ margin, top, lw, sh };
		ui_round_rect(r, &q, DS_RADIUS_PANEL < sh / 2 ? DS_RADIUS_PANEL : sh / 2, slab);
		x = q.x + padx;
		ui_round_rect(r, &(SDL_Rect){ x, top + pady + line_m / 2 - dot / 2, dot, dot }, dot / 2,
		              (SDL_Color){ (Uint8)(mark >> 16), (Uint8)(mark >> 8), (Uint8)mark, al });
		ui_text(r, fm, fit_s, x + dot + dgap, ui_line_y(UI_F_META, top + pady), -1, dim);
		ui_text(r, fl, fit_t, x, ui_line_y(UI_F_LABEL, top + pady + line_m + gap), -1, text);
		/* The keys, at the top's right where they cover little: START plays
		 * it, anything else goes back */
		q = (SDL_Rect){ ZYPEROS_SCREEN_W - margin - rw, margin, rw, kh };
		ui_round_rect(r, &q, DS_RADIUS_PANEL < kh / 2 ? DS_RADIUS_PANEL : kh / 2, slab);
		page_hint_fade(r, "START", play, q.x + padx, q.y + pady + 20, false, false, al);
		ui_text(r, fc, back, q.x + padx, ui_cap_y(fc, q.y + pady + 20 + 44), -1, dim);
		(void)line_l;
	}
	plat_draw_osd(r);
	plat_present();
}

/* --saver [cover]: one frame of it for a host shot, the first game on the
 * first shelf that has one, a test picture for its video or its cover when
 * the card has none (there is no daemon to play a snap) */
void saver_shot(app *a)
{
	int i, j = 0;

	/* The game the shelf is on, else the first game on the first shelf */
	i = a->sys_cursor;
	if (i >= 0 && i < a->sys.count && !is_muse(&a->sys.systems[i]) && a->view[i].list.count > 0)
		j = a->view[i].cursor;
	else
		for (i = 0; i < a->sys.count; i++)
			if (!is_muse(&a->sys.systems[i]) && !a->view[i].owner && a->view[i].list.count > 0) break;
	if (i < a->sys.count) {
		const game_entry *g = &a->view[i].list.items[j < a->view[i].list.count ? j : 0];

		snprintf(g_sv.title, sizeof g_sv.title, "%s", g->title[0] ? g->title : g->name);
		snprintf(g_sv.sysname, sizeof g_sv.sysname, "%s", a->sys.systems[i].name);
		g_sv.accent = a->sys.systems[i].accent;
	}
	g_sv.video = g_shot_saver == 0;
	if (!g_sv.video && !g_sv.cover) {
		/* A file given is loaded as saver_next loads a cover */
		if (g_shot_saver_pic &&
		    (g_sv.cover = saver_cover_load(a->r, g_shot_saver_pic, &g_sv.cw, &g_sv.ch))) {
			/* loaded as saver_next loads one */
		} else {
			if (!mp4v_texture()) mp4v_test_picture(a->r, 640, 480);
			g_sv.cover = mp4v_texture();
			g_sv.cw = 640;
			g_sv.ch = 480;
		}
	}
	saver_draw(a, g_shot_saver_alpha);
	g_sv.cover = NULL;
}

/* One pass of the shelf's loop, after the power button and Auto Off have had
 * theirs. True when the screensaver took the pass (the loop goes round
 * again). */
bool saver_pass(app *a)
{
	unsigned now = plat_now_ms();
	bool any = false, press = false, busy, newpic = false, ready, draw;
	saver_end why;
	float alpha = 0;
	int pct = -1, b;
	bool charging = false;

	/* A gap: a menu, a game or a message had the screen in between, and
	 * whatever this drew is not on it any more */
	if (g_sv.on && now - g_sv.last_pass >= 500) saver_stop(a, "something else had the screen");
	for (b = 0; b < IN_COUNT && !any; b++)
		any = a->in.pressed[b] || a->in.down[b];
	if (!battery_now(&pct, &charging)) pct = -1;
	/* Not idle: music playing, File Transfer on, a long task at work */
	busy = musec_playing() || g_xs.on || long_task_busy();

	if (!g_sv.on) {
		if (!saver_due(&g_sv.st, now, g_saver_s, any, busy || saver_low_battery(pct, charging)))
			return false;
		if (!saver_begin(a, now)) return false;
	}
	g_sv.last_pass = now;

	/* Decoding video warms the chip: "Getting hot" (src/heat.h) is due, so
	 * the screensaver goes, and the shelf says it as it does on any pass
	 * (heat_watch takes it). It stays gone until a button. */
	if (g_heat.due) {
		saver_spend(&g_sv.st);
		saver_stop(a, "the chip is getting hot");
		return true;
	}
	why = saver_must_end(now, g_sv.started, busy, pct, charging);
	if (why != SAVER_GO_ON) {
		/* Ended on its own: on battery's cap or a low battery it waits for a
		 * button; a task starting only waits for the task */
		if (why != SAVER_END_HELD) saver_spend(&g_sv.st);
		saver_stop(a, why == SAVER_END_HELD ? "a long task began"
		              : why == SAVER_END_BATTERY ? "the battery is low" : "on battery for long enough");
		return true;
	}

	/* A button: START boots the game on screen, as A on its card would; the
	 * volume and brightness keys do their own work and leave it be; any
	 * other goes back to the shelf as it was, and is spent doing that */
	for (b = 0; b < IN_COUNT; b++)
		if (a->in.pressed[b] && b != IN_VOLUP && b != IN_VOLDN && b != IN_BRIGHTUP &&
		    b != IN_BRIGHTDN)
			press = true;
	if (press) {
		bool boot = a->in.pressed[IN_START] && g_sv.at >= 0 && g_sv.at < g_sv.n;
		int shelf = boot ? g_sv.shelf[g_sv.at] : -1, game = boot ? g_sv.game[g_sv.at] : -1;

		saver_stop(a, boot ? "START" : "a button");
		plat_input_flush();
		memset(&a->in, 0, sizeof a->in);
		if (boot) {
			uisound_play(DS_SOUND_ACCEPT);
			g_pick.shelf = shelf;
			g_pick.game = game;
			g_pick.hold_ms = 0;
			pick_launch(a);
		}
		return true;
	}

	if (g_sv.video) {
		newpic = snapv_update(a->r);
		ready = snapv_texture(NULL, NULL, NULL) != NULL;
	} else {
		ready = g_sv.cover != NULL;
	}
	if (saver_show_step(&g_sv.show, now, ready, &alpha)) {
		if (!saver_next(a, now)) {
			saver_spend(&g_sv.st);
			saver_stop(a, "nothing more to show");
			return true;
		}
		alpha = 0;
	}
	/* A frame when the daemon put out a new picture, while a fade moves, a
	 * moment after the volume bar, and once a second regardless (the
	 * heartbeat the shelf has): a cover or a held picture is one drawn frame */
	draw = newpic || saver_show_moving(&g_sv.show) || now - g_sv.last_draw >= 1000 ||
	       plat_osd_until() != UINT32_MAX;
	/* The housekeeping every pass of the shelf's loop does through
	 * tick_theme - a File Transfer behind the shelf, the update's daily look,
	 * the theme's hour - and a theme that turned is a frame */
	g_ctl_shelf_pass = true;
	if (tick_theme(a)) draw = true;
	g_ctl_shelf_pass = false;
	if (draw) {
		saver_draw(a, alpha);
		g_sv.last_draw = now;
	} else {
		plat_input_wait(16);
	}
	return true;
}
