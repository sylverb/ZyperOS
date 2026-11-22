/* SPDX-License-Identifier: GPL-3.0-only */
#include "link_screen.h"

#include "aout_route.h"
#include "app.h"
#include "fileio.h"
#include "hotspot.h"
#include "ingame_screen.h"
#include "keyhints.h"
#include "levels.h"
#include "linknet.h"
#include "linkpanel.h"
#include "panel.h"
#include "uisound.h"
#include "wifi.h"
#include "zyperdraw.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---------- the Game Boy's Link Cable --------------------------------------
 *
 * The in-game menu's Link Cable row on a Game Boy or Game Boy Color game on
 * Gambatte opens this page over the paused game (src/link.h, DESIGN.md
 * 11.70). Off, it asks Host or Join in tiles, as the USB question does. Host
 * makes this handheld's own network and waits for the other one; Join looks
 * for a host and lists what it finds. Linked, the page closes and the game
 * goes on, and a notice says so on both. B stops what it started and goes
 * back to the tiles; B on the tiles closes the page. Every end of the game
 * ends the link (link_game_end). */

#define WLAN_ADDRESS "/sys/class/net/wlan0/address"

/* --link KIND: the page as a shot draws it, and the row's value with
 * --gamemenu */
bool      g_shot_link;
link_shot g_shot_link_kind;

static const char *const SHOT_NAMES[LINK_SHOT_KINDS] = {
	"tiles", "no-adapter", "starting", "waiting", "looking", "found", "joining",
	"linked", "lost", "failed", "stopping",
	"failed-network", "failed-join", "failed-busy", "failed-game",
};

bool link_shot_parse(const char *kind)
{
	int i;

	for (i = 0; kind && i < LINK_SHOT_KINDS; i++)
		if (!strcmp(kind, SHOT_NAMES[i])) {
			g_shot_link = true;
			g_shot_link_kind = (link_shot)i;
			return true;
		}
	return false;
}

/* What the page shows, gathered from src/link.c or made up by a shot */
typedef struct {
	link_state   st;
	link_failure why;
	bool         stopping;     /* the Wi-Fi coming back */
	const char  *self, *peer, *joining;
	const char  *hosts[LINKPANEL_HOSTS];
	int          nhosts;
	bool         scanned;
} link_view;

static void view_now(link_view *v, bool stopping)
{
	int i;

	memset(v, 0, sizeof *v);
	v->st = link_state_now();
	v->why = link_failure_now();
	v->stopping = stopping;
	v->self = link_self_name();
	v->peer = link_peer_name();
	v->joining = link_joining_name();
	v->nhosts = link_host_count();
	if (v->nhosts > LINKPANEL_HOSTS) v->nhosts = LINKPANEL_HOSTS;
	for (i = 0; i < v->nhosts; i++) v->hosts[i] = link_host_name(i);
	v->scanned = link_scanned();
}

static void view_shot(link_view *v)
{
	static const char *const HOSTS[] = { "Pixel 2 0C11", "Pixel 2 B3E4" };

	memset(v, 0, sizeof *v);
	v->self = "Pixel 2 7F2A";
	v->peer = v->joining = "Pixel 2 0C11";
	switch (g_shot_link_kind) {
	case LINK_SHOT_STARTING: v->st = LINK_HOST_STARTING; break;
	case LINK_SHOT_WAITING:  v->st = LINK_HOST_WAITING; break;
	case LINK_SHOT_LOOKING:  v->st = LINK_JOIN_LOOKING; break;
	case LINK_SHOT_FOUND:
		v->st = LINK_JOIN_LOOKING;
		v->hosts[0] = HOSTS[0];
		v->hosts[1] = HOSTS[1];
		v->nhosts = 2;
		v->scanned = true;
		break;
	case LINK_SHOT_JOINING:  v->st = LINK_JOIN_ASKING; break;
	case LINK_SHOT_LINKED:   v->st = LINK_LINKED; break;
	case LINK_SHOT_LOST:     v->st = LINK_LOST; break;
	case LINK_SHOT_FAILED:   v->st = LINK_FAILED; v->why = LINK_FAIL_ANSWER; break;
	case LINK_SHOT_STOPPING: v->st = LINK_HOST_WAITING; v->stopping = true; break;
	case LINK_SHOT_FAILED_NETWORK: v->st = LINK_FAILED; v->why = LINK_FAIL_NETWORK; break;
	case LINK_SHOT_FAILED_JOIN:    v->st = LINK_FAILED; v->why = LINK_FAIL_JOIN; break;
	case LINK_SHOT_FAILED_BUSY:    v->st = LINK_FAILED; v->why = LINK_FAIL_BUSY; break;
	case LINK_SHOT_FAILED_GAME:    v->st = LINK_FAILED; v->why = LINK_FAIL_GAME; break;
	default:                 v->st = LINK_OFF; break;
	}
}

link_state link_row_state(void)
{
	link_view v;

	if (!g_shot_link) return link_state_now();
	view_shot(&v);
	return v.st;
}

bool link_row_adapter(void)
{
	if (g_shot_link) return g_shot_link_kind != LINK_SHOT_NO_ADAPTER;
	return hotspot_available();
}

static bool view_tiles(const link_view *v)
{
	return !v->stopping && (v->st == LINK_OFF || v->st == LINK_FAILED || v->st == LINK_LOST);
}

/* ---- the page's rows ------------------------------------------------------- */

typedef struct {
	char big[128], line[2][160];
	char legend[256];
} link_bufs;

/* The line under the tiles: what went wrong, or what both need */
static const char *tiles_line(const link_view *v)
{
	if (v->st == LINK_LOST) return T(S_LINK_LOST_LINE);
	if (v->st == LINK_FAILED) {
		switch (v->why) {
		case LINK_FAIL_JOIN:   return T(S_LINK_FAIL_JOIN);
		case LINK_FAIL_ANSWER: return T(S_LINK_FAIL_ANSWER);
		case LINK_FAIL_BUSY:   return T(S_LINK_FAIL_BUSY);
		case LINK_FAIL_GAME:   return T(S_LINK_FAIL_GAME);
		default:               return T(S_LINK_FAIL_NETWORK);
		}
	}
	return T(S_LINK_ADAPTERS);
}

/* Host and Join, as the tiles take them: the two, a line under the heading
 * (none), the line under the tiles, the legend */
static int tiles_rows(const link_view *v, menu_row *rows, link_bufs *b)
{
	key_hint hh[3] = { { KEYHINT_LEFT_RIGHT, T(S_KEYHINT_MOVE) }, { "A", T(S_KEYHINT_CHOOSE) },
	                   { "B", T(S_KEYHINT_CLOSE) } };

	keyhints_pack(hh, 3, b->legend, sizeof b->legend);
	rows[0] = (menu_row){ T(S_LINK_HOST), T(S_LINK_HOST_LINE), true };
	rows[1] = (menu_row){ T(S_LINK_JOIN), T(S_LINK_JOIN_LINE), true };
	rows[2] = MENU_NOTE(NULL);
	rows[3] = MENU_NOTE(tiles_line(v));
	rows[4] = MENU_NOTE(b->legend);
	return 5;
}

/* The panel's rows (PANEL_LINK, link_panel reads them back): the large
 * line, three lines under it, the dots (any label for on), the hosts, and
 * the legend last */
static int panel_rows(const link_view *v, menu_row *rows, link_bufs *b)
{
	const char *lines[3] = { NULL, NULL, NULL };
	bool dots = false;
	int n = 0, i;

	b->big[0] = b->legend[0] = '\0';
	if (v->stopping) {
		snprintf(b->big, sizeof b->big, "%s", T(S_LINK_STOPPING));
		dots = true;
	} else if (v->st == LINK_HOST_STARTING || v->st == LINK_HOST_WAITING) {
		snprintf(b->big, sizeof b->big, "%s",
		         T(v->st == LINK_HOST_STARTING ? S_LINK_STARTING : S_LINK_WAITING));
		snprintf(b->line[0], sizeof b->line[0], T(S_LINK_THIS_ONE), v->self);
		lines[0] = b->line[0];
		lines[1] = T(S_LINK_THEN_JOIN);
		dots = true;
		snprintf(b->legend, sizeof b->legend, "%s", b_legend(S_KEYHINT_STOP));
	} else if (v->st == LINK_JOIN_LOOKING && v->nhosts) {
		key_hint hh[3] = { { "^ v", T(S_KEYHINT_MOVE) }, { "A", T(S_KEYHINT_CHOOSE) },
		                   { "B", T(S_KEYHINT_STOP) } };

		snprintf(b->big, sizeof b->big, "%s", T(S_LINK_PICK));
		keyhints_pack(hh, 3, b->legend, sizeof b->legend);
	} else if (v->st == LINK_JOIN_LOOKING) {
		snprintf(b->big, sizeof b->big, "%s", T(S_LINK_LOOKING));
		lines[0] = T(S_LINK_THEN_HOST);
		dots = true;
		snprintf(b->legend, sizeof b->legend, "%s", b_legend(S_KEYHINT_STOP));
	} else if (v->st == LINK_JOIN_JOINING || v->st == LINK_JOIN_ASKING) {
		snprintf(b->big, sizeof b->big, T(S_LINK_JOINING), v->joining);
		dots = true;
		snprintf(b->legend, sizeof b->legend, "%s", b_legend(S_KEYHINT_STOP));
	} else if (v->st == LINK_LINKED) {
		key_hint hh[2] = { { "A", T(S_KEYHINT_STOP) }, { "B", T(S_KEYHINT_CLOSE) } };

		snprintf(b->big, sizeof b->big, T(S_LINK_LINKED_WITH), v->peer);
		lines[0] = T(S_LINK_PAUSE_LINE);
		keyhints_pack(hh, 2, b->legend, sizeof b->legend);
	}
	rows[n++] = (menu_row){ b->big, NULL, false };
	for (i = 0; i < 3; i++) rows[n++] = (menu_row){ lines[i], NULL, false };
	rows[n++] = (menu_row){ dots ? "..." : NULL, NULL, false };
	for (i = 0; i < v->nhosts && v->st == LINK_JOIN_LOOKING && !v->stopping; i++)
		rows[n++] = (menu_row){ v->hosts[i], NULL, true };
	rows[n++] = MENU_NOTE(b->legend);
	return n;
}

void link_panel(app *a, const char *heading, const menu_row *rows, int n, int sel,
                unsigned accent, mm_pose pose)
{
	bool posed = pose.scale != 1.0f || pose.alpha != 1.0f, lifted = false;
	linkpanel p;
	SDL_Rect q;
	int i;

	if (n < 6) return;
	memset(&p, 0, sizeof p);
	p.heading = heading;
	p.big = rows[0].label;
	for (i = 0; i < LINKPANEL_LINES; i++) p.lines[i] = rows[1 + i].label;
	p.dots = rows[4].label != NULL;
	for (i = 5; i < n - 1 && p.nhosts < LINKPANEL_HOSTS; i++) p.hosts[p.nhosts++] = rows[i].label;
	p.sel = sel;
	p.legend = rows[n - 1].label ? rows[n - 1].label : "";
	q = linkpanel_rect(&p);
	if (posed) {
		SDL_FRect f = panel_scaled(&q, pose.scale);
		SDL_Rect fq = { (int)(f.x + 0.5f), (int)(f.y + 0.5f), (int)(f.w + 0.5f), (int)(f.h + 0.5f) };

		ui_lift_k(a->r, &fq, accent, UI_LIFT_PANEL, pose.alpha);
		lifted = true;
		posed = panel_pass_begin(a);
	}
	linkpanel_draw(a->r, &p, !lifted, anim_now());
	if (posed) panel_pass_end(a, &q, pose);
}

static const int MARKS[2] = { UI_GLYPH_BEACON, UI_GLYPH_SEARCH };

/* The page as the view says, through panel_show: the tiles or the panel */
static void show(app *a, const link_view *v, int sel)
{
	menu_row rows[5 + LINKPANEL_HOSTS + 1];
	link_bufs b;
	int n;

	if (view_tiles(v)) {
		n = tiles_rows(v, rows, &b);
		tiles_show(a, PANEL_TILES, T(S_LINK_ROW), rows, n, MARKS, 2, sel);
	} else {
		n = panel_rows(v, rows, &b);
		panel_show(a, PANEL_LINK, T(S_LINK_ROW), rows, n, sel, 0, a->tint, NULL, false, 0, NULL);
	}
}

void link_shot_draw(app *a, SDL_Texture *bg)
{
	link_view v;

	view_shot(&v);
	paused_backdrop(a, bg, 1.0f);
	show(a, &v, 0);
}

/* One frame of the page, shown before something that blocks: the
 * hotspot's start (the supplicant stops), and a stop that brings the Wi-Fi
 * back */
static void page_frame(app *a, SDL_Texture *bg, const link_view *v, int sel)
{
	paused_backdrop(a, bg, 1.0f);
	show(a, v, sel);
	plat_present();
}

/* Everything the link has up, off; the Wi-Fi back said on the page while it
 * comes */
static void stop_shown(app *a, SDL_Texture *bg)
{
	if (link_owes_wifi()) {
		link_view v;

		view_now(&v, true);
		page_frame(a, bg, &v, 0);
	}
	link_stop();
}

/* The title of the game playing, for the other one's log */
static const char *game_title(app *a)
{
	sysview *sv = &a->view[a->sys_cursor];

	if (sv->list.count <= 0 || sv->cursor < 0 || sv->cursor >= sv->list.count) return "";
	return sv->list.items[sv->cursor].title;
}

/* This handheld's adapter names it and its network: read as the page
 * opens, since an adapter can be plugged in at any time */
static void name_from_adapter(void)
{
	char mac[32] = "";

	if (link_state_now() != LINK_OFF) return;
	file_head(WLAN_ADDRESS, mac, sizeof mac);
	link_use(NULL, mac);
}

bool link_page(app *a, SDL_Texture *bg)
{
	int lv, sel = 0, host_sel = 0;
	bool done = false, linked = false;
	link_state was = link_state_now();
	float k;

	name_from_adapter();
	plat_input_flush();
	memset(&a->in, 0, sizeof a->in);
	lv = panel_open();
	while (!done && !want_quit) {
		link_view v;
		link_state st;

		plat_input_poll(&a->in);
		ui_sounds_follow();
		if (menu_leaving(a)) done = true;
		levels_keys(&a->in, BOOST_HIDE);
		/* Power stops the game from here, as from the in-game menu's
		 * other pages; the game's end stops the link */
		if (a->in.pressed[IN_POWER] || idle_due(a)) {
			plat_note_power_pressed();
			plat_resident_line("STOP");
			done = true;
		}
		view_now(&v, false);
		st = v.st;
		/* Linked while the page waited: it closes, and the game goes on */
		if (st == LINK_LINKED && was != LINK_LINKED) {
			uisound_play(DS_SOUND_ACCEPT);
			linked = done = true;
			break;
		}
		was = st;
		if (!done && view_tiles(&v)) {
			bool left = in_repeat(&a->in, IN_LEFT), right = in_repeat(&a->in, IN_RIGHT);

			if (left || right) {
				moved_sound(a, left ? IN_LEFT : IN_RIGHT);
				sel = !sel;
			}
			if (a->in.pressed[IN_ACCEPT]) {
				uisound_play(DS_SOUND_ACCEPT);
				/* What a lost link or a failure left goes first */
				if (st != LINK_OFF) stop_shown(a, bg);
				if (sel == 0) {
					link_view s;

					view_now(&s, false);
					s.st = LINK_HOST_STARTING;
					page_frame(a, bg, &s, 0);
					if (!link_host(game_title(a))) uisound_play(DS_SOUND_DENY);
				} else if (!link_join_look(game_title(a))) {
					uisound_play(DS_SOUND_DENY);
				}
				host_sel = 0;
			} else if (a->in.pressed[IN_BACK]) {
				done = true;
			}
		} else if (!done) {
			if (st == LINK_JOIN_LOOKING && v.nhosts) {
				bool up = in_repeat(&a->in, IN_UP), down = in_repeat(&a->in, IN_DOWN);

				if (host_sel >= v.nhosts) host_sel = v.nhosts - 1;
				if (up || down) {
					moved_sound(a, up ? IN_UP : IN_DOWN);
					host_sel = (host_sel + (up ? -1 : 1) + v.nhosts) % v.nhosts;
				}
				if (a->in.pressed[IN_ACCEPT]) {
					uisound_play(DS_SOUND_ACCEPT);
					if (!link_join_pick(host_sel)) uisound_play(DS_SOUND_DENY);
				}
			} else if (st == LINK_LINKED && a->in.pressed[IN_ACCEPT]) {
				uisound_play(DS_SOUND_BACK);
				stop_shown(a, bg);
			} else if (a->in.pressed[IN_ACCEPT]) {
				/* Nothing to choose while it waits: heard, and said why by
				 * the page */
				uisound_play(DS_SOUND_DENY);
			}
			if (a->in.pressed[IN_BACK]) {
				/* Linked, B closes the page and the link stays; otherwise it
				 * stops what it started, back to the tiles */
				if (st == LINK_LINKED) {
					done = true;
				} else {
					uisound_play(DS_SOUND_BACK);
					stop_shown(a, bg);
				}
			}
		}
		if (a->in.pressed[IN_ACCEPT] || a->in.pressed[IN_BACK]) {
			plat_input_flush();
			memset(&a->in, 0, sizeof a->in);
		}
		if (!panel_frame(a, lv, &k)) continue;
		/* The link's tick (link_tick) is in it, as in every screen's loop */
		tick_theme(a);
		paused_backdrop(a, bg, k);
		view_now(&v, false);
		show(a, &v, view_tiles(&v) ? sel : host_sel);
		plat_present();
		ui_frame_rest(8);
	}
	/* Closed after a failure or a lost link: what it left goes too, so the
	 * row says off and the Wi-Fi the player had is back */
	if (!linked && (link_state_now() == LINK_FAILED || link_state_now() == LINK_LOST))
		stop_shown(a, bg);
	panel_close(a, lv, false);
	plat_input_flush();
	memset(&a->in, 0, sizeof a->in);
	return linked;
}

void link_game_end(app *a)
{
	link_game_over();
	if (link_owes_wifi()) wait_panel(a, T(S_LINK_ROW), T(S_LINK_STOPPING));
	link_stop();
}

/* ---- what the link is done with (link_ops) -------------------------------- */

static bool lop_resident(const char *line) { return plat_resident_line("%s", line); }
static bool lop_hs_start(const char *ssid, const char *pass) { return hotspot_start_named(ssid, pass); }
static int  lop_hs_poll(void) { return (int)hotspot_poll(); }
static void lop_hs_stop(void) { hotspot_stop(); }
/* The radio as it was before the Join page's first scan: a scan brings the
 * supplicant up, and the end of the link puts it back down if it was */
static bool g_radio_known, g_radio_was_off;

static bool lop_scan_start(void)
{
	if (!g_radio_known) {
		g_radio_was_off = wifi_status(NULL, 0, NULL, 0) == WIFI_OFF;
		g_radio_known = true;
	}
	return wifi_up() && wifi_scan_start();
}
static int  lop_scan_poll(void) { return wifi_scan_poll(); }

static int lop_scan_take(char ssids[][LINK_SSID_MAX], int max)
{
	wifi_net nets[WIFI_MAX_NETS];
	int n = wifi_scan_take(nets, WIFI_MAX_NETS), i;

	for (i = 0; i < n && i < max; i++) snprintf(ssids[i], LINK_SSID_MAX, "%s", nets[i].ssid);
	return i;
}

static bool lop_join_start(const char *ssid, const char *pass) { return wifi_join_temp(ssid, pass); }

/* The supplicant asked at most twice a second: each ask is a fork of
 * wpa_cli out of the launcher */
static int lop_join_poll(char *addr, unsigned n)
{
	static unsigned at;
	unsigned now = plat_now_ms();

	if (at && now - at < 500u) return 0;
	at = now ? now : 1;
	return wifi_join_temp_poll(addr, (int)n);
}

static void lop_join_end(void)
{
	wifi_join_temp_end();
	if (g_radio_known && g_radio_was_off) wifi_down();
	g_radio_known = g_radio_was_off = false;
}
static unsigned lop_now(void) { return plat_now_ms(); }

/* "Linked with Pixel 2 0C11" over the game, in the queue with the other
 * pop-ups */
static void lop_notice(link_news what, const char *peer)
{
	char body[96];

	if (what == LINK_NEWS_LINKED) snprintf(body, sizeof body, T(S_LINK_LINKED_WITH), peer);
	else if (what == LINK_NEWS_LEFT) snprintf(body, sizeof body, T(S_LINK_LEFT), peer);
	else snprintf(body, sizeof body, "%s", T(S_LINK_LOST));
	notice_queue(NQ_SET, 0, 3000, T(S_LINK_ROW), body, NULL, 0, 0);
}

static const link_ops g_link_ops = {
	lop_resident, lop_hs_start, lop_hs_poll, lop_hs_stop,
	lop_scan_start, lop_scan_poll, lop_scan_take,
	lop_join_start, lop_join_poll, lop_join_end,
	linknet_open, linknet_send, linknet_recv, linknet_close,
	lop_now, lop_notice,
};

void link_setup(void)
{
	char mac[32] = "";

	file_head(WLAN_ADDRESS, mac, sizeof mac);
	link_use(&g_link_ops, mac);
}
