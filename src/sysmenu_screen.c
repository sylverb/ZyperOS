/* SPDX-License-Identifier: GPL-3.0-only */
#include "sysmenu_screen.h"
#include "dmodepreview.h"

#include "aout_route.h"
#include "muselib.h"
#include "audioout.h"
#include "dslang.h"
#include "app.h"
#include "bgm.h"
#include "bgm_menu.h"
#include "bgm_screen.h"
#include "bt.h"
#include "bt_screen.h"
#include "clock.h"
#include "clock_screen.h"
#include "devmode.h"
#include "homesys_screen.h"
#include "hotspot.h"
#include "linebreak.h"
#include "look.h"
#include "menu_panel.h"
#include "menu_pick.h"
#include "nowplaying_screen.h"
#include "osdbar.h"
#include "panel.h"
#include "powerhold.h"
#include "powerscreens.h"
#include "rafetch.h"
#include "saver.h"
#include "search_screen.h"
#include "sgdb.h"
#include "shelfbuild.h"
#include "sort.h"
#include "stats_screen.h"
#include "storage_screen.h"
#include "strength.h"
#include "tour_screen.h"
#include "ui.h"
#include "uisound.h"
#include "update_screen.h"
#include "usblink.h"

#include <stdio.h>
#include <string.h>


/* Developer (SSH) is offered on a development image (dropbear at boot) and
 * on a release image once the player has unlocked Developer Mode (devmode.h) */
static bool dev_offered(void)
{
	return usblink_dev_image() || devmode_unlocked(db_dev());
}

/* MENU's Developer Mode row: on a development image the link-always-bound
 * setting (usblink.h); on a release image, where the row is there only once
 * Developer Mode is unlocked, that it is on (the row opens its page) */
static bool dev_row_on(void)
{
	return usblink_dev_image() ? usblink_developer(db_dev()) : devmode_unlocked(db_dev());
}

/* MENU's USB rows: on a device where usbrole decides (or a shot that
 * says so), and USB Device while a plug waits */
static bool usb_power_offered(void)
{
	usb_power p;

	return shot_usb_menu || plat_usb_power(&p);
}

static bool usb_wait_row(void) { return shot_usb_menu || g_usb_wait != 0; }

/* Automatic's message, as it is chosen: what it does and its one risk, on
 * up to four balanced lines, a line more than result_message's, since the
 * risk is said whole rather than cut */
int usb_auto_lines(app *a, char lines[4][LB_LINE], menu_row rows[4])
{
	int w = menu_std_width(a), i;
	int n = ui_balance_lines(ui_font(UI_F_MENU), T(S_SYSMENU_USB_AUTO_SAID), w, 4, lines);

	ui_fit_drawn(T(S_SYSMENU_USB_AUTO_SAID));
	for (i = 0; i < n; i++) rows[i] = (menu_row){ lines[i], NULL, false };
	return n;
}

/* --cheevos-unchecked, for a shot: the Achievements rows as a game never
 * played with a network has them, whatever the card holds */
bool g_shot_unchecked;
/* --cheevos-no-hash file|system, the same for a game the launcher has no
 * hash for: its file's kind, or every kind its system takes (a PSP game) */
ra_hash_kind g_shot_no_hash;

/* A on an Achievements row that says "not checked yet": how a game gets its
 * set - one play with a network - and that it then works offline */
void cheevos_unchecked_message(app *a)
{
	char lines[4][LB_LINE];
	menu_row rows[4];
	int w = menu_std_width(a), i;
	int n = ui_balance_lines(ui_font(UI_F_MENU), T(S_CHEEVOS_UNCHECKED_SAID), w, 4, lines);

	ui_fit_drawn(T(S_CHEEVOS_UNCHECKED_SAID));
	for (i = 0; i < n; i++) rows[i] = (menu_row){ lines[i], NULL, false };
	timed_message(a, T(S_SYSMENU_CHEEVOS), rows, n, w, MENU_ZYPEROS);
}

static void usb_auto_message(app *a)
{
	char lines[4][LB_LINE];
	menu_row rows[4];
	int n = usb_auto_lines(a, lines, rows);

	timed_message(a, T(S_SYSMENU_USB_ACCESSORIES), rows, n, menu_std_width(a), MENU_ZYPEROS);
}

static const char *look_accent_name(void);   /* with the Theme page */
static const char *auto_hours_value(void);

/* ---------- option rows' choices (menu.h's menu_choice, DESIGN.md 9) ------ */

/* Each option row of more than two choices in MENU, a system's menu and the
 * Muse shelf, and the Muse shelf's Sort By (two A did not turn), as their
 * key handlers step them: in the order Left and Right go, so the runner
 * applies a choice by sending them. The words live here until the next
 * build. */
#define CH_MAX 48   /* a MENU row's most: Auto Hours' 48 half hours */
typedef struct {
	const char *lab[CH_MAX];
	char buf[CH_MAX][48];
} ch_pool;

static menu_choice g_pm_choice[PM_ROWS], g_sm_choice[SM_ROWS], g_smm_choice[SM_MUSE_ROWS];

static menu_choice ch_make(const ch_pool *p, int n, int cur, bool none)
{
	if (n > CH_MAX) n = CH_MAX;
	return (menu_choice){ .n = (unsigned char)n, .cur = (unsigned char)(cur >= 0 && cur < n ? cur : 0),
	                      .none = none, .labels = p->lab };
}

/* The shaders this device runs, in shader_step's order, `cur` the place of
 * shader `on` */
static int ch_shaders(ch_pool *p, int on, int *cur)
{
	int k, n = 0;

	*cur = 0;
	for (k = 0; k < SHADER_COUNT && n < CH_MAX; k++) {
		if (SHADERS[k].gpu && !plat_gpu_shaders()) continue;
		if (k == on) *cur = n;
		p->lab[n++] = shader_label(k);
	}
	return n;
}

/* Strength's rungs, 10% to 100% */
static int ch_strengths(ch_pool *p, int on, int *cur)
{
	int v, n = 0;

	*cur = 0;
	for (v = STRENGTH_MIN; v <= STRENGTH_MAX && n < CH_MAX; v += STRENGTH_STEP) {
		if (v == on) *cur = n;
		snprintf(p->buf[n], sizeof p->buf[n], "%d%%", v);
		p->lab[n] = p->buf[n];
		n++;
	}
	return n;
}
/* The shelf's Display list's picture (menu_choice's preview): the game
 * under the cursor's last frame, its Auto picture, as mode `k` would draw
 * it, with the shape its system's games last said (dmode_note_shape); a
 * game never played has no frame, and the stand-in shows the mode */
static void sm_dmode_preview(void *app_, int k, int x, int y, int w, int h)
{
	static SDL_Texture *t;
	static char made[LIB_PATH * 2];
	static int tw, th;
	app *a = app_;
	sysview *v = &a->view[a->sys_cursor];
	char path[LIB_PATH * 2] = "";
	double aspect = 0.0;
	bool known = false;
	int fw, fh, o;

	if (v->cursor >= 0 && v->cursor < v->list.count) {
		o = shelf_owner(a, a->sys_cursor, v->cursor);
		preview_path(a, o, &v->list.items[v->cursor], path, sizeof path);
		aspect = dmode_shape_of(a->sys.systems[o].tag, &known);
	}
	if (strcmp(path, made)) {
		if (t) SDL_DestroyTexture(t);
		t = path[0] ? load_image(a->r, path, &tw, &th, NULL) : NULL;
		snprintf(made, sizeof made, "%s", path);
	}
	/* Kept shape unknown: a console's */
	if (!known) aspect = 4.0 / 3.0;
	dmode_surface(&fw, &fh);
	dmode_preview_draw(a, t, t ? tw : 0, t ? th : 0, aspect, fw, fh, k, x, y, w, h);
}


static void menu_choices(app *a, int sys, bool games)
{
	/* A system's menu and MENU each their own words: the shelf under MENU
	 * builds a system's rows too, and must not write over MENU's */
	static ch_pool PS[9], PM[10];
	ch_pool *P = games ? PS : PM;
	int i, n, cur;

	memset(g_pm_choice, 0, sizeof g_pm_choice);
	memset(g_sm_choice, 0, sizeof g_sm_choice);
	memset(g_smm_choice, 0, sizeof g_smm_choice);
	if (games) {
		const system_cfg *sc = &a->sys.systems[sys];
		sysview *v = &a->view[sys];

		if (is_muse(sc)) {
			/* Show: the kinds the card has, then All, round (ML_ALL is
			 * the number after the three) */
			static const int show_text[ML_ALL + 1] = { S_SYSMENU_MUSE_MUSIC, S_SYSMENU_MUSE_AUDIOBOOKS,
			                                           S_SYSMENU_MUSE_VIDEOS, S_SYSMENU_MUSE_ALL };

			for (i = 0, n = 0, cur = 0; i <= ML_ALL; i++) {
				if (!ml_count_kind(&g_muse, (ml_kind)i)) continue;
				if (i == (int)muse_kind_shown()) cur = n;
				P[0].lab[n++] = T(show_text[i]);
			}
			g_smm_choice[SMM_SHOW] = ch_make(&P[0], n, cur, false);
			for (i = 0; i < ML_ORDERS; i++) P[1].lab[i] = ml_order_label_kind((ml_order)i, muse_kind_shown());
			g_smm_choice[SMM_SORT] = ch_make(&P[1], ML_ORDERS, v->sort, false);
			return;
		}
		n = cfg_core_count(sc);
		for (i = 0, cur = 0; i < n && i < CH_MAX; i++) {
			char core[CFG_STR];

			if (!cfg_core_at(sc, i, core, sizeof core)) core[0] = '\0';
			if (!strcmp(core, sc->core)) cur = i;
			core_label(core, P[2].buf[i], sizeof P[2].buf[i]);
			P[2].lab[i] = P[2].buf[i];
		}
		if (n > 1) g_sm_choice[SM_CORE] = ch_make(&P[2], n, cur, false);
		for (i = 0; i < DSL_CHOICES; i++) P[3].lab[i] = dslang_label((dsl_choice)i, P[3].buf[i], sizeof P[3].buf[i]);
		g_sm_choice[SM_DSLANG] = ch_make(&P[3], DSL_CHOICES, dslang_load(), false);
		for (i = 0; i < SORT_COUNT; i++) P[4].lab[i] = sort_label(i);
		g_sm_choice[SM_SORT] = ch_make(&P[4], SORT_COUNT, v->sort, false);
		{
			static const char *notes[CH_MAX];

			for (i = 0; i < DMODE_COUNT && i < CH_MAX; i++) {
				P[5].lab[i] = dmode_label(i);
				notes[i] = dmode_note(i);
			}
			g_sm_choice[SM_DISPLAY] = ch_make(&P[5], DMODE_COUNT, v->dmode, false);
			g_sm_choice[SM_DISPLAY].notes = notes;
			g_sm_choice[SM_DISPLAY].preview = sm_dmode_preview;
		}
		n = ch_shaders(&P[6], v->shader, &cur);
		g_sm_choice[SM_SHADER] = ch_make(&P[6], n, cur, false);
		n = ch_strengths(&P[7], strength_clamp(v->strength), &cur);
		g_sm_choice[SM_STRENGTH] = ch_make(&P[7], n, cur, false);
		/* View: in the order Right steps it (view_turn) */
		P[8].lab[0] = T(S_SYSMENU_VIEW_COVERS);
		P[8].lab[1] = T(S_SYSMENU_VIEW_GRID);
		P[8].lab[2] = T(S_SYSMENU_VIEW_LIST);
		g_sm_choice[SM_VIEW] = ch_make(&P[8], 3, v->listed ? 2 : v->grid ? 1 : 0, false);
		return;
	}
	/* MENU */
	for (i = 0; i < 3; i++) P[0].lab[i] = appearance_name((appearance)i);
	g_pm_choice[PM_APPEARANCE] = ch_make(&P[0], 3, (int)theme_appearance(), false);
	P[1].lab[BATT_BARS] = T(S_PANEL_BATTERY_BARS);
	P[1].lab[BATT_PCT] = T(S_PANEL_BATTERY_PERCENTAGE);
	P[1].lab[BATT_TIME] = T(S_PANEL_BATTERY_TIME);
	g_pm_choice[PM_BATTERY] = ch_make(&P[1], BATT_MODES, g_batt_mode, false);
	for (i = 0, cur = 0; i < CHARGE_LIMITS_COUNT; i++) {
		if (CHARGE_LIMITS[i] == g_charge_limit) cur = i;
		if (CHARGE_LIMITS[i] > 0) snprintf(P[2].buf[i], sizeof P[2].buf[i], "%d%%", CHARGE_LIMITS[i]);
		P[2].lab[i] = CHARGE_LIMITS[i] > 0 ? P[2].buf[i] : T(S_MENU_OFF_TITLE);
	}
	g_pm_choice[PM_CHARGE] = ch_make(&P[2], CHARGE_LIMITS_COUNT, cur, false);
	{
		bool mixed = false;

		for (i = 0; i < a->sys.count; i++) if (a->view[i].shader != g_shader_all) mixed = true;
		n = ch_shaders(&P[3], g_shader_all, &cur);
		g_pm_choice[PM_SHADER] = ch_make(&P[3], n, cur, mixed);
		mixed = false;
		{
			int all = strength_read_all(db_dev());

			for (i = 0; i < a->sys.count; i++)
				if (strength_clamp(a->view[i].strength) != strength_clamp(all)) mixed = true;
			n = ch_strengths(&P[4], strength_clamp(all), &cur);
			g_pm_choice[PM_STRENGTH] = ch_make(&P[4], n, cur, mixed);
		}
	}
	{
		aout_state ao = aout_now();
		aout_dest dest = aout_actual(&ao);

		for (i = 0; i < AOUT_CHANNELS_COUNT && i < CH_MAX; i++) {
			aout_channels_label((aout_channels)i, aout_channels_for((aout_channels)i, dest, plat_speaker_mono()),
			                    P[5].buf[i], sizeof P[5].buf[i]);
			P[5].lab[i] = P[5].buf[i];
		}
		g_pm_choice[PM_CHANNELS] = ch_make(&P[5], AOUT_CHANNELS_COUNT, (int)g_channels, false);
	}
	{
		/* The voices round from the first, as uisound_setting_step goes */
		int s0 = 0, s = s0, on = uisound_setting();

		for (n = 0, cur = 0; n < CH_MAX; ) {
			if (s == on) cur = n;
			P[6].lab[n++] = uisound_setting_name(s);
			s = uisound_setting_step(s, 1);
			if (s == s0) break;
		}
		g_pm_choice[PM_SOUNDS] = ch_make(&P[6], n, cur, false);
	}
	for (i = 0, cur = 0; i < AUTO_OFF_COUNT; i++) {
		if (AUTO_OFF[i] == a->auto_off) cur = i;
		sys_menu_auto_off_label(AUTO_OFF[i], P[7].buf[i], sizeof P[7].buf[i]);
		P[7].lab[i] = P[7].buf[i];
	}
	g_pm_choice[PM_SLEEP] = ch_make(&P[7], AUTO_OFF_COUNT, cur, false);
	{
		/* The screensaver's rungs, from its lowest up, as saver_step goes */
		int s = g_saver_s, t;

		while ((t = saver_step(s, -1)) != s) s = t;
		for (n = 0, cur = 0; n < CH_MAX; ) {
			if (s == g_saver_s) cur = n;
			sys_menu_saver_label(s, P[8].buf[n], sizeof P[8].buf[n]);
			P[8].lab[n] = P[8].buf[n];
			n++;
			if ((t = saver_step(s, 1)) == s) break;
			s = t;
		}
		g_pm_choice[PM_SAVER] = ch_make(&P[8], n, cur, false);
	}
	/* Home Screen: in the order Right steps it */
	for (i = 0; i < HOME_LAYOUTS; i++) P[9].lab[i] = home_name((home_layout)i);
	g_pm_choice[PM_HOME] = ch_make(&P[9], HOME_LAYOUTS, (int)a->home_layout, false);
	g_pm_choice[PM_LANGUAGE] = (menu_choice){ .n = LANG_COUNT, .cur = (unsigned char)i18n_lang(), .own = true };
}

int menu_build(app *a, screen_id screen, int sys,
               menu_row *out, menu_bufs *b, const char **heading)
{
	char ss[WIFI_SSID_MAX];
	sys_ui u = { 0 };

	/* Asked once. The cache is why - see menu_wifi - and three rows used to
	 * ask separately, which is how it got expensive in the first place. */
	u.wifi  = menu_wifi(ss, sizeof ss);
	u.ssid  = ss;
	u.games = screen == SCREEN_GAMES;
	u.no_wifi = !plat_has_wifi();
	u.no_bt   = !plat_has_bluetooth();
	/* USB File Transfer over the cable, or over a phone's tethering */
	u.cable_xfer = xfer_link_up();
	/* A way out to the internet with no radio: a phone's tethering */
	u.online = net_internet(NULL);
	u.no_developer = !dev_offered();
	u.developer  = dev_row_on();
	u.hotspot = hotspot_available();     /* two stats, no fork */
	/* File Transfer behind the shelf: its row says so, under its name */
	u.xfer = xs_row_value();
	u.xfer_cable = xs_row_cable();
	u.no_lights = !plat_battery_lights();
	u.no_charge_limit = !plat_charge_limit_supported();
	u.charge_limit = g_charge_limit;
	u.no_usb_power = !usb_power_offered();
	u.usb_auto = g_usb_auto;
	u.no_usb_wait = !usb_wait_row();
	u.tab = g_tabs.at;

	if (u.games) {
		const system_cfg *sc = &a->sys.systems[sys];

		static char corebuf[CFG_STR];

		u.sys_name   = sc->name;
		u.sys_core   = core_label(sc->core, corebuf, sizeof corebuf);
		u.core_choice = cfg_core_count(sc) > 1;
		{
			static char dslbuf[64];

			u.dslang = dslang_row_shown(a, sys) ? dslang_label(dslang_load(), dslbuf, sizeof dslbuf) : NULL;
		}
		/* By what it HAS, not by its name or its position: a shelf with no
		 * core and no folder is the built one, and a check can hand that
		 * over without inventing a flag day. */
		u.muse       = is_muse(sc);
		u.fav        = !u.muse && sc->core[0] == '\0' && sc->folder[0] == '\0';
		/* Every game of the system, in its folders too: the shelf's own
		 * entries would count a folder as one */
		u.game_count = sys_games(&a->view[sys])->count;
		u.dmode      = dmode_label(a->view[sys].dmode);
		u.shader     = shader_label(a->view[sys].shader);
		u.strength   = strength_row_label(a, sys);
		u.no_strength = !u.strength;
		u.listed     = a->view[sys].listed;
		u.grid       = a->view[sys].grid;
		u.muse_books = u.muse && muse_books_shown();
		u.muse_videos = u.muse && muse_kind_shown() == ML_VIDEOS;
		u.muse_all   = u.muse && muse_kind_shown() == ML_ALL;
		u.muse_songs = u.muse && ml_count_kind(&g_muse, ML_MUSIC) > 0;
		u.muse_both  = u.muse && muse_kinds() > 1;
		u.muse_playing = u.muse && lock_allowed();
		u.sort       = u.muse ? ml_order_label_kind((ml_order)a->view[sys].sort, muse_kind_shown())
		                      : sort_label(a->view[sys].sort);
	} else {
		aout_state ao = aout_now();

		u.ra_in     = ra_signed_in();
		u.ra_name   = u.ra_in ? ra_user() : NULL;
		/* A check the page started may have ended after it closed */
		sgdb_try_poll();
		u.scraper   = g_scraper_val[0] ? g_scraper_val : NULL;
		{
			static char upv[48];
			u.update = up_have() ? su_menu_value(up_view(false), upv, sizeof upv) : NULL;
		}
		u.storage = storage_menu_value();
		u.appearance = appearance_name(theme_appearance());
		u.shader_all = shader_all_label(a);
		u.strength_all = strength_all_label(a);
		u.no_strength = !strength_all_shown(a);
		u.auto_hours = auto_hours_value();
		u.theme = look_accent_name();
		u.home = home_name(a->home_layout);
		u.order = g_order_release ? T(S_PANEL_ORDER_RELEASE) : T(S_PANEL_ORDER_AZ);
		{
			static char hs[24];

			u.homesys = homesys_value(hs, sizeof hs);
		}
		u.battery = g_batt_mode == BATT_TIME ? T(S_PANEL_BATTERY_TIME)
		          : g_batt_mode == BATT_PCT ? T(S_PANEL_BATTERY_PERCENTAGE) : T(S_PANEL_BATTERY_BARS);
		u.ui_sounds  = uisound_setting_name(uisound_setting());
		u.menu_music = bgm_menu_value(bgm_on(), bgm_count());
		u.auto_off  = a->auto_off;
		u.power_hold = powerhold_mode();
		u.saver     = g_saver_s;
		{
			static char lv[16];
			u.lights = u.no_lights ? NULL : side_lights_value(plat_side_lights(), lv, sizeof lv);
		}
		{
			static char clk[32];
			time_t t = time(NULL);

			/* In a shot, --status's time or none, as the corner's */
			if (shot_time(&t)) clock_label(t, g_clock24, clk, sizeof clk);
			else               clk[0] = '\0';
			u.clock = clk[0] ? clk : NULL;
		}
		u.clock_24h = g_clock24;
		u.audio_policy = ao.policy;
		u.audio_dest   = aout_actual(&ao);
		u.audio_channels = g_channels;
		u.audio_mix    = aout_channels_for(g_channels, u.audio_dest, plat_speaker_mono());

		/* The connected headset's name.
		 *
		 * Cached for two seconds, the same way the Wi-Fi row is and for the
		 * same reason: this is rebuilt every menu frame and the answer costs
		 * a fork. The first version read /tmp/zyperos_btsink to avoid that,
		 * which is what made this row say "not connected" while the Bluetooth
		 * screen said the opposite - launch.sh writes that file on a
		 * twenty-second poll. One source for both now. */
		{
			static char btname[BT_NAME_MAX];
			static unsigned next_bt;
			unsigned now = plat_now_ms();

			if (next_bt == 0 || now >= next_bt) {
				bt_device bd[BT_MAX];
				int nb = bt_bonded(bd, BT_MAX), i;

				next_bt = now + 2000;
				btname[0] = '\0';
				if (nb > 0 && bt_status() == BT_READY) {
					bt_mark_connected_now(bd, nb);
					for (i = 0; i < nb; i++)
						if (bd[i].connected) {
							snprintf(btname, sizeof btname, "%s", bd[i].name);
							break;
						}
				}
			}
			u.bt_name = btname[0] ? btname : NULL;
		}
	}
	menu_choices(a, sys, u.games);
	u.pm_choice = g_pm_choice;
	u.sm_choice = g_sm_choice;
	u.smm_choice = g_smm_choice;
	return sys_menu_build(&u, out, b, heading);
}

/* ---------- MENU > Display: Auto Hours and Theme (DESIGN.md 3.5, 11.42) ---- */

/* The volume and brightness bar the launcher draws (src/osdbar.c) in the
 * look's colors, or the tokens' for the look as it ships */
void look_bar_colors(void)
{
	int t;

	for (t = 0; t < DS_THEMES; t++)
		osdbar_colors(t, look_is_default(look_now()) ? NULL : theme_at(t));
}

/* The look and Auto's hours as the database keeps them (src/look.h,
 * src/theme.h): read at start before theme_init, and written as a row
 * changes. Words for the look, minutes for the hours. */
void look_load(void)
{
	char ac[16], li[16], dk[16], co[16], zy[8];
	look l;

	db_get_str(db_dev(), "look.accent", ac, sizeof ac, DS_ACCENT_ID[0]);
	db_get_str(db_dev(), "look.light", li, sizeof li, DS_TONE_ID[DS_LIGHT][0]);
	db_get_str(db_dev(), "look.dark", dk, sizeof dk, DS_TONE_ID[DS_DARK][0]);
	db_get_str(db_dev(), "look.corners", co, sizeof co, "rounded");
	db_get_str(db_dev(), "look.zyper", zy, sizeof zy, "on");
	l = look_parse(ac, li, dk, co, zy);
	look_set(&l);
	look_bar_colors();
	theme_set_hours(db_get_int(db_dev(), "auto.light", THEME_LIGHT_FROM),
	                db_get_int(db_dev(), "auto.dark", THEME_DARK_FROM));
}

/* A new look: on, kept, and every screen told. The bar's other programs read
 * it from the theme file theme_init writes (themefile.h), so that is written
 * again too; cards and marks drawn in the old colors are drawn again. */
static void look_change(app *a, const look *l)
{
	look_set(l);
	look_bar_colors();
	db_set_str(db_dev(), "look.accent", DS_ACCENT_ID[l->accent]);
	db_set_str(db_dev(), "look.light", DS_TONE_ID[DS_LIGHT][l->tone[DS_LIGHT]]);
	db_set_str(db_dev(), "look.dark", DS_TONE_ID[DS_DARK][l->tone[DS_DARK]]);
	db_set_str(db_dev(), "look.corners", l->square ? "square" : "rounded");
	db_set_str(db_dev(), "look.zyper", l->zyper_off ? "off" : "on");
	theme_init(theme_appearance());
	theme_changed(a);
}

/* A time of day, `m` minutes in, as MENU > Clock writes one: "7:00 AM",
 * or "07:00" in 24-hour */
static void time_of_day(int m, char *out, size_t n)
{
	int h = m / 60, mi = m % 60;

	if (g_clock24) snprintf(out, n, "%02d:%02d", h, mi);
	else snprintf(out, n, T(S_LOOK_TIME_12H), h % 12 ? h % 12 : 12, mi,
	              T(h < 12 ? S_CLOCK_AM : S_CLOCK_PM));
}

/* Auto Hours' value in MENU: "7:00 AM to 7:00 PM" */
static const char *auto_hours_value(void)
{
	static char v[64];
	char l[24], d[24];
	int lm, dm;

	theme_hours(&lm, &dm);
	time_of_day(lm, l, sizeof l);
	time_of_day(dm, d, sizeof d);
	snprintf(v, sizeof v, T(S_LOOK_HOURS), l, d);
	return v;
}

static const unsigned char HOURS_SOUND[2] = { MENU_SND_RING, MENU_SND_RING };

/* Auto Hours' two rows' choices (menu.h's menu_choice): the day's half
 * hours from midnight, as hours_key steps them, so A opens their list */
static menu_choice g_hours_choice[2];

int hours_build(void *ctx, menu_row *rows, int max, const char **heading)
{
	static const char *lab[24 * 60 / THEME_HOURS_STEP];
	static char buf[24 * 60 / THEME_HOURS_STEP][24];
	_Static_assert(24 * 60 / THEME_HOURS_STEP <= MENU_CHOICES_MAX, "Auto Hours' list");
	hours_view *v = ctx;
	int lm, dm, i, n = 24 * 60 / THEME_HOURS_STEP;

	(void)max;
	theme_hours(&lm, &dm);
	time_of_day(lm, v->l, sizeof v->l);
	time_of_day(dm, v->d, sizeof v->d);
	*heading = T(S_SYSMENU_AUTO_HOURS);
	rows[0] = (menu_row){ T(S_LOOK_LIGHT_FROM), v->l, true };
	rows[1] = (menu_row){ T(S_LOOK_DARK_FROM), v->d, true };
	for (i = 0; i < n; i++) {
		time_of_day(i * THEME_HOURS_STEP, buf[i], sizeof buf[i]);
		lab[i] = buf[i];
	}
	g_hours_choice[0] = (menu_choice){ .n = (unsigned char)n, .cur = (unsigned char)(lm / THEME_HOURS_STEP), .labels = lab };
	g_hours_choice[1] = (menu_choice){ .n = (unsigned char)n, .cur = (unsigned char)(dm / THEME_HOURS_STEP), .labels = lab };
	return 2;
}

static menu_result hours_key(app *a, void *ctx, in_button key, int sel)
{
	int d = key == IN_RIGHT || key == IN_ACCEPT ? 1 : key == IN_LEFT ? -1 : 0;
	int lm, dm, *m, was = theme_index();
	const int day = 24 * 60;

	(void)ctx;
	if (!d || sel < 0 || sel > 1) return MENU_STAY;
	theme_hours(&lm, &dm);
	m = sel == 0 ? &lm : &dm;
	*m = ((*m + d * THEME_HOURS_STEP) % day + day) % day;
	theme_set_hours(lm, dm);
	db_set_int(db_dev(), "auto.light", lm);
	db_set_int(db_dev(), "auto.dark", dm);
	theme_init(theme_appearance());
	if (theme_index() != was) theme_changed(a);
	return MENU_CHANGED;
}

static void auto_hours_screen(app *a)
{
	hours_view v = { .a = a };
	menu_style st = { .fixed_w = menu_std_width(a), .accent = MENU_ZYPEROS,
	                  .sound = HOURS_SOUND, .choices = g_hours_choice };

	menu_run(a, &st, hours_build, hours_key, &v);
}

/* A theme's chip, 64 by 36: its ground in its edge, its accent on it, so
 * the row says what the theme is now (DESIGN.md 11.42) */
static void look_chip(app *a, const ds_theme *t, int x, int cy)
{
	SDL_Rect c = { x, cy - 18, 64, 36 };

	ui_round_rect(a->r, &c, DS_RADIUS_CHIP + 1, ui_col(t->edge, 255));
	ui_round_rect(a->r, &(SDL_Rect){ c.x + 3, c.y + 3, c.w - 6, c.h - 6 }, DS_RADIUS_CHIP - 2,
	              ui_col(t->ground, 255));
	ui_round_rect(a->r, &(SDL_Rect){ c.x + 12, cy - 6, 40, 12 }, 6, ui_col(t->accent, 255));
}

/* The marks before Theme's values: the accent's dot on Accent, each theme's
 * chip on its background's row, and both on MENU's Theme row */
void look_row_mark(app *a, const menu_row *row, int right, int cy, float on_fill)
{
	const char *l = row->label;

	(void)on_fill;
	if (l == T(S_LOOK_BG_LIGHT)) look_chip(a, theme_at(DS_LIGHT), right - 64, cy);
	else if (l == T(S_LOOK_BG_DARK)) look_chip(a, theme_at(DS_DARK), right - 64, cy);
	else if (l == T(S_SYSMENU_THEME)) {
		look_chip(a, theme_at(DS_DARK), right - 64, cy);
		look_chip(a, theme_at(DS_LIGHT), right - 64 - DS_GRID - 64, cy);
	} else if (l == T(S_LOOK_ACCENT)) {
		const ds_rgb ac = DS_ACCENT[look_now()->accent];

		ui_round_rect(a->r, &(SDL_Rect){ right - 34, cy - 17, 34, 34 }, 17,
		              ui_col(theme()->edge, 255));
		ui_round_rect(a->r, &(SDL_Rect){ right - 32, cy - 15, 30, 30 }, 15,
		              ui_col(theme()->surface, 255));
		ui_round_rect(a->r, &(SDL_Rect){ right - 28, cy - 11, 22, 22 }, 11,
		              (SDL_Color){ ac.r, ac.g, ac.b, 255 });
	}
}

/* One theme, small: the home with a menu over it, drawn from shapes (text is
 * never under 37 units), in that theme as the look has it now. */
static void look_thumb(app *a, const ds_theme *t, const SDL_Rect *q)
{
	SDL_Renderer *r = a->r;
	const int rad = 16;
	int mx = q->x + q->w * 48 / 100, my = q->y + 12, mw = q->w * 48 / 100, mh = q->h - 24;
	int bx = q->x + 16, i;
#define BAR(x, y, w, h, c) ui_round_rect(r, &(SDL_Rect){ (x), (y), (w), (h) }, (h) / 2, ui_col((c), 255))

	ui_round_rect(r, q, rad, ui_col(t->edge, 255));
	ui_round_rect(r, &(SDL_Rect){ q->x + 2, q->y + 2, q->w - 4, q->h - 4 }, rad - 2,
	              ui_col(t->ground, 255));
	/* the home: the system's name and count, A, the art, the dock and its
	 * mark under the one in focus */
	BAR(bx, q->y + 22, q->w * 22 / 100, 13, t->text);
	BAR(bx, q->y + 44, q->w * 13 / 100, 8, t->text_dim);
	ui_round_rect(r, &(SDL_Rect){ bx, q->y + 60, 16, 16 }, 8, ui_col(t->accent, 255));
	BAR(bx + 22, q->y + 64, q->w * 11 / 100, 8, t->text_soft);
	ui_round_rect(r, &(SDL_Rect){ q->x + q->w * 27 / 100, q->y + 14, q->w * 16 / 100, q->w * 12 / 100 },
	              6, ui_col(t->raised, 255));
	for (i = 0; i < 4; i++)
		ui_round_rect(r, &(SDL_Rect){ bx + i * (q->w / 10), q->y + q->h - 46, q->w / 14, q->w / 14 },
		              4, ui_col(i == 2 ? t->text_soft : t->text_dim, 255));
	BAR(bx + 2 * (q->w / 10), q->y + q->h - 14, q->w / 14, 4, t->accent_ink);
	/* a menu over it: its heading, rule and bar, three rows, one in focus */
	ui_round_rect(r, &(SDL_Rect){ mx, my, mw, mh }, 12, ui_col(t->edge, 255));
	ui_round_rect(r, &(SDL_Rect){ mx + 2, my + 2, mw - 4, mh - 4 }, 10, ui_col(t->surface, 255));
	BAR(mx + mw / 2 - mw / 6, my + 12, mw / 3, 8, t->text_soft);
	ui_round_rect(r, &(SDL_Rect){ mx + 10, my + 28, mw - 20, 2 }, 0, ui_col(t->edge, 255));
	BAR(mx + mw / 2 - mw / 6, my + 27, mw / 3, 4, t->accent_ink);
	BAR(mx + 14, my + 40, mw * 35 / 100, 8, t->text_soft);
	BAR(mx + mw - 14 - mw / 5, my + 40, mw / 5, 8, t->text_dim);
	ui_round_rect(r, &(SDL_Rect){ mx + 6, my + 56, mw - 12, 22 }, 6, ui_col(t->accent, 255));
	BAR(mx + 14, my + 63, mw * 40 / 100, 8, t->on_accent);
	BAR(mx + 14, my + mh - 18, mw * 30 / 100, 8, t->text_soft);
	BAR(mx + mw - 14 - mw / 4, my + mh - 18, mw / 4, 8, t->text_dim);
	/* Zyper in its corner, a dot at this size, while he is on */
	if (!look_zyper_off())
		ui_round_rect(r, &(SDL_Rect){ mx + mw - 22, my + 7, 12, 12 }, 6, ui_col(t->text_dim, 255));
#undef BAR
}

/* Theme's picture: Beard and Salt & Pepper as the look has them, side by
 * side, Light at the left whatever is on */
void look_header(app *a, const SDL_Rect *q)
{
	int gap = DS_GRID * 3, w = (q->w - gap) / 2;

	look_thumb(a, theme_at(DS_LIGHT), &(SDL_Rect){ q->x, q->y, w, q->h });
	look_thumb(a, theme_at(DS_DARK), &(SDL_Rect){ q->x + w + gap, q->y, w, q->h });
}

static const unsigned char LOOK_SOUND[LK_ROWS] = {
	MENU_SND_RING, MENU_SND_RING, MENU_SND_RING, MENU_SND_RING, MENU_SND_RING, 0 };
static const int ACCENT_NAME[DS_ACCENTS] = {
	S_LOOK_ACCENT_LIME, S_LOOK_ACCENT_GREEN, S_LOOK_ACCENT_TEAL, S_LOOK_ACCENT_BLUE,
	S_LOOK_ACCENT_PURPLE, S_LOOK_ACCENT_PINK, S_LOOK_ACCENT_RED, S_LOOK_ACCENT_ORANGE,
	S_LOOK_ACCENT_YELLOW, S_LOOK_ACCENT_SILVER };
static const int TONE_NAME[DS_THEMES][DS_TONES] = {
	{ S_LOOK_TONE_WARM, S_LOOK_TONE_NEUTRAL, S_LOOK_TONE_COOL },
	{ S_LOOK_TONE_BLACK, S_LOOK_TONE_DEEPBLUE, S_LOOK_TONE_DARKGREY } };

static const char *look_accent_name(void)
{
	return T(ACCENT_NAME[look_now()->accent]);
}

/* Theme's rows' choices (menu.h's menu_choice): the accents and each
 * theme's grounds, as look_key steps them, so A opens their list */
static menu_choice g_lk_choice[LK_ROWS];

int look_build(void *ctx, menu_row *rows, int max, const char **heading)
{
	static const char *acc[DS_ACCENTS], *tone[DS_THEMES][DS_TONES];
	const look *l = look_now();
	int i, t;

	(void)ctx; (void)max;
	for (i = 0; i < DS_ACCENTS; i++) acc[i] = T(ACCENT_NAME[i]);
	for (t = 0; t < DS_THEMES; t++)
		for (i = 0; i < DS_TONES; i++) tone[t][i] = T(TONE_NAME[t][i]);
	memset(g_lk_choice, 0, sizeof g_lk_choice);
	g_lk_choice[LK_ACCENT] = (menu_choice){ .n = DS_ACCENTS, .cur = (unsigned char)l->accent, .labels = acc };
	g_lk_choice[LK_LIGHT] = (menu_choice){ .n = DS_TONES, .cur = (unsigned char)l->tone[DS_LIGHT],
	                                       .labels = tone[DS_LIGHT] };
	g_lk_choice[LK_DARK] = (menu_choice){ .n = DS_TONES, .cur = (unsigned char)l->tone[DS_DARK],
	                                      .labels = tone[DS_DARK] };
	*heading = T(S_SYSMENU_THEME);
	rows[LK_ACCENT]  = (menu_row){ T(S_LOOK_ACCENT), T(ACCENT_NAME[l->accent]), true };
	rows[LK_LIGHT]   = (menu_row){ T(S_LOOK_BG_LIGHT), T(TONE_NAME[DS_LIGHT][l->tone[DS_LIGHT]]), true };
	rows[LK_DARK]    = (menu_row){ T(S_LOOK_BG_DARK), T(TONE_NAME[DS_DARK][l->tone[DS_DARK]]), true };
	rows[LK_CORNERS] = (menu_row){ T(S_LOOK_CORNERS), T(l->square ? S_LOOK_SQUARE : S_LOOK_ROUNDED), true };
	rows[LK_ZYPER]   = (menu_row){ T(S_LOOK_ZYPER), T(l->zyper_off ? S_MENU_OFF_TITLE : S_MENU_ON_TITLE), true };
	rows[LK_RESET]   = (menu_row){ T(S_LOOK_RESET), NULL, true };
	return LK_ROWS;
}

static menu_result look_key(app *a, void *ctx, in_button key, int sel)
{
	int d = key == IN_RIGHT || key == IN_ACCEPT ? 1 : key == IN_LEFT ? -1 : 0;
	look l = *look_now();

	(void)ctx;
	if (!d || sel < 0 || sel >= LK_ROWS) return MENU_STAY;
	switch (sel) {
	case LK_ACCENT:  l.accent = (l.accent + d + DS_ACCENTS) % DS_ACCENTS; break;
	case LK_LIGHT:   l.tone[DS_LIGHT] = (l.tone[DS_LIGHT] + d + DS_TONES) % DS_TONES; break;
	case LK_DARK:    l.tone[DS_DARK] = (l.tone[DS_DARK] + d + DS_TONES) % DS_TONES; break;
	case LK_CORNERS: l.square = !l.square; break;
	case LK_ZYPER:   l.zyper_off = !l.zyper_off; break;
	case LK_RESET:
		if (key != IN_ACCEPT) return MENU_STAY;
		l = LOOK_DEFAULT;
		break;
	}
	look_change(a, &l);
	return MENU_CHANGED;
}

static void look_screen(app *a)
{
	menu_style st = { .fixed_w = menu_std_width(a), .accent = MENU_ZYPEROS,
	                  .sound = LOOK_SOUND, .choices = g_lk_choice, .row_mark = look_row_mark,
	                  .header = look_header, .header_h = 150 };

	menu_run(a, &st, look_build, look_key, NULL);
}

menu_result sysmenu_key(app *a, void *ctx, in_button key, int sel)
{
	int d = key == IN_RIGHT ? 1 : key == IN_LEFT ? -1 : 0;

	(void)ctx;

	if (a->screen == SCREEN_GAMES) {
		/* Muse's menu is its own rows - see SM_MUSE_ROWS - and which is
		 * where depends on what its shelf shows, so the enum's row numbers
		 * below mean nothing on it: sys_menu_muse_rows says. Sort By goes on
		 * to the Sort By below like any shelf's. Album Art is a dead row off
		 * the network, and the runner does not land on dead rows. */
		if (is_muse(&a->sys.systems[a->sys_cursor])) {
			sm_muse_row ids[SM_MUSE_ROWS];
			int n = sys_menu_muse_rows(muse_rows_kind(), muse_kinds() > 1,
			                           !plat_has_wifi() && !net_internet(NULL), ids);
			sm_muse_row id = sel >= 0 && sel < n ? ids[sel] : SMM_COUNT;

			if (id == SMM_SORT) {
				sel = SM_SORT;
			} else if (id == SMM_LOCK) {
				/* Closes the menu; the screen underneath locks. */
				if (key != IN_ACCEPT) return MENU_STAY;
				muse_ask_lock();
				return MENU_DONE;
			} else if (id == SMM_SHUFFLE) {
				/* Closes the menu; Now Playing opens on the first song. */
				if (key != IN_ACCEPT) return MENU_STAY;
				muse_ask_shuffle();
				return MENU_DONE;
			} else if (id == SMM_SHOW) {
				/* All, then the kinds the card has, round in either
				 * direction (A opens their list, menu_choose). ML_ALL is the
				 * number after the three, so the round is All, Music,
				 * Audiobooks, Videos. */
				if (d || key == IN_ACCEPT) {
					int k = muse_kind_shown(), step = d < 0 ? ML_ALL : 1, t;

					for (t = 0; t <= ML_ALL; t++) {
						k = (k + step) % (ML_ALL + 1);
						if (ml_count_kind(&g_muse, (ml_kind)k)) break;
					}
					muse_show(&a->view[a->sys_cursor], (ml_kind)k);
				}
				return MENU_STAY;
			} else {
				if (key != IN_ACCEPT) return MENU_STAY;
				if (id == SMM_ART) { album_art_screen(a); return MENU_STAY; }
				if (id != SMM_RESCAN) return MENU_STAY;
				wait_panel(a, "MP4", T(S_LIBRARY_SCANNING));
				rescan_all(a);
				return MENU_DONE;
			}
		}
		/* Favorites' menu is its own rows too, Games and Sort By - see
		 * SM_FAV_ROWS - so its Sort By is row 1 and not SM_SORT. Nothing here
		 * knew that from the day the menu was cut to two rows, and left and
		 * right on Favorites' Sort By did nothing at all. */
		if (a->view[a->sys_cursor].owner)
			sel = sel == 1 ? SM_RANDOM : sel == 2 ? SM_SORT : sel == 3 ? SM_VIEW : SM_GAMES;
		/* And a console's rows lose Box Art on a device with no radio, which
		 * moves Rescan Folder up one: which row is which comes from the same
		 * list the build used. */
		else if (!is_muse(&a->sys.systems[a->sys_cursor])) {
			sys_ui dev = { .no_wifi = !plat_has_wifi(), .online = net_internet(NULL),
			               .dslang = dslang_row_shown(a, a->sys_cursor) ? "" : NULL,
			               .no_strength = !shader_has_strength(a->view[a->sys_cursor].shader) };
			sm_row ids[SM_ROWS];
			int n = sys_menu_system_rows(&dev, ids);

			sel = sel >= 0 && sel < n ? (int)ids[sel] : SM_ROWS;
		}
		/* The core, on a shelf that lists more than one: left, right or A
		 * steps to the next, saved at once like the display mode. The next
		 * game launched from here runs on it. */
		if ((d || key == IN_ACCEPT) && sel == SM_CORE &&
		    cfg_core_count(&a->sys.systems[a->sys_cursor]) > 1) {
			core_step(a, a->sys_cursor, d ? d : 1);
			return MENU_STAY;
		}
		/* The Nintendo DS's game language, stepped round the eight the same
		 * way, saved at once: the next game started runs in it. */
		if ((d || key == IN_ACCEPT) && sel == SM_DSLANG) {
			dsl_choice c = dslang_step(dslang_load(), d ? d : 1);

			db_set_str(db_dev(), DSLANG_SETTING, dslang_id(c));
			return MENU_STAY;
		}
		/* The shader, stepped round on left, right and A and kept at once,
		 * for this system alone: its next game plays with it. MENU's row
		 * sets every system's. */
		if ((d || key == IN_ACCEPT) && sel == SM_SHADER) {
			sysview *v = &a->view[a->sys_cursor];

			v->shader = shader_step(v->shader, d ? d : 1);
			shader_save(a, a->sys_cursor);
			return MENU_STAY;
		}
		/* Its strength, on left and right, stopping at the ends, kept at
		 * once for this system alone (src/strength.h) */
		if (d && sel == SM_STRENGTH) {
			sysview *v = &a->view[a->sys_cursor];

			v->strength = strength_step(v->strength, d);
			strength_write(db_dev(), a->sys.systems[a->sys_cursor].tag, v->strength);
			return MENU_STAY;
		}
		/* Display mode, saved the moment it changes because there is no
		 * confirm step to hang the write off. */
		/* View: the covers, the grid or a list, this shelf's (DESIGN.md
		 * 11.39, 11.47). Right steps it forward, Left back, round the
		 * three; A opens the three as a list (menu_choices' SM_VIEW, the
		 * runner's). The shelf under the menu turns with it, on the game it
		 * was on. */
		if ((d || key == IN_ACCEPT) && sel == SM_VIEW) {
			view_turn(a, a->sys_cursor, d < 0 ? -1 : 1);
			return MENU_STAY;
		}
		if (d && sel == SM_DISPLAY) {
			sysview *v = &a->view[a->sys_cursor];

			v->dmode = (v->dmode + d + DMODE_COUNT) % DMODE_COUNT;
			display_save(a);
			return MENU_STAY;
		}
		/* Sort order, on the same left/right idiom.
		 *
		 * The cursor follows the GAME, not its index. Re-sorting under a
		 * cursor left where it sat means closing the menu onto a different
		 * game than the one that was selected when it opened - the shelf
		 * would appear to have jumped on its own. Which game you were
		 * looking at is the thing that survives a reorder; where it happened
		 * to sit in the old order is not. On Muse's shelf the same goes for
		 * the album, whose folder is its `file`. */
		if (d && sel == SM_SORT) {
			sysview *v = &a->view[a->sys_cursor];
			char keep[LIB_PATH];
			int i, keep_owner = -1;

			keep[0] = '\0';
			if (v->cursor >= 0 && v->cursor < v->list.count) {
				snprintf(keep, sizeof keep, "%s",
				         v->list.items[v->cursor].file);
				/* On Favorites a file name is not enough: the same one can
				 * be on the shelf twice, from two systems. */
				if (v->owner) keep_owner = v->owner[v->cursor];
			}

			v->sort = is_muse(&a->sys.systems[a->sys_cursor])
			        ? (v->sort + d + ML_ORDERS) % ML_ORDERS
			        : sort_step(v->sort, d);
			sort_shelf(a, a->sys_cursor);
			sort_save(a);

			/* The textures are indexed by position, so they moved with
			 * nothing. Dropping them lets the next frame fetch each card's
			 * art for where it now sits. */
			free_view_textures(v);

			v->cursor = 0;
			for (i = 0; i < v->list.count; i++)
				if (!strcmp(v->list.items[i].file, keep) &&
				    (!v->owner || v->owner[i] == keep_owner)) {
					v->cursor = i;
					break;
				}
			cf_reset(&v->cf, v->cursor);
	games_reset(&v->row, v->cursor, v->list.count, plat_now_ms());
			return MENU_STAY;
		}
		if (key != IN_ACCEPT) return MENU_STAY;
		/* One of this shelf's games: the menu closes and it starts */
		if (sel == SM_RANDOM) {
			if (!random_pick(a, false, &g_pick.shelf, &g_pick.game)) return MENU_STAY;
			g_pick.hold_ms = 700;
			return MENU_DONE;
		}
		/* The system's own color, not the menu's. This acts on the shelf you
		 * are looking at, and menu_draw already follows that rule everywhere
		 * else. */
		if (sel == SM_BOXART)
			scraper_page(a, a->sys.systems[a->sys_cursor].folder);
		if (sel == SM_RESCAN) {
			wait_panel(a, a->sys.systems[a->sys_cursor].name, T(S_LIBRARY_SCANNING));
			rescan_all(a);
			/* Close, rather than redraw this menu over a shelf that may have
			 * just lost the system it was built for. Acting and closing is
			 * also what pressing it means. */
			return MENU_DONE;
		}
		return MENU_STAY;
	}

	/* The row's identity rather than its position: a device with no radio has
	 * fewer of them, and the same list the build used says which is which. */
	{
		sys_ui dev = { .no_wifi = !plat_has_wifi(), .no_bt = !plat_has_bluetooth(),
		               .cable_xfer = xfer_link_up(), .online = net_internet(NULL),
		               .no_developer = !dev_offered(),
		               .no_lights = !plat_battery_lights(),
		               .no_charge_limit = !plat_charge_limit_supported(),
		               .no_usb_power = !usb_power_offered(),
		               .no_usb_wait = !usb_wait_row(),
		               /* the same rule as the build's (menu_build): a card with
		                * the update program has the row */
		               .update = up_have() ? "" : NULL,
		               .no_strength = !strength_all_shown(a) };
		pm_row ids[PM_ROWS];
		int n = sys_menu_tab_rows(&dev, (pm_tab)g_tabs.at, ids);

		/* L1 and R1 go round the tabs (DESIGN.md 11.3): the row each tab
		 * is on is kept, and the cursor lands on the next tab's own */
		if (key == IN_L1 || key == IN_R1) {
			int dir = key == IN_R1 ? +1 : -1, k;

			if (sel >= 0 && sel < n) g_tab_row[g_tabs.at] = ids[sel];
			tabs_turn(&g_tabs, dir);
			n = sys_menu_tab_rows(&dev, (pm_tab)g_tabs.at, ids);
			for (k = 0; k < n && ids[k] != g_tab_row[g_tabs.at]; k++) ;
			g_menu_sel_to = k < n ? k : 0;
			moved_sound(a, key);
			redraw_now();
			return MENU_STAY;
		}
		sel = sel >= 0 && sel < n ? (int)ids[sel] : PM_ROWS;
	}

	/* Auto Off, on the left/right idiom Display mode uses. A step past either
	 * end changes nothing, and Zyper is told only of a change. */
	if (d && sel == PM_SLEEP) {
		int k, at = 0, was = a->auto_off;

		for (k = 0; k < AUTO_OFF_COUNT; k++)
			if (AUTO_OFF[k] == a->auto_off) { at = k; break; }
		at += d;
		if (at < 0) at = 0;
		if (at >= AUTO_OFF_COUNT) at = AUTO_OFF_COUNT - 1;
		a->auto_off = AUTO_OFF[at];
		auto_off_save(a->auto_off);
		/* From now, not from whenever the last countdown began: choosing 30s
		 * should not inherit two minutes of an old one already spent. */
		a->idle.since_ms = plat_now_ms();
		return a->auto_off != was ? MENU_CHANGED : MENU_STAY;
	}
	/* Power Button, Press or Hold (src/powerhold.h): two choices, so A
	 * switches it as Left and Right do. Saved as its word, which portkeys
	 * reads from the same database as a port starts. */
	if (sel == PM_POWER && (d || key == IN_ACCEPT)) {
		powerhold_set_mode(!powerhold_mode());
		db_set_str(db_dev(), POWERHOLD_KEY, powerhold_mode() ? "hold" : "press");
		redraw_now();
		return MENU_CHANGED;
	}
	/* Screensaver, on Auto Off's idiom: left and right step the delay, off
	 * at one end; a step past either end changes nothing. */
	if (d && sel == PM_SAVER) {
		int was = g_saver_s;

		g_saver_s = saver_step(g_saver_s, d);
		saver_save(g_saver_s);
		return g_saver_s != was ? MENU_CHANGED : MENU_STAY;
	}
	/* System Order: the shelves read again in the other order, the cursor
	 * kept on its system by tag (rescan_all), as File Transfer reads them
	 * when it closes. Once per press, never per frame. */
	if (sel == PM_ORDER && (d || key == IN_ACCEPT)) {
		g_order_release = !g_order_release;
		db_set_str(db_dev(), "systems.order", g_order_release ? "release" : "az");
		rescan_all(a);
		redraw_now();
		return MENU_CHANGED;
	}
	/* The home layout is independent of Appearance and the selected system. */
	if (sel == PM_HOME && (d || key == IN_ACCEPT)) {
		/* Spotlight, Gallery, Grid and round: Right on, Left back; A opens
		 * the three as a list (menu_choices' PM_HOME, the runner's) */
		a->home_layout = (home_layout)(((int)a->home_layout + (d < 0 ? HOME_LAYOUTS - 1 : 1)) % HOME_LAYOUTS);
		db_set_str(db_dev(), "home", home_id(a->home_layout));
		home_reset(&a->home, a->sys_cursor, a->sys.count, plat_now_ms());
		redraw_now();
		return MENU_CHANGED;
	}
	/* Charge Limit, round Off, 80%, 85% and 90% as Side Lights goes round its
	 * four (A opens their list, menu_choose): saved, and the kernel told at once, so the
	 * charger stops or goes on by its next reading */
	if (sel == PM_CHARGE && (d || key == IN_ACCEPT)) {
		int k, at = 0;

		for (k = 0; k < CHARGE_LIMITS_COUNT; k++)
			if (CHARGE_LIMITS[k] == g_charge_limit) at = k;
		at = (at + (d ? d : 1) + CHARGE_LIMITS_COUNT) % CHARGE_LIMITS_COUNT;
		g_charge_limit = CHARGE_LIMITS[at];
		db_set_int(db_dev(), "chargelimit", g_charge_limit);
		charge_limit_apply();
		redraw_now();
		return MENU_CHANGED;
	}
	/* Light, Dark, Auto, stepped either way (A opens their list), like the
	 * choices, and applied the moment it changes: the theme is resolved
	 * again here, saved as its word, and the theme file the bar's other
	 * programs read is rewritten by theme_init - so the next frame is drawn
	 * from the new theme(), and the next bar over a game or a port is drawn
	 * in it. Saved even when the theme it comes to is the one already on:
	 * Auto at noon is Beard and so is Light, and they are still two
	 * settings. */
	if (sel == PM_APPEARANCE && (d || key == IN_ACCEPT)) {
		appearance k = appearance_step(theme_appearance(), d ? d : 1);
		int was = theme_index();

		theme_init(k);
		db_set_str(db_dev(), "appearance", appearance_id(k));
		boot_theme_changed();
		if (theme_index() != was) theme_changed(a);
		return MENU_CHANGED;
	}

	/* The shader for every system, stepped round (A opens the list):
	 * kept as MENU's, and each system's own choice goes with it
	 * (shader_set_all). From "per system" a step goes on from MENU's last. */
	if (sel == PM_SHADER && (d || key == IN_ACCEPT)) {
		shader_set_all(a, shader_step(g_shader_all, d ? d : 1));
		return MENU_CHANGED;
	}
	/* Strength for every system, on Auto Off's idiom: left and right, a
	 * step past either end changing nothing; kept as MENU's, and each
	 * system's own goes with it. From "per system" a step goes on from
	 * MENU's last (src/strength.h). */
	if (d && sel == PM_STRENGTH) {
		int was = strength_read_all(db_dev()), k = strength_step(was, d), i;
		bool moved = k != was;

		for (i = 0; i < a->sys.count; i++) {
			if (strength_clamp(a->view[i].strength) != k) moved = true;
			a->view[i].strength = k;
		}
		strength_write_all(db_dev(), k);
		return moved ? MENU_CHANGED : MENU_STAY;
	}

	/* On or off, so left, right and A all turn it over. Off lets the link
	 * go at once unless something holds it, and the next computer is asked
	 * (src/usblink.h); the card's file says so for the next boot too. */
	if (sel == PM_DEVELOPER && (d || key == IN_ACCEPT)) {
		if (usblink_dev_image()) {
			usblink_set_developer(db_dev(), P_USERDATA, !usblink_developer(db_dev()));
		} else {
			/* A release image: the row is there only while Developer Mode is
			 * on, and opens its page, where A turns it off. Unlocking again
			 * is the hold on About. */
			dev_screen(a);
		}
		return MENU_CHANGED;
	}

	/* Ask or Automatic, so left, right and A all turn it over; Automatic
	 * says what it does and its one risk as it is chosen */
	if (sel == PM_USB && (d || key == IN_ACCEPT)) {
		usb_auto_save(!g_usb_auto);
		if (g_usb_auto) usb_auto_message(a);
		return MENU_CHANGED;
	}
	/* A plug waiting: the question again, for that plug */
	if (sel == PM_USB_WAIT && key == IN_ACCEPT) {
		if (g_usb_wait) usb_power_ask(a, g_usb_wait);
		return MENU_CHANGED;
	}

	/* 12-hour or 24-hour, two positions, so left, right and A all turn it
	 * over. The shelf's corner and the row above it read g_clock24 as they
	 * are drawn, so the next frame is in the new one. */
	if (sel == PM_HOURS && (d || key == IN_ACCEPT)) {
		clock24_save(!g_clock24);
		return MENU_CHANGED;
	}
	/* Bars, Percentage or Time Left: three positions round, as Charge Limit
	 * goes, Right forward and Left back (A opens the list), saved as its word */
	if (sel == PM_BATTERY && (d || key == IN_ACCEPT)) {
		g_batt_mode = (g_batt_mode + (d < 0 ? BATT_MODES - 1 : 1)) % BATT_MODES;
		db_set_str(db_dev(), "battery_icon", BATT_IDS[g_batt_mode]);
		redraw_now();
		return MENU_CHANGED;
	}

	/* Two positions, so left and right and A all do the same thing: there is
	 * nothing to step through, only something to turn off and on. */
	if (sel == PM_AUDIO && (d || key == IN_ACCEPT)) {
		g_aout_policy = g_aout_policy == AOUT_AUTO ? AOUT_SPEAKER : AOUT_AUTO;
		aout_save(g_aout_policy);
		/* Forced: the resolved DEVICE may not have changed - pinning Speaker
		 * with a cable in is still the codec - but the setting did, and the
		 * next unplug must act on the new policy rather than on a cached
		 * device string that happens to match. */
		aout_apply(true);
		return MENU_CHANGED;
	}

	/* Five positions, Auto after mono right and before stereo, stepped either
	 * way like the card sets (A opens their list).
	 * Applied at once, to Muse and to Diatom if they are there to be told, so
	 * a player trying them over music hears each as it is chosen. */
	if (sel == PM_CHANNELS && (d || key == IN_ACCEPT)) {
		g_channels = (aout_channels)((g_channels + (d ? d : 1) + AOUT_CHANNELS_COUNT)
		                             % AOUT_CHANNELS_COUNT);
		channels_save(g_channels);
		aout_apply(false);
		return MENU_CHANGED;
	}

	/* Wood, Glass, Bubble, Off, stepped either way like Audio Channels, A
	 * going forward, and heard as each is chosen: the row is MENU_SND_OWN, so
	 * the runner plays nothing for it and its press is played here, in the
	 * voice it comes to, before the setting is written. Off is silent. */
	if (sel == PM_SOUNDS && (d || key == IN_ACCEPT)) {
		int k = uisound_setting_step(uisound_setting(), d ? d : 1);

		uisound_set(k);
		if (d) moved_sound(a, key);
		else   uisound_play(DS_SOUND_ACCEPT);
		db_set_str(db_dev(), "uisounds", uisound_setting_id(k));
		return MENU_STAY;
	}

	if (key != IN_ACCEPT) return MENU_STAY;
	switch (sel) {
	case PM_WIFI:         wifi_screen(a); break;
	/* A game from every system: the menu closes and it starts */
	case PM_RANDOM:
		if (!random_pick(a, true, &g_pick.shelf, &g_pick.game)) break;
		g_pick.hold_ms = 700;
		return MENU_DONE;
	case PM_XFER:         xfer_screen(a); break;
	case PM_LIGHTS:       lights_screen(a); break;
	/* The shelves the home shows; the home built again as it closes */
	case PM_HOMESYS:      homesys_screen(a); return MENU_CHANGED;
	case PM_LANGUAGE:     language_screen(a); return MENU_CHANGED;
	/* These three say how they went, which is Zyper's to answer */
	case PM_SCRAPER:      return scraper_screen(a);
	case PM_ACHIEVEMENTS: return ra_signin_screen(a);
	case PM_BT:           bt_screen(a); break;
	case PM_CLOCK:        clock_screen(a); break;
	case PM_MUSIC:        bgm_screen(a); break;
	case PM_STATS:        if (stats_screen(a)) return MENU_DONE; break;
	case PM_CONTROLS:     controls_screen(a); break;
	/* Closes the menu: the tutorial ends on the shelf, where its finish
	 * says to start playing. */
	case PM_TOUR:         tour_run(a, false); return MENU_DONE;
	case PM_STORAGE:      storage_screen(a); break;
	case PM_AUTOHOURS:    auto_hours_screen(a); return MENU_CHANGED;
	case PM_THEME:        look_screen(a); return MENU_CHANGED;
	case PM_UPDATE:       update_screen(a); break;
	case PM_ABOUT:        about_screen(a); break;
	/* A game by its name: the menu closes once one is picked, and it starts */
	case PM_SEARCH:
		if (!search_screen(a, &g_pick.shelf, &g_pick.game)) break;
		g_pick.hold_ms = 0;
		return MENU_DONE;
	default: break;
	}
	return MENU_STAY;
}

void zyperos_menu_for(app *a, int screen)
{
	sysmenu_ctx c;

	uisound_play(DS_SOUND_ACCEPT);                /* a menu opening */
	memset(&c, 0, sizeof c);
	c.a = a;
	c.screen = screen;
	menu_style st = zyperos_menu_style(a, screen);
	/* Each row's sound class, which the build sets beside the rows */
	st.sound = c.bufs.sound;
	/* And its choices, so an option row's A opens its list (menu_choose) */
	st.choices = c.bufs.choice;

	if (st.tabs) {
		sys_ui dev = { .no_wifi = !plat_has_wifi(), .no_bt = !plat_has_bluetooth(),
		               .cable_xfer = xfer_link_up(), .online = net_internet(NULL),
		               .no_developer = !dev_offered(),
		               .no_lights = !plat_battery_lights(),
		               .no_charge_limit = !plat_charge_limit_supported(),
		               .no_usb_power = !usb_power_offered(),
		               .no_usb_wait = !usb_wait_row(),
		               .update = up_have() ? "" : NULL,
		               .no_strength = !strength_all_shown(a) };
		pm_row ids[PM_ROWS];
		int n, k;

		/* A plug waiting for its answer: Connections, on USB Device */
		if (!dev.no_usb_wait && !dev.no_usb_power) {
			g_tabs.at = PT_CONNECTIONS;
			g_tab_row[PT_CONNECTIONS] = PM_USB_WAIT;
		}
		g_tabs.from = g_tabs.at;
		g_tabs.t0 = 0;
		g_menu_epoch++;
		n = sys_menu_tab_rows(&dev, (pm_tab)g_tabs.at, ids);
		for (k = 0; k < n && ids[k] != g_tab_row[g_tabs.at]; k++) ;
		st.start_sel = k < n ? k : 0;
		c.seen = st.start_sel;
		st.sel_out = &c.seen;
		g_pick.shelf = -1;
		menu_run(a, &st, sysmenu_build, sysmenu_key, &c);
		/* where it was closed, for the next time it opens; Strength may
		 * have come or gone with a Shader step */
		dev.no_strength = !strength_all_shown(a);
		n = sys_menu_tab_rows(&dev, (pm_tab)g_tabs.at, ids);
		if (c.seen >= 0 && c.seen < n) g_tab_row[g_tabs.at] = ids[c.seen];
	} else {
		g_pick.shelf = -1;
		menu_run(a, &st, sysmenu_build, sysmenu_key, &c);
	}
	/* A game Random Game or Search Games picked, once the menu has gone */
	if (g_pick.shelf >= 0 && a->running && !want_quit) pick_launch(a);
	g_pick.shelf = -1;
}

void zyperos_menu(app *a)
{
	zyperos_menu_for(a, a->screen);
}

void sysmenu_rows_report(app *a)
{
	static menu_row rows[MENU_MAX_ROWS];
	static menu_bufs b;
	hours_view hv = { .a = a };
	const char *h;
	int n, was = g_tabs.at;

	g_tabs.at = PT_TABS;   /* every one of MENU's rows (sys_menu_build) */
	n = menu_build(a, SCREEN_SYSTEMS, a->sys_cursor, rows, &b, &h);
	menu_rows_print("menu", rows, n, b.sound, b.choice);
	g_tabs.at = was;
	n = menu_build(a, SCREEN_GAMES, a->sys_cursor, rows, &b, &h);
	menu_rows_print("system", rows, n, b.sound, b.choice);
	n = look_build(NULL, rows, MENU_MAX_ROWS, &h);
	menu_rows_print("theme", rows, n, LOOK_SOUND, g_lk_choice);
	n = hours_build(&hv, rows, MENU_MAX_ROWS, &h);
	menu_rows_print("hours", rows, n, HOURS_SOUND, g_hours_choice);
}

/* MENU > Power Button as saved: "hold" is Hold, anything else Press, the
 * default (src/powerhold.h). Once, as the launcher starts. */
void power_button_load(void)
{
	char v[8];

	db_get_str(db_dev(), POWERHOLD_KEY, v, sizeof v, "press");
	powerhold_set_mode(!strcmp(v, "hold"));
}
