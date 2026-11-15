/* SPDX-License-Identifier: GPL-3.0-only */
#include "player2_screen.h"

#include "aout_route.h"
#include "app.h"
#include "hare.h"
#include "ingame_screen.h"
#include "keyhints.h"
#include "look.h"
#include "mdns.h"
#include "p2.h"
#include "p2panel.h"
#include "p2share.h"
#include "panel.h"
#include "spawn.h"
#include "uisound.h"
#include "usblink.h"
#include "webtheme.h"
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

/* ---------- NES Player 2 ---------------------------------------------------
 *
 * The in-game menu's Player 2 row on an NES game opens this page over the
 * paused game (src/p2.h, src/p2panel.h): A starts it, on the handheld's own
 * Wi-Fi when an adapter is in or for a computer on the cable, and stops it;
 * B closes the page, and Continue goes back to the game with it on. The
 * page says how a friend joins and who is playing; a notice over the game
 * says when one joins or leaves. Every end of the game stops it (launch). */

typedef struct {
	char how[192];
	char addr[96];
	char who[P2PANEL_STATUS][32];
} p2_bufs;

/* --player2 KIND: the page as a shot draws it, and the row's value with
 * --gamemenu (shot_draw_player2) */
bool    g_shot_p2;
p2_view g_shot_p2_view;

/* The state the in-game menu's row says */
p2_state p2_row_state(void)
{
	return g_shot_p2 ? g_shot_p2_view.st : p2_state_now();
}

/* The page's rows, from the view: the code's text (none while it is not
 * shown), the network and its password beside it with the address under
 * them, the line that says what to do, a line per player and the key
 * legend. What src/p2panel.c draws and what the panel's copy keeps
 * (snap_record), so a shot of it is the page. */
static int p2_rows(const p2_view *v, menu_row *rows, p2_bufs *b)
{
	const bool on = v->st == P2_WAITING || v->st == P2_CONNECTED;
	const bool code = on && v->link == P2_HOTSPOT && !v->starting && !v->closing &&
	                  v->qr && v->qr[0];
	const char *legend;
	int n = 0, k;

	/* The stop's moment: the hotspot's says the Wi-Fi is coming back */
	if (v->closing && v->link == P2_HOTSPOT) snprintf(b->how, sizeof b->how, "%s", T(S_XFER_CLOSING_HOTSPOT));
	else if (v->closing) snprintf(b->how, sizeof b->how, "%s", T(S_PLAYER2_STOPPING));
	else if (v->starting) snprintf(b->how, sizeof b->how, "%s", T(S_XFER_STARTING_HOTSPOT));
	else if (v->st == P2_FAILED) snprintf(b->how, sizeof b->how, "%s", T(S_PLAYER2_FAILED));
	else if (code) snprintf(b->how, sizeof b->how, "%s", T(S_PLAYER2_SCAN));
	/* The name, which is what a person types: zyperos.local/play */
	else if (on && v->link == P2_CABLE) snprintf(b->how, sizeof b->how, T(S_PLAYER2_CABLE), v->url);
	else if (v->link == P2_HOTSPOT) snprintf(b->how, sizeof b->how, "%s", T(S_PLAYER2_INTRO_WIFI));
	else if (v->link == P2_CABLE) snprintf(b->how, sizeof b->how, "%s", T(S_PLAYER2_INTRO_CABLE));
	else snprintf(b->how, sizeof b->how, "%s", T(S_PLAYER2_NO_LINK));

	rows[n++] = (menu_row){ code ? v->qr : "", NULL, false };
	rows[n++] = (menu_row){ T(S_HOTPANEL_NETWORK), code ? v->ssid : NULL, false };
	rows[n++] = (menu_row){ T(S_PANEL_PASSWORD), code ? v->pass : NULL, false };
	/* The address too, for a phone whose sign-in page does not open by
	 * itself once it joined: the page's own, which p2d lets in */
	b->addr[0] = '\0';
	if (code) snprintf(b->addr, sizeof b->addr, T(S_PLAYER2_OR_OPEN), v->url);
	rows[n++] = (menu_row){ b->addr, NULL, false };
	rows[n++] = (menu_row){ b->how, NULL, false };
	/* A line for each player a friend can be: player 2 on the NES, and on
	 * a system with more, 3 and 4 the same way */
	for (k = 0; k + 2 <= v->players && k < P2PANEL_STATUS; k++) {
		const char *st = v->starting ? T(S_HOTPANEL_STARTING)
		               : !on ? T(S_MENU_OFF)
		               : v->guests & (1u << k) ? T(S_PLAYER2_CONNECTED) : T(S_PLAYER2_WAITING);

		snprintf(b->who[k], sizeof b->who[k], T(S_PLAYER2_PLAYER), k + 2);
		rows[n++] = (menu_row){ b->who[k], st, false };
	}
	/* A starts or stops it ("stop", since "turn off" and "close" are one
	 * word in Chinese); with nothing to start it over, only B */
	if (on)
		legend = keyhints_legend((key_hint[]){ { "A", T(S_KEYHINT_STOP) },
		                                       { "B", T(S_KEYHINT_CLOSE) } }, 2);
	else if (v->link != P2_NO_LINK)
		legend = keyhints_legend((key_hint[]){ { "A", T(S_KEYHINT_TURN_ON) },
		                                       { "B", T(S_KEYHINT_CLOSE) } }, 2);
	else
		legend = b_legend(S_KEYHINT_CLOSE);
	rows[n++] = MENU_NOTE(legend);
	return n;
}

/* The page's panel from its rows, as panel_show and the panel's copy draw
 * it (PANEL_P2), at `pose`: its lift on the screen at the panel's size, the
 * panel into the pass, as menu_panel moves its own */
void p2_panel(app *a, const char *heading, const menu_row *rows, int n,
              unsigned accent, mm_pose pose)
{
	bool posed = pose.scale != 1.0f || pose.alpha != 1.0f, lifted = false;
	p2panel p;
	SDL_Rect q;
	int i;

	if (n < 6) return;
	memset(&p, 0, sizeof p);
	p.heading = heading;
	p.accent = accent;
	p.qr = rows[0].label;
	p.ssid = rows[1].value;
	p.pass = rows[2].value;
	p.addr = rows[3].label;
	p.how = rows[4].label;
	for (i = 5; i < n - 1 && p.nwho < P2PANEL_STATUS; i++, p.nwho++) {
		p.who[p.nwho] = rows[i].label;
		p.state[p.nwho] = rows[i].value;
	}
	p.legend = rows[n - 1].label;
	q = p2panel_rect(&p);
	if (posed) {
		SDL_FRect f = panel_scaled(&q, pose.scale);
		SDL_Rect fq = { (int)(f.x + 0.5f), (int)(f.y + 0.5f), (int)(f.w + 0.5f), (int)(f.h + 0.5f) };

		ui_lift_k(a->r, &fq, accent, UI_LIFT_PANEL, pose.alpha);
		lifted = true;
		posed = panel_pass_begin(a);
	}
	p2panel_draw(a->r, &p, !lifted);
	if (posed) panel_pass_end(a, &q, pose);
}

/* The page as it is now, the address shown without its http:// and slash */
static void p2_view_now(p2_view *v, const system_cfg *q, p2_link can, bool starting, bool closing)
{
	static char url[64];
	const p2_system *sys = p2_system_of(q->core);
	const char *u = p2_join_url();
	size_t un;

	memset(v, 0, sizeof *v);
	v->st = p2_state_now();
	/* The link it started on; while off, or after a start that never got
	 * one, the one A would start */
	v->link = p2_link_on();
	if (v->link == P2_NO_LINK) v->link = can;
	v->starting = starting || p2_starting();
	v->closing = closing;
	v->guests = p2_guests();
	v->players = p2_players() ? p2_players() : sys ? sys->players : 2;
	v->ssid = hotspot_ssid();
	v->pass = hotspot_password();
	v->qr = hotspot_qr_text();
	if (!strncmp(u, "http://", 7)) u += 7;
	snprintf(url, sizeof url, "%s", u);
	un = strlen(url);
	if (un && url[un - 1] == '/') url[un - 1] = '\0';
	v->url = url;
}

void p2_show(app *a, const p2_view *v)
{
	menu_row rows[5 + P2PANEL_STATUS + 1];
	p2_bufs b;
	int n = p2_rows(v, rows, &b);

	panel_show(a, PANEL_P2, T(S_PLAYER2_ROW), rows, n, -1, 0, a->tint, NULL, false, 0, NULL);
}

/* One frame of the page, shown before something that blocks: the hotspot's
 * start, which waits for the supplicant to stop, and its stop, which waits
 * for the Wi-Fi to come back (as File Transfer's say it, wait_panel) */
static void p2_page_frame(app *a, SDL_Texture *bg, const system_cfg *q, p2_link can,
                          bool starting, bool closing)
{
	p2_view v;

	p2_view_now(&v, q, can, starting, closing);
	paused_backdrop(a, bg, 1.0f);
	p2_show(a, &v);
	plat_present();
}

/* The page, from the in-game menu's row, over the paused game `bg` */
void player2_page(app *a, SDL_Texture *bg)
{
	const system_cfg *q = &a->sys.systems[shelf_owner(a, a->sys_cursor,
	                                      a->view[a->sys_cursor].cursor)];
	p2_link can = P2_NO_LINK;
	unsigned looked = 0;
	int lv, done = 0;
	float k;

	plat_input_flush();
	memset(&a->in, 0, sizeof a->in);
	lv = panel_open();
	while (!done && !want_quit) {
		unsigned now = plat_now_ms();
		bool on = p2_state_now() == P2_WAITING || p2_state_now() == P2_CONNECTED;

		plat_input_poll(&a->in);
		ui_sounds_follow();
		if (menu_leaving(a)) done = 1;
		if (in_repeat(&a->in, IN_VOLUP))    plat_volume_nudge(+1);
		if (in_repeat(&a->in, IN_VOLDN))    plat_volume_nudge(-1);
		if (in_repeat(&a->in, IN_BRIGHTUP)) plat_brightness_nudge(+1);
		if (in_repeat(&a->in, IN_BRIGHTDN)) plat_brightness_nudge(-1);
		/* Power stops the game from here, as from the in-game menu's
		 * other pages; the game's end stops Player 2 */
		if (a->in.pressed[IN_POWER] || idle_due(a)) {
			plat_note_power_pressed();
			plat_resident_line("STOP");
			done = 1;
		}
		/* Which link a start would take, looked at five times a second, so
		 * an adapter or a computer plugged in with the page open shows */
		if (!looked || now - looked >= 200u) {
			looked = now ? now : 1;
			can = p2_link_now(hotspot_available(), plat_usb_peer() == USB_PEER_COMPUTER);
		}
		if (!done && a->in.pressed[IN_ACCEPT]) {
			if (on) {
				uisound_play(DS_SOUND_BACK);
				p2_page_frame(a, bg, q, can, false, true);
				p2_stop();
			} else if (can == P2_NO_LINK) {
				uisound_play(DS_SOUND_DENY);
			} else {
				uisound_play(DS_SOUND_ACCEPT);
				p2_page_frame(a, bg, q, can, can == P2_HOTSPOT, false);
				if (!p2_start(q->core, can)) uisound_play(DS_SOUND_DENY);
			}
			plat_input_flush();
			memset(&a->in, 0, sizeof a->in);
		}
		if (!panel_frame(a, lv, &k)) continue;
		/* Player 2's tick (p2_tick) is in it, as in every screen's loop */
		tick_theme(a);
		paused_backdrop(a, bg, k);
		{
			p2_view v;

			p2_view_now(&v, q, can, false, false);
			p2_show(a, &v);
		}
		plat_present();
		ui_frame_rest(8);
	}
	/* Closed after a failure: what it left goes too, so the row says off and
	 * the Wi-Fi the player had is back */
	if (p2_state_now() == P2_FAILED) {
		p2_page_frame(a, bg, q, can, false, true);
		p2_stop();
	}
	panel_close(a, lv, false);
	plat_input_flush();
	memset(&a->in, 0, sizeof a->in);
}

/* ---- what Player 2 is done with (p2_ops) ---------------------------------- */

static bool p2op_resident(const char *line) { return plat_resident_line("%s", line); }

static bool p2op_answer(const char *prefix, char *out, unsigned n, unsigned ms)
{
	return plat_resident_sync_line(prefix, out, n, (int)ms);
}

/* The theme the handheld shows now (src/webtheme.h), its look with it */
static bool p2op_theme(void)
{
	return webtheme_write(WEBTHEME_FILE, theme(), theme_index() == DS_DARK, look_square());
}

static bool p2op_hotspot_start(void) { return hotspot_start_dns(false); }
static int  p2op_hotspot_poll(void)  { return (int)hotspot_poll(); }
static void p2op_hotspot_stop(void)  { hotspot_stop(); }
static void p2op_cable_up(void)      { usblink_for_transfer(); }
static void p2op_cable_down(void)    { usblink_after_transfer(db_dev()); }

static bool p2op_mdns_start(void)    { return mdns_start("usb0", P2_CABLE_ADDR, false); }
static void p2op_mdns_poll(unsigned now) { mdns_poll(now); }

/* Developer Mode's SSH has the name on the cable too (dev_ssh_tick), and
 * keeps it for as long as the cable is in */
static void p2op_mdns_stop(void)     { if (!g_dv.run) mdns_stop(); }

static long p2op_spawn(char *const argv[])     { return spawn_child(argv); }
static void p2op_signal(long pid, int sig)     { kill((pid_t)pid, sig); }
static bool p2op_ended(long pid, int *status)  { return spawn_ended(pid, status); }
static unsigned p2op_now(void)                 { return plat_now_ms(); }
static void p2op_sleep(unsigned ms)            { SDL_Delay(ms); }

/* The share's guests, read from the file rather than mapped: a hundred
 * bytes ten times a second, and nothing held from one game to the next,
 * whose share is a new file. No file, or its magic cleared, is no share. */
static int p2op_guests(void)
{
	p2s_head h;
	int fd = open(P2S_PATH, O_RDONLY | O_CLOEXEC);
	ssize_t got;

	if (fd < 0) return -1;
	got = pread(fd, &h, sizeof h, 0);
	close(fd);
	if (got != (ssize_t)sizeof h || h.magic != P2S_MAGIC) return -1;
	return (int)(h.guests & 0x7FFFFFFFu);
}

static void p2op_unshare(void) { unlink(P2S_PATH); }

/* "Player 2 joined" over the game, in the queue with the other pop-ups */
static void p2op_notice(p2_news what, int player)
{
	char body[96];

	if (what == P2_STOPPED) {
		snprintf(body, sizeof body, "%s", T(S_PLAYER2_STOPPED));
		fprintf(stderr, "player 2: said it stopped\n");
	} else {
		snprintf(body, sizeof body, T(what == P2_JOINED ? S_PLAYER2_JOINED : S_PLAYER2_LEFT), player);
		fprintf(stderr, "player 2: player %d %s\n", player, what == P2_JOINED ? "joined" : "left");
	}
	notice_queue(NQ_SET, 0, 3000, T(S_PLAYER2_NOTICE), body, NULL, 0, 0);
}

/* The page's words in the language the handheld is in, as hare's /i18n.js */
static const char *p2op_lang(void) { return lang_code(i18n_lang()); }

static char   g_p2_bin[CFG_STR * 2];
static p2_ops g_p2_ops = {
	p2op_resident, p2op_answer, p2op_theme,
	p2op_hotspot_start, p2op_hotspot_poll, p2op_hotspot_stop,
	p2op_cable_up, p2op_cable_down,
	p2op_mdns_start, p2op_mdns_poll, p2op_mdns_stop,
	p2op_spawn, p2op_signal, p2op_ended,
	p2op_now, p2op_sleep, p2op_guests, p2op_unshare, p2op_notice, p2op_lang,
	NULL, NULL,
};

/* Once, as the launcher starts: p2d beside it on the card, the page in its
 * res/web (mk/payload.sh lays both out) */
void p2_setup(void)
{
	snprintf(g_p2_bin, sizeof g_p2_bin, "%s/p2d", P_ROOT);
	g_p2_ops.p2d = g_p2_bin;
	g_p2_ops.root = P_WEB;
	p2_use(&g_p2_ops);
}
