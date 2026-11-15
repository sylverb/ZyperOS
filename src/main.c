/* SPDX-License-Identifier: GPL-3.0-only
 *
 * ZyperOS - a custom firmware for the TrimUI Brick that plays eleven
 * 8-, 16- and 32-bit consoles.
 *
 * The whole design metric is speed. The launcher starts behind the boot
 * animation rather than after it, hands games to an emulator that is already
 * running rather than starting one, and never tears its own display down - so
 * coming back from a game is a frame, not a second and a half. Around that sit
 * RetroAchievements, box art the device fetches itself, a file server over
 * Wi-Fi, Bluetooth audio and play time - each one row in one menu, and none of
 * it in the way of starting a game.
 */
#include "atomic.h"
#include "autozone.h"
#include "about.h"
#include "battery_eta.h"
#include "battery_poll.h"
#include "bgm.h"
#include "bgm_menu.h"
#include "blur.h"
#include "cardgen.h"
#include "cheevos.h"
#include "clock.h"
#include "config.h"
#include "ctl.h"
#include "ctl_channel.h"
#include "update_screen.h"
#include "search_screen.h"
#include "clock_screen.h"
#include "bgm_screen.h"
#include "gamedel_screen.h"
#include "gamern_screen.h"
#include "storage_screen.h"
#include "media_screen.h"
#include "menu_panel.h"
#include "nowplaying_screen.h"
#include "bt.h"
#include "btvol.h"
#include "bt_menu.h"
#include "db.h"
#include "stats.h"
#include "sort.h"
#include "coverflow.h"
#include "library.h"
#include "platform.h"
#include "texload.h"
#include "tween.h"
#include "artshrink.h"
#include "artbadge.h"
#include "look.h"
#include "artscrape.h"
#include "taskhold.h"
#include "favorites.h"
#include "hare.h"
#include "xfer.h"
#include "home.h"
#include "games.h"
#include "hareart.h"
#include "hotspot.h"
#include "emptyhome.h"
#include "hotpanel.h"
#include "captive.h"
#include "p2.h"
#include "p2panel.h"
#include "p2share.h"
#include "spawn.h"
#include "webtheme.h"
#include "devmode.h"
#include "sshlink.h"
#include "mdns.h"
#include "linksteps.h"
#include "keyhints_draw.h"
#include "usblink.h"
#include "usblog.h"
#include "phonelink.h"
#include "chargedark.h"
#include "pace.h"
#include "idle.h"
#include "notice.h"
#include "randgame.h"
#include "saver.h"
#include "osdbar.h"
#include "rafetch.h"
#include "noticeq.h"
#include "rabadge.h"
#include "rahash.h"
#include "theme.h"
#include "boottheme.h"
#include "net.h"
#include "keyboard.h"
#include "logpack.h"
#include "wifi.h"
#include "audioout.h"
#include "uisound.h"
#include "menu.h"
#include "menumotion.h"
#include "message.h"
#include "menusnap.h"
#include "musec.h"
#include "mp4lock.h"
#include "topbar.h"
#include "mp4video.h"
#include "museart.h"
#include "muselib.h"
#include "sgdb.h"
#include "snaps.h"
#include "snapshelf.h"
#include "snapview.h"
#include "ss.h"
#include "ssrun.h"
#include "search.h"
#include "status.h"
#include "sys_menu.h"
#include "sysupdate.h"
#include "storage.h"
#include "storage_page.h"
#include "strength.h"
#include "artsrc.h"
#include "mediafetch.h"
#include "mediapage.h"
#include "mediatex.h"
#include "controls.h"
#include "ctltab_screen.h"
#include "achtab_screen.h"
#include "rahashjob.h"
#include "remapstore.h"
#include "tour.h"
#include "tourdraw.h"
#include "tourdevice.h"
#include "game_menu.h"
#include "portmenu.h"
#include "gamedel.h"
#include "gamerows.h"
#include "portinfo.h"
#include "dsv.h"
#include "dslang.h"
#include "corestate.h"
#include "dsresume.h"
#include "ui.h"
#include "ds/mark.h"    /* badges, switch marks, the progress bar */
#include "ds/page.h"    /* a page: its frame, heading, line over the keys, foot */
#include "ds/tiles.h"   /* a question in tiles, or a list */
#include "wifi_menu.h"
#include "zyperrig.h"
#include "songpop.h"
#include "heat.h"
#include "lowbatt.h"
#include "charging.h"
#include "app.h"
#include "aout_route.h"
#include "bt_screen.h"
#include "panel.h"
#include "place.h"
#include "pminstall.h"
#include "portload.h"
#include "powerscreens.h"
#include "raqueue.h"
#include "shelfbuild.h"
#include "shelfdir.h"
#include "esmedia.h"
#include "gamern.h"
#include "stats_screen.h"
#include "ingame_screen.h"
#include "sysmenu_screen.h"
#include "launch_screen.h"
#include "saver_screen.h"
#include "muse_screen.h"
#include "player2_screen.h"
#include "tour_screen.h"
#include "devmode_screen.h"
#include "signin_screen.h"
#include "about_screen.h"
#include "usbdrive_screen.h"
#include "xfer_screen.h"
#include "scraper_screen.h"
#include "gameinfo_screen.h"
#include "shelf_keys.h"
#include "shelf_draw.h"
#include "zyperdraw.h"
#include "zypermark.h"

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

/* The row is allowed to glide only across cards this policy keeps decoded. If
 * TEX_KEEP_NEAR ever drops below it, a long move would animate over cold cards
 * and stall exactly as it did before CF_WARM_CARDS existed. */
_Static_assert(TEX_KEEP_NEAR >= CF_WARM_CARDS,
               "the row glides across cards the texture policy must keep");

volatile sig_atomic_t want_quit;
static void on_sigterm(int sig) { (void)sig; want_quit = 1; plat_terminate(); }

/* This process is the in-game menu over a port (src/portmenu.h, --port-menu),
 * started for it by portkeys while the port is stopped: the menu, its
 * Controls page and nothing else. */
bool g_port_menu;

/* Boot time is the one number a launcher cannot be careless about, so the
 * phases are timed and logged rather than guessed at. On the project this
 * grew out of, the phase everyone assumed was expensive turned out to be 8%
 * of startup -- which is only knowable by measuring. */
unsigned t_boot0;
static void t_mark(const char *what)
{
	fprintf(stderr, "boot: %-14s %5u ms\n", what, plat_now_ms() - t_boot0);
}

/* One number for the way back, because this path keeps being asked how fast it
 * is and a number is how the answer stays true. The five phase marks that took
 * it from 213ms to here are in the history if it ever needs breaking down
 * again; the budget they found was 5ms to black, 4ms of bookkeeping, and the
 * rest decoding the focused card's autosave preview off the SD card. */
unsigned t_back0;

/* Counts the frames begun. What starts a whole frame steps it - draw_shelf,
 * paused_backdrop, present_clear and Now Playing's np_draw, one of which
 * starts every frame a menu's panel can be drawn over or left for. The panel
 * drawn last remembers the count it was drawn at (g_snap), and is still the
 * one on screen only while the count has not moved since: a keyboard, a slot
 * strip or Now Playing drawn after it has moved it. */
/* A screen's one-button hint, B and its word (keyhints.h), for a row */
const char *b_legend(str_id word)
{
	return keyhints_legend((key_hint[]){ { "B", T(word) } }, 1);
}

/* The tutorial's START hint, as every hint (tour_hint_draw_set) */
static void tour_hint(SDL_Renderer *r, const char *key, const char *label, int x, int cy)
{
	key_hint h = { key, label };

	keyhints_draw(r, &h, 1, x, x + keyhints_width(key, label), cy, KEYHINTS_CENTER, false, 255);
}

unsigned g_frame_no;

/* One present of nothing but `c`, over the whole screen. */
void present_clear(app *a, ds_rgb c)
{
	g_frame_no++;
	SDL_SetRenderDrawBlendMode(a->r, SDL_BLENDMODE_NONE);
	SDL_SetRenderDrawColor(a->r, c.r, c.g, c.b, 255);
	SDL_RenderClear(a->r);
	plat_present();
	SDL_SetRenderDrawBlendMode(a->r, SDL_BLENDMODE_BLEND);
}

/* Get off the game's last frame before doing anything slow.
 *
 * Everything between EXIT and the first frame of the shelf blocks and none of
 * it presents, so the panel holds whatever the launcher drew last - which
 * after a quit is the in-game menu, over a frame of the game that has ended.
 * Measured on the device: 123ms of it, 52 in the LED writes that ran here
 * then (since removed) and 69 rebuilding the focused card off the SD card.
 *
 * One present of the theme's ground, because the shelf that follows is drawn
 * on it: in Beard a black frame between a game and the light shelf is a flash
 * of a color the theme has nowhere else. This is not the buffer-establishing
 * that was tried and reverted: that guarded a stale back buffer, which a swap
 * cannot show. This gets a stale FRONT frame off the glass, which is
 * measurable and was measured. */
void present_ground(app *a)
{
	present_clear(a, theme()->ground);
}

/* Black, the one color here that is not the theme's, because it is Diatom's:
 * its pages start opaque black and it draws only the game's picture into
 * them, so the border around a game that does not fill the panel is this
 * black in both themes - the game is content, not part of the theme (DESIGN.md
 * 3.4). What resuming a game presents has to be the same black, or the border
 * would change color as Diatom cycles its pages (see where this is called). */
const ds_rgb DIATOM_PAGE = { 0, 0, 0 };

/* ---------- per-system display mode --------------------------------------- */

/* MENU's shader, every system's that has not chosen its own (SHADERS) */
int g_shader_all;

/* A shader by Diatom's name for it (SHADERS), or -1 */
static int shader_index(const char *name)
{
	int k;

	for (k = 0; k < SHADER_COUNT; k++)
		if (!strcmp(SHADERS[k].name, name)) return k;
	return -1;
}

/* Whether this device offers shader k: the old screens only where Diatom
 * draws them with a GPU (plat_gpu_shaders) */
static bool shader_offered(int k)
{
	return k >= 0 && k < SHADER_COUNT && (!SHADERS[k].gpu || plat_gpu_shaders());
}

/* The shader a step `d` comes to from k, round the ones this device offers:
 * each Shader row steps with it */
int shader_step(int k, int d)
{
	int i;

	for (i = 0; i < SHADER_COUNT; i++) {
		k = (k + d + SHADER_COUNT) % SHADER_COUNT;
		if (shader_offered(k)) break;
	}
	return k;
}

/* Keyed on the system's tag rather than its folder or its place in the list,
 * for the same reason saves are: renaming a ROM folder or reordering
 * systems.cfg then cannot quietly hand a system somebody else's setting.
 * The shaders too, each system's: its own shader.<TAG>, else MENU's
 * `shader`, else Off; and their strength the same way (src/strength.h). */
void display_load(app *a)
{
	char key[CFG_STR + 16], name[CFG_STR];
	int i, k;

	g_shader_all = 0;
	if (db_get_str(db_dev(), "shader", name, sizeof name, NULL) && (k = shader_index(name)) >= 0)
		g_shader_all = k;
	for (i = 0; i < a->sys.count; i++) {
		snprintf(key, sizeof key, "shader.%s", a->sys.systems[i].tag);
		a->view[i].shader = g_shader_all;
		if (db_get_str(db_dev(), key, name, sizeof name, NULL) && (k = shader_index(name)) >= 0)
			a->view[i].shader = k;
		a->view[i].strength = strength_read(db_dev(), a->sys.systems[i].tag);
		snprintf(key, sizeof key, "display.%s", a->sys.systems[i].tag);
		if (!db_get_str(db_dev(), key, name, sizeof name, NULL)) continue;
		for (k = 0; k < DMODE_COUNT; k++)
			if (!strcmp(DMODES[k].name, name)) { a->view[i].dmode = k; break; }
	}
}

/* One row per visible system, and nothing else touched.
 *
 * The file this replaces was rewritten whole, so it had to read itself back
 * first and carry forward the modes of systems that are not on the shelf right
 * now. That was not tidiness: empty systems are hidden, so "every system" meant
 * "every system with games in it today", and a plain rewrite would quietly
 * erase the mode of any system whose ROMs happened to be off the card. Take
 * the ROMs out, change one unrelated setting, put them back, and the mode you
 * chose is gone with no sign it existed.
 *
 * A row write cannot touch another row, so that whole pass is gone rather than
 * ported. It is the clearest thing the database bought. */
void display_save(app *a)
{
	char key[CFG_STR + 16];
	int i;

	for (i = 0; i < a->sys.count; i++) {
		snprintf(key, sizeof key, "display.%s", a->sys.systems[i].tag);
		db_set_str(db_dev(), key, DMODES[a->view[i].dmode].name);
	}
}

/* ---------- the shader ------------------------------------------------------ */

/* System `sys`'s own shader, kept the moment it changes, as its display mode
 * is: the system menu's row and the in-game menu's. */
void shader_save(app *a, int sys)
{
	char key[CFG_STR + 16];

	snprintf(key, sizeof key, "shader.%s", a->sys.systems[sys].tag);
	db_set_str(db_dev(), key, SHADERS[a->view[sys].shader].name);
}

/* Every key a system's own shader is kept under, gathered before any goes:
 * a walk is not the place to delete what it walks */
typedef struct { char key[CFG_MAX_SYSTEMS * 2][CFG_STR + 16]; int n; } shader_keys;

static bool shader_key_note(const char *key, const char *value, void *ctx)
{
	shader_keys *k = ctx;

	(void)value;
	if (k->n >= (int)(sizeof k->key / sizeof k->key[0])) return false;
	snprintf(k->key[k->n++], sizeof k->key[0], "%s", key);
	return true;
}

/* MENU's row: shader `k` for every system, those whose games are off the card
 * now too, so each system's own choice goes and the next game of any of them
 * plays with it. */
void shader_set_all(app *a, int k)
{
	static shader_keys keys;
	int i;

	g_shader_all = k;
	db_set_str(db_dev(), "shader", SHADERS[k].name);
	keys.n = 0;
	db_each_prefix(db_dev(), "shader.", shader_key_note, &keys);
	for (i = 0; i < keys.n; i++) db_del(db_dev(), keys.key[i]);
	for (i = 0; i < a->sys.count; i++) a->view[i].shader = k;
}

/* What MENU's row says: the shader every system has, or "per system" when one
 * of them chose another in its own menu or in a game */
const char *shader_all_label(const app *a)
{
	int i;

	for (i = 0; i < a->sys.count; i++)
		if (a->view[i].shader != g_shader_all) return T(S_GAMEMENU_SHADER_MIXED);
	return shader_label(g_shader_all);
}

/* The Strength rows (src/strength.h). MENU has one while any system draws an
 * old screen; a system's menu and the in-game menu while that system does. */
bool strength_all_shown(const app *a)
{
	int i;

	for (i = 0; i < a->sys.count; i++)
		if (shader_has_strength(a->view[i].shader)) return true;
	return false;
}

/* MENU's row's value: every system's strength, or "per system" when one
 * chose another */
const char *strength_all_label(const app *a)
{
	static char buf[16];
	int i, all = strength_read_all(db_dev());

	for (i = 0; i < a->sys.count; i++)
		if (strength_clamp(a->view[i].strength) != all) return T(S_GAMEMENU_SHADER_MIXED);
	strength_label(all, buf, sizeof buf);
	return buf;
}

/* System `sys`'s row's value, NULL when it has no row */
const char *strength_row_label(const app *a, int sys)
{
	static char buf[16];

	if (!shader_has_strength(a->view[sys].shader)) return NULL;
	strength_label(a->view[sys].strength, buf, sizeof buf);
	return buf;
}

/* ---------- per-system core ----------------------------------------------- */

/* A shelf whose core field lists more than one core, the first the default -
 * the Dreamcast: flycast2021, a libretro core run in Diatom with ZyperOS's own
 * in-game menu, and the standalone Flycast 2.7, a program run through a
 * script. The shelf's Core row chooses (core_step, from the system menu), and
 * the choice is kept per system under core.<TAG>, beside display.<TAG>.
 * Everything that runs a game reads s->core, so choosing is writing that
 * field and nothing else follows it. A shelf with one core keeps the row, as
 * a fact rather than a choice: it names the core.
 *
 * A core that is not on the card is passed over when there is no choice on
 * record: a card without flycast2021 plays its Dreamcast games on Flycast
 * 2.7, if that is there, without anyone visiting a menu, and the row offers
 * both. A choice on record is kept even if its core has gone, so taking
 * Flycast 2.7 off for a while does not lose it. A choice on record of a core
 * the shelf no longer lists is passed over the same way: the Nintendo DS's
 * core.NDS, which named DraStic's script on a card that chose it before
 * 2026-11-08, is never read, since the DS has one core now. */
bool core_present(const char *core)
{
	char path[CFG_STR * 2];

	if (!strncmp(core, "run:", 4)) {
		snprintf(path, sizeof path, "%s/%s", P_CARD, core + 4);
		return access(path, R_OK) == 0;
	}
	if (!strcmp(core, "port")) return true;
	snprintf(path, sizeof path, "%s/cores/%s_libretro.so", P_ROOT, core);
	return access(path, R_OK) == 0;
}

/* What the Core row says: a libretro core by its short name, as every other
 * shelf's row always has, and a script by its own name - "Flycast" for
 * run:Emulators/Flycast/Flycast.sh - since the path is the card's business. */
const char *core_label(const char *core, char *buf, size_t n)
{
	const char *b, *dot;

	if (strncmp(core, "run:", 4) != 0) return core;
	b = strrchr(core, '/');
	b = b ? b + 1 : core + 4;
	dot = strrchr(b, '.');
	snprintf(buf, n, "%.*s", dot ? (int)(dot - b) : (int)strlen(b), b);
	return buf;
}

void core_load(app *a)
{
	char key[CFG_STR + 16], name[CFG_STR], alt[CFG_STR];
	int i, k, n;

	for (i = 0; i < a->sys.count; i++) {
		system_cfg *s = &a->sys.systems[i];
		bool set = false;

		if ((n = cfg_core_count(s)) < 2) continue;
		snprintf(key, sizeof key, "core.%s", s->tag);
		if (db_get_str(db_dev(), key, name, sizeof name, NULL))
			for (k = 0; k < n && !set; k++)
				if (cfg_core_at(s, k, alt, sizeof alt) && !strcmp(alt, name)) {
					snprintf(s->core, sizeof s->core, "%s", alt);
					set = true;
				}
		for (k = 0; k < n && !set; k++)
			if (cfg_core_at(s, k, alt, sizeof alt) && core_present(alt)) {
				snprintf(s->core, sizeof s->core, "%s", alt);
				set = true;
			}
		fprintf(stderr, "core: %s runs on %s%s\n", s->name, s->core,
		        set ? "" : " (none of its cores is on the card)");
	}
}

/* The next core in the shelf's list, or the previous one. */
void core_step(app *a, int sys, int d)
{
	system_cfg *s = &a->sys.systems[sys];
	char key[CFG_STR + 16], alt[CFG_STR];
	int n = cfg_core_count(s), k, cur = 0;

	if (n < 2) return;
	for (k = 0; k < n; k++)
		if (cfg_core_at(s, k, alt, sizeof alt) && !strcmp(alt, s->core)) cur = k;
	cur = (cur + d + n) % n;
	if (!cfg_core_at(s, cur, alt, sizeof alt)) return;
	snprintf(s->core, sizeof s->core, "%s", alt);
	snprintf(key, sizeof key, "core.%s", s->tag);
	db_set_str(db_dev(), key, s->core);
	fprintf(stderr, "core: %s now runs on %s\n", s->name, s->core);
}

/* ---------- the Nintendo DS's game language ------------------------------- */

/* Game Language, the row under Core on the Nintendo DS shelf's menu
 * (src/dslang.h): Auto, the language of MENU > Language where the DS has it
 * and English where not, or one of the choices outright. Saved as
 * ds_language the moment it changes; the next game started runs in it
 * (src/platform.c sends it as the core's dsperate_language), and so does the
 * next Reset of the one running (mk/patches/dsperate-lr-language.patch). */
dsl_choice dslang_load(void)
{
	char v[16];

	db_get_str(db_dev(), DSLANG_SETTING, v, sizeof v, "auto");
	return dslang_parse(v);
}

bool dslang_row_shown(const app *a, int sys)
{
	const system_cfg *s = &a->sys.systems[sys];

	return !strcmp(s->tag, DSLANG_TAG) && !strcmp(s->core, "dsperate");
}

/* What the row says for choice `c`: a language by its own name, and Auto by
 * the language it comes to - "Auto (English)" for a handheld in Português
 * (Brasil) - since a row that reads only "Auto" makes the player guess. */
const char *dslang_label(dsl_choice c, char *buf, size_t n)
{
	if (c == DSL_AUTO) {
		dsl_choice to = dslang_parse(dslang_value(i18n_lang()));

		snprintf(buf, n, T(S_SYSMENU_DSLANG_AUTO), lang_name(dslang_label_lang(to)));
	} else {
		snprintf(buf, n, "%s", lang_name(dslang_label_lang(c)));
	}
	return buf;
}

/* A DS game's resume, by the language it was left in (src/dsresume.h): for
 * the game about to start, `st` its auto state, the language it will run in
 * into `lang` ("" when that cannot be known, or the game is not a DS game on
 * DSperate), and true when its auto state was made in another one, so it
 * starts fresh instead. The language is the one the core gets: the last
 * dsperate_language the launcher sends, which is the row's unless the
 * library database has one of its own (src/platform.c). */
bool ds_resume_fresh(const system_cfg *s, const char *romfile, const char *st,
                     char *lang, size_t n)
{
	char fw[CFG_STR * 2], opt[32] = "", was[16];
	const char *eff, *rec;
	int i, k = plat_coreopt_count(s->tag);

	lang[0] = '\0';
	if (strcmp(s->tag, DSLANG_TAG) || strcmp(s->core, "dsperate")) return false;
	for (i = 0; i < k; i++) {
		const char *kv = plat_coreopt(s->tag, i);

		if (!strncmp(kv, DSLANG_KEY "=", sizeof DSLANG_KEY))
			snprintf(opt, sizeof opt, "%s", kv + sizeof DSLANG_KEY);
	}
	snprintf(fw, sizeof fw, "%s/Bios/firmware.bin", P_CARD);
	if (!(eff = dsr_effective(opt, fw))) return false;
	snprintf(lang, n, "%s", eff);
	if (access(st, F_OK) != 0) return false;
	rec = dsr_recorded(st, fw, was, sizeof was);
	if (!dsr_start_fresh(rec, eff)) return false;
	fprintf(stderr, "dslang: %s: its auto state was made in %s, the game runs in %s now: "
	        "starting it fresh\n", romfile, rec, eff);
	return true;
}

/* A game's resume, by the core that left it (src/corestate.h): true when its
 * auto state `st` was written by another core of the shelf than the one the
 * game starts on now, so it starts fresh instead of having Diatom refuse the
 * state without a word. The N64's: a game left on Mupen64Plus-Next, before
 * ParallelN64 was the default, or on whichever the Core row chose then. */
bool core_resume_fresh(const system_cfg *s, const char *romfile, const char *st)
{
	char who[96];

	if (cfg_core_count(s) < 2 || !cst_writer(st, who, sizeof who)) return false;
	if (!cst_other_core(who, s->core, s->cores)) return false;
	fprintf(stderr, "corestate: %s: its auto state was left by %s, the game runs on %s now: "
	        "starting it fresh\n", romfile, who, s->core);
	return true;
}

/* As the game ends: when its auto state was written in this run (it is not
 * the file `before` saw), the language it was written in goes beside it. The
 * state's own time and size, not the way the game ended: Diatom writes it at
 * a quit, at POWER and at a shutdown's TERM alike, and not after a crash. */
void ds_resume_record(const char *st, const char *lang, const struct stat *before,
                      bool had)
{
	struct stat now;

	if (!lang[0] || stat(st, &now) != 0) return;
	if (had && now.st_mtime == before->st_mtime && now.st_size == before->st_size &&
	    now.st_ino == before->st_ino)
		return;
	if (!dsr_record(st, lang))
		fprintf(stderr, "dslang: could not write the language beside %s\n", st);
}

/* ---------- two screens ---------------------------------------------------- */

/* A Nintendo DS game on a libretro core is one frame with both screens in it,
 * top over bottom; Diatom lays them out when told the frame holds two
 * (RUN screens=2, Diatom's src/screens.h). Keyed on the tag, not on the core:
 * any DS core hands over the same stacked frame. The layout is kept per
 * system, screens.<TAG>, single (the top screen alone) until chosen, and sent
 * after every RUN, as the display mode is. The swap and the stylus are the
 * game's, from the top screen and off: L2 and R2 change them in the game
 * (Diatom's screens.h), and the in-game menu reads them back from Diatom. */
/* The game running now is one: set at launch, read by the in-game menu. */
bool g_game_dual;

bool two_screens(const system_cfg *s)
{
	return !strcmp(s->tag, "NDS") && strncmp(s->core, "run:", 4) != 0 &&
	       strcmp(s->core, "port") != 0;
}

void screens_get(const char *tag, int *layout, bool *swap)
{
	char key[32], v[32];

	snprintf(key, sizeof key, "screens.%s", tag);
	*layout = db_get_str(db_dev(), key, v, sizeof v, NULL) ? gm_layout_index(v)
	                                                      : GM_LAYOUT_DEFAULT;
	*swap = false;
}

void screens_put(const char *tag, int layout, bool swap)
{
	char key[32];

	snprintf(key, sizeof key, "screens.%s", tag);
	db_set_str(db_dev(), key, gm_layout_names[layout]);
	plat_resident_line("SETSCREENS\tlayout=%s\tswap=%d", gm_layout_names[layout], swap ? 1 : 0);
}

/* ---------- the quick slot ------------------------------------------------- *
 *
 * SELECT+R1 saves a state to it and SELECT+L1 loads it (Diatom's chord.h),
 * and the in-game menu's Quick Slot row picks it: one of the six, for each
 * system, slot 1 until one is picked. Kept in the device's database beside
 * screens.<TAG>, under quickslot.<TAG>, so it stays on the card. */
/* The last system asked for and its slot: the in-game menu builds its rows
 * every frame, and the database is read once, not sixty times a second.
 * Written only through quick_slot_put, which keeps it. */
static struct { char tag[CFG_STR]; int slot; } quick_seen;

int quick_slot_get(const char *tag)
{
	char key[48], v[8];

	if (quick_seen.slot && !strcmp(quick_seen.tag, tag)) return quick_seen.slot;
	gm_quick_key(tag, key, sizeof key);
	snprintf(quick_seen.tag, sizeof quick_seen.tag, "%s", tag);
	quick_seen.slot = gm_quick_slot(db_get_str(db_dev(), key, v, sizeof v, NULL) ? v : NULL);
	return quick_seen.slot;
}

void quick_slot_put(const char *tag, int slot)
{
	char key[48], v[8];

	if (slot < 1 || slot > GM_SLOTS) slot = GM_QUICK_DEFAULT;
	gm_quick_key(tag, key, sizeof key);
	snprintf(v, sizeof v, "%d", slot);
	db_set_str(db_dev(), key, v);
	snprintf(quick_seen.tag, sizeof quick_seen.tag, "%s", tag);
	quick_seen.slot = slot;
}

/* The game being played, for the hotkeys: they come while this process waits
 * on the game and has no shelf to look at, so the launch leaves what they
 * need here (quick_bind). A slot's files are <stem>.<n>.state and .bmp, as
 * the menu's Save writes them (slot_state_path). `asked` is the slot of a
 * SAVE or LOAD sent and not yet answered, 0 for none, and `path` its state. */
static struct {
	char tag[CFG_STR];
	char name[CFG_STR];
	char file[LIB_PATH];  /* the game's, for what is kept per game (rms_stick_put) */
	system_cfg sys;       /* its system, whose stick default the key is kept against */
	char stem[LIB_PATH * 2];
	bool states;
	int  asked;
	bool load;
	char path[LIB_PATH * 2 + 16];
} g_quick;

/* ---------- the Nintendo 64's Pak ------------------------------------------ *
 *
 * What an N64 game's controller holds: the Rumble Pak, ZyperOS's default
 * (src/db.c: the Pixel 2 has a motor), or the Controller Pak, the memory
 * card some games save to - Mario Kart 64's ghosts, and games that save
 * nowhere else. Chosen per game in the in-game menu's Pak row, since it is
 * the game that needs one or the other, and kept in the library's database
 * as pak.<TAG>.<file>, beside the system's core options; a game never chosen
 * for has the system's (coreopt.N64.parallel-n64-pak1, or
 * coreopt.N64.mupen64plus-pak1 on the shelf's other core). Both cores read
 * their option again whenever one changes and swap the pak in the controller
 * there and then, so the row works while the game runs, and the Controller
 * Pak's contents live in the game's own save RAM, its .srm, whichever is in.
 * The choice is the game's, not the core's: it holds on either. */
bool pak_system(const system_cfg *s)
{
	return s && gm_pak_option(s->core) != NULL;
}

/* The system's own: its core's option, or the Rumble Pak */
static const char *pak_default(const system_cfg *s)
{
	const char *opt = gm_pak_option(s->core);
	size_t len = opt ? strlen(opt) : 0;
	int i, n = plat_coreopt_count(s->tag);

	for (i = 0; opt && i < n; i++) {
		const char *kv = plat_coreopt(s->tag, i);

		if (!strncmp(kv, opt, len) && kv[len] == '=')
			return gm_pak_value(kv + len + 1);
	}
	return gm_pak_value(NULL);
}

const char *pak_get(const system_cfg *s, const char *file)
{
	char key[CFG_STR + LIB_PATH + 8], v[16];

	gm_pak_key(s->tag, file, key, sizeof key);
	return db_get_str(db_lib(), key, v, sizeof v, NULL) ? gm_pak_value(v) : pak_default(s);
}

void pak_put(const char *tag, const char *file, const char *value)
{
	char key[CFG_STR + LIB_PATH + 8];

	gm_pak_key(tag, file, key, sizeof key);
	db_set_str(db_lib(), key, gm_pak_value(value));
}

/* ---------- D-pad as Stick: kept per game (src/remapstore.c) ---------------
 *
 * The handheld has no stick. A system with a stick|tag|mode line
 * (config/systems.cfg) can turn the d-pad into the core's left stick, game by
 * game: the N64 starts with it on, the Dreamcast and the PlayStation off.
 * The PSP's (stick|PSP|toggle) starts with the d-pad as the d-pad, and R2
 * turns it into the left stick and back (Diatom's RUN stick=toggle), as the
 * Pixel 2's own firmware and Deadstation Portable do it. The side a game was
 * left on is kept for that game, so a game that moves with the stick starts
 * with it the next time. */

/* ---------- per-system sort order ----------------------------------------- */

/* Keyed on the tag for the same reason display.<TAG> is, and stored beside it.
 * Loading does NOT re-sort: it runs before the scan on a rescan and after it
 * at startup, and the shelf is put in order by sort_all once both are done.
 *
 * Muse's shelf has orders of its own, by artist and by album, stored by their
 * own names under the same key - sort.MUSE. */
void sort_load(app *a)
{
	char key[CFG_STR + 16], name[CFG_STR];
	int i;

	for (i = 0; i < a->sys.count; i++) {
		snprintf(key, sizeof key, "sort.%s", a->sys.systems[i].tag);
		if (!db_get_str(db_dev(), key, name, sizeof name, NULL)) continue;
		a->view[i].sort = is_muse(&a->sys.systems[i]) ? (int)ml_order_index(name)
		                                              : sort_index(name);
	}
}

/* MENU > View, per shelf like Sort By: "list" or none (the covers), kept
 * in the device's database by the system's tag (DESIGN.md 11.39) */
bool view_listed(const char *tag)
{
	char key[CFG_STR + 16], v[16];

	snprintf(key, sizeof key, "view.%s", tag);
	return db_get_str(db_dev(), key, v, sizeof v, NULL) && !strcmp(v, "list");
}

void view_load(app *a)
{
	for (int i = 0; i < a->sys.count; i++) {
		a->view[i].listed = view_listed(a->sys.systems[i].tag);
		a->view[i].lm.set = false;
	}
}

static void view_save(app *a, int sys)
{
	char key[CFG_STR + 16];

	snprintf(key, sizeof key, "view.%s", a->sys.systems[sys].tag);
	db_set_str(db_dev(), key, a->view[sys].listed ? "list" : "covers");
}

/* View turned over on shelf `sys`: covers to the list or back, saved, the
 * shelf on the same entry. MENU's View row and START on the shelf both. */
void view_turn(app *a, int sys)
{
	sysview *v = &a->view[sys];

	v->listed = !v->listed;
	v->lm.set = false;
	view_save(a, sys);
	cf_reset(&v->cf, v->cursor);
	games_reset(&v->row, v->cursor, v->list.count, plat_now_ms());
}

void sort_save(app *a)
{
	char key[CFG_STR + 16];
	int i;

	for (i = 0; i < a->sys.count; i++) {
		snprintf(key, sizeof key, "sort.%s", a->sys.systems[i].tag);
		db_set_str(db_dev(), key,
		           is_muse(&a->sys.systems[i]) ? ml_order_name((ml_order)a->view[i].sort)
		                                       : SORTS[a->view[i].sort].name);
	}
}

/* Put every shelf in its own order. One pass over the systems, and the
 * session rows are folded once per system that needs them rather than once
 * per game - which is why this is a loop here and not a call inside scan_all.
 *
 * The favorites shelf is built elsewhere and keeps its own order: it is a
 * shelf of games from many systems and has no single tag to hang a setting
 * off. */
/* One shelf in its own order.
 *
 * Favorites through sort_apply_owned: its games are several systems', and each
 * one's owner has to come with it and its play time has to be read under its
 * own system's tag. This ran the plain sort on Favorites as well, which only
 * held because Favorites was always sorted by name, the order it is built in -
 * any other order would have launched some favorites under another system's
 * core.
 *
 * Muse by its own orders, in muse_order_view: its cards are albums, and which
 * album each card is has to move with it. */
void sort_shelf(app *a, int sys)
{
	sysview *v = &a->view[sys];

	if (is_muse(&a->sys.systems[sys])) { muse_order_view(v); return; }
	/* The added times this order reads, each game's from its own system's
	 * folder (Favorites' from its owner's), where the scan left them unknown */
	if (sort_needs_added(v->sort))
		for (int i = v->nfold; i < v->list.count; i++)
			if (v->list.items[i].added == LIB_ADDED_UNKNOWN)
				v->list.items[i].added = lib_added(P_ROMS,
					a->sys.systems[v->owner ? v->owner[i] : sys].folder,
					v->list.items[i].file, v->list.items[i].dir);
	if (v->owner) {
		const char *tags[CFG_MAX_SYSTEMS];
		int i;

		for (i = 0; i < a->sys.count && i < CFG_MAX_SYSTEMS; i++)
			tags[i] = a->sys.systems[i].tag;
		sort_apply_owned(v->list.items, v->owner, v->list.count, v->sort, tags);
		return;
	}
	/* A shelf's folders stay first, A to Z (src/shelfdir.h): the order is
	 * its games' */
	sort_apply(v->list.items + v->nfold, v->list.count - v->nfold, v->sort,
	           a->sys.systems[sys].tag);
}

void sort_all(app *a)
{
	int i;

	for (i = 0; i < a->sys.count; i++) sort_shelf(a, i);
}

/* ---------- textures ----------------------------------------------------- */


/* How far down the surface its opaque pixels reach, as a fraction of height.
 *
 * Art here is squared and centered, so a console photographed low in its frame
 * carries a band of transparency underneath - 102 of 384 rows for the Genesis,
 * against 10 for a Game Boy. Reflecting from the card's edge mirrors that
 * emptiness too, which is why the same reflection setting produced a flush
 * reflection on one card and a gap of about 196px on another.
 *
 * Scanned bottom-up and stopping at the first row with anything in it, so the
 * cost is the padding rather than the image. Alpha over 8 rather than over 0:
 * a PNG's fully transparent region is not always exactly zero. */
static float content_bottom(SDL_Surface *s)
{
	int y, x;

	if (!s || s->h <= 0 || s->format->BytesPerPixel != 4) return 1.0f;
	for (y = s->h - 1; y >= 0; y--) {
		const Uint32 *row = (const Uint32 *)((Uint8 *)s->pixels + y * s->pitch);

		for (x = 0; x < s->w; x++)
			if ((row[x] >> 24) > 8)
				return (float)(y + 1) / (float)s->h;
	}
	return 1.0f;
}

/* A picture off the card, as ARGB8888, the one form everything here reads
 * pixels in; NULL if it is not there or will not decode. */
static SDL_Surface *load_surface(const char *path)
{
	SDL_Surface *raw = IMG_Load(path), *conv;

	if (!raw) return NULL;
	conv = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ARGB8888, 0);
	SDL_FreeSurface(raw);
	return conv;
}

SDL_Texture *load_image(SDL_Renderer *r, const char *path, int *w, int *h,
                        float *cb)
{
	SDL_Surface *conv = load_surface(path);
	SDL_Texture *t;

	if (cb) *cb = 1.0f;
	if (!conv) return NULL;
	if (cb) *cb = content_bottom(conv);
	t = SDL_CreateTextureFromSurface(r, conv);
	SDL_FreeSurface(conv);
	if (t) SDL_QueryTexture(t, NULL, NULL, w, h);
	return t;
}


/* Muse: what the Music folder holds, read at boot and on every rescan.
 *
 * RECOGNIZED BY ITS TAG, not by what it lacks. Favorites is the shelf with no
 * core and no folder, and the system menu tells them apart that way; Muse has
 * neither either, but its music is not a ROM folder at all, so an empty folder
 * is the honest value and the tag is what says which of the two it is. */
ml_lib g_muse;
home_song g_home_song;
static int g_home_track = -1;
/* Where the scan read it from: the card, which holds Music and Audiobooks. */
char   g_muse_root[CFG_STR * 2];
/* Bumped by every build of the library, so a Muse screen can tell that MENU's
 * Rescan Folder replaced it while its menu was open - see muse_menu. */
unsigned g_muse_gen;

bool is_muse(const system_cfg *s)
{
	return s && !strcmp(s->tag, "MUSE");
}

/* What is known about each album's cover: one per album in g_muse, rebuilt
 * with it. ASKED carries when, because an answer can be lost with the daemon,
 * and an album is then asked about again rather than waited on for good.
 * The states, COV_*, and cover_state are in src/app.h. */
cover_state *g_cov;

/* ---- Muse: books ------------------------------------------------------------ */

/* Which kind Muse's shelf shows, music or books - and now videos, MP4's
 * third kind (ml_kind). Eric's, 2026-09-27: one Muse card and a Show row in
 * its menu, rather than a card for books beside it - two ways into one player
 * read as two players. Kept across restarts. Or all three on one shelf,
 * ML_ALL, which is where it starts (2026-11-09). */
int g_muse_kind = ML_ALL;

/* Whether each album is a book listened to the end: one per album in g_muse,
 * rebuilt with it, like g_cov. A finished book is marked and not forgotten,
 * and A on it starts it over. Eric's, 2026-09-27. */
bool *g_book_done;

/* The kind the shelf really shows: the one chosen, unless there is none of it
 * - then the first kind there is some of. All, on a card with one kind, is
 * that kind: the shelf and its menu say Albums, not All, when albums are all
 * there is. */
ml_kind muse_kind_shown(void)
{
	int k;

	if (g_muse_kind == ML_ALL && muse_kinds() > 1) return ML_ALL;
	if (g_muse_kind >= 0 && g_muse_kind < ML_KINDS && ml_count_kind(&g_muse, (ml_kind)g_muse_kind))
		return (ml_kind)g_muse_kind;
	for (k = 0; k < ML_KINDS; k++)
		if (ml_count_kind(&g_muse, (ml_kind)k)) return (ml_kind)k;
	return ML_MUSIC;
}

bool muse_books_shown(void) { return muse_kind_shown() == ML_BOOKS; }

/* The kind MP4's menu has the rows of (sys_menu_muse_rows): the shelf's,
 * but All on a card with no songs has the videos' - no Shuffle All, no Album
 * Art - as sys_menu_build makes of the same facts */
int muse_rows_kind(void)
{
	ml_kind k = muse_kind_shown();

	return k == ML_ALL && !ml_count_kind(&g_muse, ML_MUSIC) ? ML_VIDEOS : k;
}

/* How many kinds the card has: Show is in the menu only when it is two or
 * more. */
int muse_kinds(void)
{
	int k, n = 0;

	for (k = 0; k < ML_KINDS; k++)
		if (ml_count_kind(&g_muse, (ml_kind)k)) n++;
	return n;
}

/* Where book `al`'s place is kept: the settings, under its folder. */
void book_key(int al, char *out, size_t n)
{
	const char *p = g_muse.tracks[g_muse.albums[al].first].path;
	const char *slash = strrchr(p, '/');

	snprintf(out, n, "book.%.*s", slash ? (int)(slash - p) : 0, p);
}

/* A place is "<seconds>\t<file>", the file by its name rather than its number
 * so a file added to the folder later moves nothing, or "finished". */
static void book_write(int al, const char *track, double at)
{
	char key[LIB_PATH + 8], val[LIB_PATH + 32];
	const char *file = strrchr(track, '/');

	book_key(al, key, sizeof key);
	snprintf(val, sizeof val, "%.1f\t%s", at, file ? file + 1 : track);
	db_set_str(db_dev(), key, val);
	g_book_done[al] = false;
}

static void book_finish(int al)
{
	char key[LIB_PATH + 8];

	book_key(al, key, sizeof key);
	db_set_str(db_dev(), key, "finished");
	g_book_done[al] = true;
	fprintf(stderr, "muse: finished %s\n", g_muse.albums[al].name);
}

/* Where book `al` starts: its file, and `at` seconds into it. The beginning
 * for a book never started, or finished. */
int book_place(int al, double *at)
{
	const ml_album *b = &g_muse.albums[al];
	char key[LIB_PATH + 8], val[LIB_PATH + 32];
	const char *tab;
	int i;

	*at = 0;
	book_key(al, key, sizeof key);
	db_get_str(db_dev(), key, val, sizeof val, "");
	if (!(tab = strchr(val, '\t'))) return 0;
	for (i = 0; i < b->n; i++) {
		const char *p = g_muse.tracks[b->first + i].path;
		const char *f = strrchr(p, '/');

		if (!strcmp(f ? f + 1 : p, tab + 1)) { *at = atof(val); return i; }
	}
	return 0;
}

/* KEEPING A BOOK'S PLACE, wherever Muse is polled: on its screens, over the
 * shelf and the menus, and through a game, which is where a book is likely to
 * be heard.
 *
 * Written when the file changes, when it pauses or stops, and every thirty
 * seconds of listening in between, so a Brick that loses its battery goes back
 * half a minute at most. `now` writes whatever there is, for the power button
 * and for a new queue about to replace this one. */
static struct { int al, index; mu_state st; double at; } g_bk = { -1, -1, MU_OFF, 0 };

/* A VIDEO'S PLACE, kept the same way for each file: a film runs two hours and
 * is watched in pieces, as a book is listened to. Its own key per file, since
 * a folder of films or episodes is many places, and none kept for the first
 * and last minute - starting a film near its start is starting it, and a
 * place near the end would only replay the credits. my MP4, 2026-11-05. */
static struct { char track[LIB_PATH]; int index; mu_state st; double at, len; } g_vk = { "", -1, MU_OFF, 0, 0 };

static void video_key(const char *track, char *out, size_t n)
{
	snprintf(out, n, "video.%s", track);
}

static double video_place(const char *track)
{
	char key[LIB_PATH + 8], val[32];

	video_key(track, key, sizeof key);
	db_get_str(db_dev(), key, val, sizeof val, "");
	return atof(val) > 0 ? atof(val) : 0;
}

static void video_write(const char *track, double at, double len)
{
	char key[LIB_PATH + 8], val[32] = "";

	video_key(track, key, sizeof key);
	if (at >= 60 && (len <= 0 || at < len - 60)) snprintf(val, sizeof val, "%.1f", at);
	db_set_str(db_dev(), key, val);
}

static void video_keep(bool now, bool ran_out)
{
	const mu_now *mn = musec_now();
	const char *track = musec_track(mn->index);

	if (!musec_is_videos() || !track) {
		g_vk.track[0] = '\0';
		return;
	}
	/* The file before this one, as it was left: a file that played out, or
	 * that was left in its last minute, starts from the top next time. */
	if (g_vk.track[0] && strcmp(g_vk.track, track)) {
		video_write(g_vk.track, ran_out || mn->index != g_vk.index ? g_vk.len : g_vk.at, g_vk.len);
		g_vk.track[0] = '\0';
	}
	if (ran_out) { video_write(track, mn->len, mn->len); g_vk.track[0] = '\0'; return; }
	if (mn->state != MU_PLAYING && mn->state != MU_PAUSED) return;
	if (now || strcmp(g_vk.track, track) || mn->state != g_vk.st || fabs(mn->at - g_vk.at) >= 30) {
		video_write(track, mn->at, mn->len);
		snprintf(g_vk.track, sizeof g_vk.track, "%s", track);
		g_vk.index = mn->index;
		g_vk.st = mn->state;
	}
	g_vk.at = mn->at;
	g_vk.len = mn->len;
}

void book_keep(bool now)
{
	const mu_now *mn = musec_now();
	const char *track;
	bool ran_out = musec_take_ran_out();
	int al;

	video_keep(now, ran_out);
	if (!musec_is_book() || !g_book_done) { g_bk.al = -1; return; }
	track = musec_track(mn->index);
	al = ml_album_of(&g_muse, track);
	if (al < 0 || !g_muse.albums[al].book) { g_bk.al = -1; return; }
	if (ran_out) { book_finish(al); g_bk.al = -1; return; }
	if (mn->state != MU_PLAYING && mn->state != MU_PAUSED) return;
	if (now || al != g_bk.al || mn->index != g_bk.index || mn->state != g_bk.st ||
	    fabs(mn->at - g_bk.at) >= 30) {
		book_write(al, track, mn->at);
		g_bk.al = al;
		g_bk.index = mn->index;
		g_bk.st = mn->state;
		g_bk.at = mn->at;
	}
}

/* Muse, polled: what the daemon said, and a book's place kept. Everything that
 * polls Muse comes through here. */
void muse_poll(void)
{
	musec_poll();
	book_keep(false);
	/* Menu music is quiet while Muse is heard, or about to be */
	bgm_hold(BGM_HOLD_MUSE, musec_heard());
	const mu_now *mn = musec_now();
	if (home_song_update(&g_home_song,
	                     mn->state == MU_PLAYING || mn->state == MU_PAUSED,
	                     mn->state == MU_PAUSED, mn->title) || mn->index != g_home_track) redraw_now();
	g_home_track = mn->index;
}

/* A book's speed, kept beside its place: the last one chosen on it, 1x for a
 * book never changed. Its own key rather than a third field in the place, so
 * a place written before speeds existed still reads. */
static void book_speed_key(int al, char *out, size_t n)
{
	const char *p = g_muse.tracks[g_muse.albums[al].first].path;
	const char *slash = strrchr(p, '/');

	snprintf(out, n, "bookspeed.%.*s", slash ? (int)(slash - p) : 0, p);
}

double book_speed(int al)
{
	char key[LIB_PATH + 16], val[16];
	double x;

	book_speed_key(al, key, sizeof key);
	db_get_str(db_dev(), key, val, sizeof val, "1");
	x = atof(val);
	return x >= 0.5 && x <= 2.0 ? x : 1.0;
}

/* Y on a book's Now Playing: the next speed, 1x to 2x in quarters and then
 * 0.75x before coming round - the speeds a listener reaches for, in the order
 * they reach for them. Set on the book as it plays and kept for it. */
void book_cycle_speed(void)
{
	static const double STEPS[] = { 1.0, 1.25, 1.5, 1.75, 2.0, 0.75 };
	int al = ml_album_of(&g_muse, musec_track(musec_now()->index));
	double now = musec_speed(), next = STEPS[0];
	char key[LIB_PATH + 16], val[16];
	size_t k;

	for (k = 0; k < sizeof STEPS / sizeof STEPS[0]; k++)
		if (fabs(STEPS[k] - now) < 0.01) {
			next = STEPS[(k + 1) % (sizeof STEPS / sizeof STEPS[0])];
			break;
		}
	musec_set_speed(next);
	if (al < 0 || !g_muse.albums[al].book) return;
	book_speed_key(al, key, sizeof key);
	snprintf(val, sizeof val, "%.2f", next);
	db_set_str(db_dev(), key, val);
}

/* A new queue. Whatever book it replaces has its place written first, since
 * the queue is the only record of where that was. */
void muse_play(const char *const *paths, int n, int start, double at,
               bool book, double speed, const char *artist, const char *album)
{
	book_keep(true);
	g_bk.al = -1;
	g_vk.track[0] = '\0';
	musec_play(paths, n, start, at, book, speed, artist, album);
}

/* A queue of videos, from `start`, at its file's kept place. */
void muse_play_videos(const char *const *paths, int n, int start,
                      const char *artist, const char *album)
{
	book_keep(true);
	g_bk.al = -1;
	g_vk.track[0] = '\0';
	musec_play_videos(paths, n, start, start >= 0 && start < n ? video_place(paths[start]) : 0,
	                  artist, album);
}

/* A track's artist and album, as the shelf names them: for a queue drawn from
 * every album, Shuffle All's, which has no one album to name. */
static void muse_track_names(const char *track, char *artist, size_t an,
                             char *album, size_t aln)
{
	int al = ml_album_of(&g_muse, track);

	artist[0] = album[0] = '\0';
	if (al < 0) return;
	snprintf(artist, an, "%s", g_muse.artists[muse_artist_of(al)].name);
	snprintf(album, aln, "%s", g_muse.albums[al].name);
}

/* The play modes as the player meets them: the name saved in the settings and
 * the mark. In order has no mark: it is what plays when nothing has been asked
 * for, and a mark meaning "nothing special" is noise.
 *
 * The name each mode was SHOWN by went with the notice that showed it - see
 * muse_cycle_mode. Nothing spells a mode out now; the mark beside the track
 * count on Now Playing is the whole of it. */
const muse_mode_name MUSE_MODES[MUQ_MODES] = {
	[MUQ_IN_ORDER]   = { "in order",   -1 },
	[MUQ_REPEAT_ALL] = { "repeat all", UI_GLYPH_REPEAT },
	[MUQ_REPEAT_ONE] = { "repeat one", UI_GLYPH_REPEAT_ONE },
	[MUQ_SHUFFLE]    = { "shuffle",    UI_GLYPH_SHUFFLE },
};

/* The cover on the Now Playing screen, kept between visits so that opening it
 * is not a decode: the album it is for and its texture. `done` means this is
 * what the album will show - its art, or the card made for one with none - and
 * nothing more needs asking. */
np_cover_state g_np = { .album = -1 };

/* The menus' panel pass (panel_pass_begin): a screen, kept rather than made
 * per frame. */
static SDL_Texture *g_panel_tex;

/* Game cards keep their theme tags; home art is theme-independent. */
bool tex_stale(SDL_Texture *t)
{
	return t && !cardgen_kept(ui_card_theme(t), theme_index());
}
static bool g_loads_held;

/* Explicit home inputs. The renderer never changes the library or owns game art. */
void home_items(const app *a, home_item *items)
{
	for (int i = 0; i < a->sys.count; i++)
		items[i] = (home_item){ a->sys.systems[i].tag, a->sys.systems[i].name };
}
void prime_home(app *a)
{
	if (a->screen != SCREEN_SYSTEMS || g_loads_held) return;
	home_item items[CFG_MAX_SYSTEMS];
	home_items(a, items);
	home_prepare(&a->home, a->r, P_ROOT, items, a->sys.count,
	             a->sys_cursor, plat_now_ms());
}

/* Where a game's autosave preview lives: the frame the player was looking at
 * when they stopped. The directory is named after the ROM's folder under
 * Roms/.
 *
 * The directory and the slot names are ZyperOS's own. Both were briefly
 * borrowed from the emulator ZyperOS replaced, and the borrowing cost more than
 * it saved: a name that describes another project invites a reader to treat
 * the contents as dead, and on 2026-08-28 the live autosave tree was deleted
 * by someone auditing the card for exactly that reason. Nothing was lost only
 * because states were being cleared for testing the same hour.
 *
 * Renamed with no migration path, deliberately - nothing has shipped, so there
 * are no cards in the world to be kind to. */
/* Six manual save slots and one autosave.
 *
 * Six because it is enough and it fits the carousel's dot rail without
 * crowding, not because any particular number is correct. An earlier eight was
 * copied from another launcher and had no reasoning of its own behind it.
 *
 * The resume slot is named rather than numbered on disk: `<game>.auto.state`
 * explains itself, where a number needs a reader to already know which slot
 * means auto. Its value is only an internal sentinel and must not collide with
 * 1..GM_SLOTS, so it is derived rather than picked. Both live in game_menu.h
 * now, with the carousel mapping beside them, so a check can reach all three.
 */

static const char *slot_name(int slot)
{
	static char buf[8];
	if (slot == SLOT_AUTO) return "auto";
	snprintf(buf, sizeof buf, "%d", slot);
	return buf;
}

/* The name a game's states and previews go by: its file's, which for a disc
 * game in a folder is the disc's ("Game (Disc 1).chd"). On a hooked shelf the
 * folder is the game and the file is whatever it held first - readme.txt for
 * two different ScummVM games - so the folder's name. And so for a PSN game:
 * a folder holding EBOOT.PBP, the same name in every such game's folder (a
 * PSP game's, a PlayStation classic's), whose states would all be one. */
static const char *persist_base(app *a, int s, const game_entry *g)
{
	const char *base = strrchr(g->file, '/');

	if (base && (lib_shelf_hooked(a->sys.systems[s].exts) || !strcasecmp(base + 1, "EBOOT.PBP")))
		return g->name;
	return base ? base + 1 : g->file;
}

void preview_path(app *a, int s, const game_entry *g, char *out, size_t n)
{
	snprintf(out, n, "%s/.zyperos/%s/%s.%s.bmp",
	         P_SHARED, a->sys.systems[s].folder, persist_base(a, s, g),
	         slot_name(SLOT_AUTO));
}

/* Where Diatom writes the frame it hands over, which is NOT the Auto card's
 * picture.
 *
 * Diatom writes its preview twice: before PAUSED, so the launcher has a frame
 * to dim and draw the menu over, and again at exit beside the state. Pointing
 * both at <rom>.auto.bmp meant every menu open overwrote the Auto card with
 * the current moment, while the card's timestamp still came from the state -
 * so the card contradicted itself and the picture was the convincing half.
 * Reported 2026-09-07: died in Contra, opened the menu, and the Auto card
 * showed the death while loading it went back two levels.
 *
 * One scratch file for the whole device rather than one per game: nothing
 * reads it after the launch that wrote it, and a per-game copy would leave
 * litter beside every save. */
void pause_preview_path(char *out, size_t n)
{
	snprintf(out, n, "%s/pause.bmp", P_USERDATA);
}

/* The Diatom transport takes explicit paths rather than a slot number, so the
 * state lives beside the preview it belongs to, named the same way.
 *
 * The autosave is named `auto`, not a number. Slots 1..GM_SLOTS are the
 * player's own,
 * reachable from the in-game menu, and are genuinely numbered; the resume slot
 * is not one of them and calling it 9 only meant something to someone who knew
 * which emulator picked that number. Each slot is a state and a preview named
 * alike, so a slot that has a picture has a game behind it. */
void slot_state_path(app *a, int s, const game_entry *g, int slot,
                     char *out, size_t n)
{
	snprintf(out, n, "%s/.zyperos/%s/%s.%s.state",
	         P_SHARED, a->sys.systems[s].folder, persist_base(a, s, g),
	         slot_name(slot));
}

void slot_preview_path(app *a, int s, const game_entry *g, int slot,
                       char *out, size_t n)
{
	snprintf(out, n, "%s/.zyperos/%s/%s.%s.bmp",
	         P_SHARED, a->sys.systems[s].folder, persist_base(a, s, g),
	         slot_name(slot));
}

void state_path(app *a, int s, const game_entry *g, char *out, size_t n)
{
	slot_state_path(a, s, g, SLOT_AUTO, out, n);
}

/* The hotkeys' copy of where this game's slots are (g_quick), at its launch:
 * slot_state_path's, less the slot */
void quick_bind(app *a, int s, const game_entry *g)
{
	const system_cfg *sc = &a->sys.systems[s];

	memset(&g_quick, 0, sizeof g_quick);
	g_quick.sys = *sc;
	snprintf(g_quick.tag, sizeof g_quick.tag, "%s", sc->tag);
	snprintf(g_quick.name, sizeof g_quick.name, "%s", sc->name);
	snprintf(g_quick.file, sizeof g_quick.file, "%s", g->file);
	snprintf(g_quick.stem, sizeof g_quick.stem, "%s/.zyperos/%s/%s",
	         P_SHARED, sc->folder, persist_base(a, s, g));
	g_quick.states = ctl_has_states(sc->tag);
}

/* The launcher owns the paths, so the launcher makes the directories - the
 * emulator writes where it is told and fails where it cannot. A missing mkdir
 * here once cost every preview on the card, silently, because writing to a
 * path in a directory that does not exist fails per-write and looks like
 * nothing happening. */
/* Where the two databases live, and the directories they need. Both callers
 * are in main(): --dump, which runs before video, and the boot path. Having
 * one of them create the directory and the other not is how --dump failed on a
 * fresh tree the first time it was tried. */
static bool db_paths_ready(char *dev, size_t ndev, char *lib, size_t nlib,
                           char *env, size_t nenv)
{
	char d[CFG_STR * 2];

	mkdir(P_USERDATA, 0755);
	mkdir(P_SHARED, 0755);
	snprintf(d, sizeof d, "%s/.zyperos", P_SHARED);
	mkdir(d, 0755);

	snprintf(dev, ndev, "%s/zyperos.db", P_USERDATA);
	snprintf(lib, nlib, "%s/.zyperos/library.db", P_SHARED);
	if (env) snprintf(env, nenv, "%s/boot.env", P_USERDATA);
	return true;
}

void persist_dir_ensure(app *a, int s)
{
	char d[LIB_PATH * 2];
	snprintf(d, sizeof d, "%s/.zyperos", P_SHARED);
	mkdir(d, 0755);
	snprintf(d, sizeof d, "%s/.zyperos/%s", P_SHARED, a->sys.systems[s].folder);
	mkdir(d, 0755);
}

/* Card art, in order of preference: the box art the user put in .media/, then
 * the autosave preview -- the last frame they saw, which for a game in
 * progress is a better card than any box -- then a generated slab. */
/* The system an entry on a shelf belongs to. Real shelves answer with
 * themselves; Favorites answers per entry. */
int shelf_owner(app *a, int sysidx, int item)
{
	sysview *v = &a->view[sysidx];
	if (v->owner && item >= 0 && item < v->list.count) return v->owner[item];
	return sysidx;
}

/* The view whose SYSTEM settings the game under the cursor belongs to.
 *
 * Favorites is a shelf of other systems' games, so the two kinds of setting
 * have to be told apart: a property of the SYSTEM - the core, the ROM folder,
 * the BIOS, the display mode - resolves through the game's owner, while a
 * property of the SHELF - its sort order, its cursor, its own card - belongs
 * to the shelf you are standing on.
 *
 * launch() had that right for three of the four and wrong for the display
 * mode, which it read off the shelf: a Genesis game set to integer on its own
 * shelf played stretched when it was started from Favorites, because the mode
 * it used was the one filed under FAV. Nothing showed it on this card, where
 * every system reads stretch and all the answers agree. Named as a function
 * rather than repeated at each site so the next per-system setting has one
 * obvious thing to call. */
sysview *owner_view(app *a)
{
	sysview *v = &a->view[a->sys_cursor];

	return &a->view[shelf_owner(a, a->sys_cursor, v->cursor)];
}

/* Where a game's box art lives.
 *
 * One definition because four places need the same file - the card, the info
 * screen, the replace action, and the return from a game - and the last of
 * those has to test exactly the file the card loads. Four hand-written copies
 * of the format string only have to drift once for it to ask about one file
 * and draw another. */
void box_art_path(const char *folder, const char *name, char *out, size_t n)
{
	snprintf(out, n, "%s/%s/.media/%s.png", P_ROMS, folder, name);
}

/* Whether it is there. An empty file is not art - the rule the info screen
 * already used, now the rule for everyone. */
bool has_box_art(const char *folder, const char *name)
{
	char p[LIB_PATH * 3];
	struct stat st;

	box_art_path(folder, name, p, sizeof p);
	return stat(p, &st) == 0 && st.st_size > 0;
}

/* EmulationStation's picture for a game with no box art of its own
 * (src/esmedia.h), as the cover worker's second file: the key is the game's
 * folder, file and name, a tab between, found on the worker (TEXLOAD_LATE),
 * where the folder's gamelist.xml may be read the first time */
static void es_cover_key(const char *folder, const game_entry *g, char *out, size_t n)
{
	snprintf(out, n, "%s%s\t%s\t%s", TEXLOAD_LATE, folder, g->file, g->name);
}

/* The folder a computer's file names (--meta, --titles: systems.cfg's, as
 * tools/scrape-art.py writes them) as the card's shelf reads it: an
 * EmulationStation folder where that shelf reads one (DESIGN.md 11.40).
 * The card is read for it once. */
static const char *card_folder(const char *folder)
{
	static systems_cfg cfg;
	static bool read;
	static char out[CFG_STR];

	if (!read) {
		char path[CFG_STR * 2];

		read = true;
		snprintf(path, sizeof path, "%s/systems.cfg", P_ROOT);
		if (cfg_load_systems(path, &cfg)) shelf_folders_from_card(&cfg);
	}
	for (int i = 0; i < cfg.count; i++)
		if (!strcmp(cfg.systems[i].home, folder)) {
			snprintf(out, sizeof out, "%s", cfg.systems[i].folder);
			return out;
		}
	return folder;
}

/* Box Art's question (art_es_media): `dir` is Roms/<folder> */
static bool es_art_has(const char *dir, const char *file, const char *stem, es_kind kind)
{
	char roms[LIB_PATH * 2], p[ES_PATH];
	const char *slash = strrchr(dir, '/');

	if (!slash) return false;
	snprintf(roms, sizeof roms, "%.*s", (int)(slash - dir), dir);
	return es_media(roms, slash + 1, file, stem, kind, true, p, sizeof p);
}

bool es_art_cover(const char *dir, const char *file, const char *stem)
{
	return es_art_has(dir, file, stem, ES_COVER);
}

bool es_art_video(const char *dir, const char *file, const char *stem)
{
	return es_art_has(dir, file, stem, ES_VIDEO);
}

static bool es_cover_late(const char *key, char *path, size_t n)
{
	char k[LIB_PATH * 3], *file, *name;

	snprintf(k, sizeof k, "%s", key);
	if (!(file = strchr(k, '\t')) || !(name = strchr(file + 1, '\t'))) return false;
	*file++ = '\0';
	*name++ = '\0';
	return es_media(P_ROMS, k, file, name, ES_COVER, true, path, n);
}

/* Whether game `i`'s slot on a shelf wants a card: it has none, or it holds a
 * generated card made for the theme that was, which stays on the shelf until
 * its replacement comes (theme_changed). Box art is no theme's and never is
 * stale. */
static bool game_card_wanted(const sysview *v, int i)
{
	return !v->tex[i] || tex_stale(v->tex[i]);
}

SDL_Texture *game_get_tex(void *ctx, int i, int *w, int *h, float *cb)
{
	app *a = ctx;
	int s = a->sys_cursor;
	sysview *v = &a->view[s];
	/* The game's own system, not the shelf's: on Favorites those differ, and
	 * every one of these three lookups would otherwise go to the wrong
	 * folder, the wrong state directory and the wrong accent. */
	int o = shelf_owner(a, s, i);

	char pm[LIB_PATH * 2];

	if (is_muse(&a->sys.systems[s]) && game_card_wanted(v, i)) {
		muse_card_want(a, s, i);
	} else if (game_card_wanted(v, i) && v->list.items[i].folder) {
		/* A folder on the shelf (src/shelfdir.h): its own card, no art */
		ui_card_order card;

		ui_card_order_folder(&card, v->list.items[i].title, a->sys.systems[o].accent,
		                     v->list.items[i].folder);
		if (!texload_want_or_draw(s, i, NULL, NULL, &card) && !g_loads_held) {
			if (v->tex[i]) SDL_DestroyTexture(v->tex[i]);
			v->tex[i] = ui_make_folder_card(a->r, v->list.items[i].title,
			                                a->sys.systems[o].accent,
			                                v->list.items[i].folder, &v->tw[i], &v->th[i]);
			v->cb[i] = 1.0f;
		}
	} else if (game_card_wanted(v, i) &&
	           pm_cover_path(a, o, &v->list.items[i], theme_index(), pm, sizeof pm)) {
		/* PortMaster's own cover, in the theme that is on, made a card of
		 * that theme as a generated one is, so a change of theme asks for
		 * the other (src/shelfbuild.h) */
		if (!texload_want_art(s, i, pm, NULL, theme_index()) && !g_loads_held) {
			SDL_Surface *sf = IMG_Load(pm);

			if (v->tex[i]) SDL_DestroyTexture(v->tex[i]);
			v->tex[i] = NULL;
			if (sf) {
				v->cb[i] = content_bottom(sf);
				v->tex[i] = ui_card_upload(a->r, sf, theme_index());
				if (v->tex[i]) { v->tw[i] = sf->w; v->th[i] = sf->h; }
				SDL_FreeSurface(sf);
			}
		}
	} else if (!is_muse(&a->sys.systems[s]) && game_card_wanted(v, i)) {
		char art[LIB_PATH * 3];
		ui_card_order card;

		char es[LIB_PATH * 3];

		box_art_path(a->sys.systems[o].folder, v->list.items[i].name,
		             art, sizeof art);
		/* and EmulationStation's, for a game with none of its own */
		es_cover_key(a->sys.systems[o].folder, &v->list.items[i], es, sizeof es);
		/* Hand it to the worker and draw what the slot holds this frame:
		 * nothing, or the card of the theme that was until its replacement
		 * lands (theme_changed). Asking is cheap and ignores a card already
		 * in hand, so callers ask every frame. With the card to draw if
		 * there is no box art, so that is the worker's to draw as well. */
		ui_card_order_set(&card, v->list.items[i].title,
		                  a->sys.systems[o].accent, false);
		bool asked = texload_want_or_draw(s, i, art, es, &card);

		if (!asked && !g_loads_held) {
			/* No worker. Exactly what this did before, on the frame, over
			 * the card of the theme that was if there is one. */
			if (v->tex[i]) SDL_DestroyTexture(v->tex[i]);
			v->tex[i] = load_image(a->r, art, &v->tw[i], &v->th[i], &v->cb[i]);
			if (!v->tex[i] && es_cover_late(es + 1, es, sizeof es))
				v->tex[i] = load_image(a->r, es, &v->tw[i], &v->th[i], &v->cb[i]);
			if (!v->tex[i])
				v->tex[i] = ui_make_card(a->r, v->list.items[i].title,
				                         a->sys.systems[o].accent,
				                         &v->tw[i], &v->th[i]);
		}
	}
	if (!v->tex[i]) {
		/* Still coming. cf_draw skips a card with no texture, so the slot is
		 * simply empty for a few frames instead of the shelf stopping. */
		*w = *h = 0;
		if (cb) *cb = 1.0f;
		return NULL;
	}
	*w = v->tw[i];
	*h = v->th[i];
	if (cb) *cb = v->cb[i];
	return v->tex[i];
}


/* One finished picture into its slot: texload_deliver's `put`. */
void shelf_put(void *ctx, texload_got *g)
{
	app *a = ctx;
	SDL_Surface *surf = g->surf;
	int s = g->sys, i = g->idx;
	sysview *v;

	if (home_accept(&a->home, a->r, s, i, surf)) {
		if (surf) SDL_FreeSurface(surf);
		return;
	}
	if (s < 0 || s >= a->sys.count) { if (surf) SDL_FreeSurface(surf); return; }
	v = &a->view[s];
	/* The shelf may have moved on, or something else may have filled this
	 * slot while the decode was in the air. A slot holding a generated
	 * card of the theme that was is not filled: it is waiting for this. */
	if (i < 0 || i >= v->list.count || !game_card_wanted(v, i)) {
		if (surf) SDL_FreeSurface(surf);
		return;
	}
	/* The card of the theme that was, drawn until now, goes as this one
	 * comes in. */
	if (v->tex[i]) {
		SDL_DestroyTexture(v->tex[i]);
		v->tex[i] = NULL;
	}
	if (surf && g->card) {
		/* The generated card, drawn on the worker: only its texture is
		 * made here, marked with its theme as one made here would be. */
		v->tex[i] = ui_card_upload(a->r, surf, g->theme);
		if (v->tex[i]) { v->tw[i] = surf->w; v->th[i] = surf->h; }
		v->cb[i] = 1.0f;
		SDL_FreeSurface(surf);
		surf = NULL;
	}
	/* A cover is squared and given a card's corners, the way Now
	 * Playing draws it, so one album looks the same in both places. */
	if (surf && is_muse(&a->sys.systems[s])) cover_shape(&surf);
	if (surf) {
		v->cb[i] = content_bottom(surf);
		/* Art drawn for one theme (PortMaster's cover) is kept as a card
		 * of that theme is, and gives way to the other's at a change */
		v->tex[i] = g->theme >= 0 ? ui_card_upload(a->r, surf, g->theme)
		                          : SDL_CreateTextureFromSurface(a->r, surf);
		if (v->tex[i])
			SDL_QueryTexture(v->tex[i], NULL, NULL, &v->tw[i], &v->th[i]);
		SDL_FreeSurface(surf);
	}
	if (!v->tex[i]) {
		/* No file could be read and no card came with it - the worker could
		 * not have the faces: the generated card, made here. */
		int o = shelf_owner(a, s, i);
		v->tex[i] = is_muse(&a->sys.systems[s])
		          ? ui_make_cover(a->r, v->list.items[i].title,
		                          a->sys.systems[o].accent, &v->tw[i], &v->th[i])
		          : v->list.items[i].folder
		          ? ui_make_folder_card(a->r, v->list.items[i].title,
		                                a->sys.systems[o].accent, v->list.items[i].folder,
		                                &v->tw[i], &v->th[i])
		          : ui_make_card(a->r, v->list.items[i].title,
		                         a->sys.systems[o].accent,
		                         &v->tw[i], &v->th[i]);
		v->cb[i] = 1.0f;
	}
}

/* Finished art into textures, as much of it as fits in this frame's share
 * (texload_deliver): a landing or a fast scroll finishes a dozen at once,
 * and one frame making every texture was the frame the shelf hitched on. The
 * rest come in the next frames, which the loops draw while any is waiting
 * (texload_ready). */
void texload_drain(app *a)
{
	texload_deliver(theme_index(), shelf_put, a);
}

void evict_far(sysview *v, int keep)
{
	for (int i = 0; i < v->list.count; i++) {
		int d = abs(i - v->cursor);
		if (v->list.count >= CF_WINDOW)
			d = d > v->list.count / 2 ? v->list.count - d : d;
		if (v->tex[i] && d > keep) {
			SDL_DestroyTexture(v->tex[i]);
			v->tex[i] = NULL;
		}
	}
}

#define PRIME_HALF_MAX (CF_HALF_WINDOW + 2)

/* Ask for the cards on shelf `s` around `center`, out to `half` either side.
 *
 * FARTHEST FIRST. The workers take the newest request first (texload.c), so
 * they decode in the reverse of the order asked, and the center has to be asked
 * for last to be decoded first. Both callers used to ask for it first or in the
 * middle, on the reasoning that the order asked is the order decoded - true of
 * the queue they were written against, and backwards since it went newest-first.
 * Measured 2026-09-14, cold after a reboot: entering a system put the focused
 * card up 70-160ms after A, only 15-35ms ahead of the rest of its row, when it
 * should have been the first to arrive.
 *
 * Each card is asked for once. On a shelf shorter than the window, wrapping
 * offers the same card from both sides, and the second ask would be refused as
 * already queued - leaving the card wherever the first ask put it.
 *
 * game_get_tex reads a->sys_cursor rather than taking a shelf, so it is swapped
 * around the calls. */
static void prime_around(app *a, int s, int center, int half)
{
	sysview *v = &a->view[s];
	int n = v->list.count, idx[2 * PRIME_HALF_MAX + 1], m = 0;
	int k, j, save = a->sys_cursor;

	if (n <= 0) return;
	if (half > PRIME_HALF_MAX) half = PRIME_HALF_MAX;
	for (k = 0; k <= half; k++) {
		int side;
		for (side = 1; side >= (k ? -1 : 1); side -= 2) {
			int i = center + k * side;
			if (n >= 2) {
				i %= n;
				if (i < 0) i += n;
			} else if (i != 0) {
				continue;
			}
			for (j = 0; j < m && idx[j] != i; j++) ;
			if (j == m) idx[m++] = i;
		}
	}
	a->sys_cursor = s;
	while (m-- > 0) {
		int w, h;
		if (game_card_wanted(v, idx[m])) game_get_tex(a, idx[m], &w, &h, NULL);
	}
	a->sys_cursor = save;
}

/* The visible row and two more either side, so the first scroll finds its
 * neighbors already there instead of sliding in blank. */
void prime_window(app *a, int s)
{
	prime_around(a, s, a->view[s].cursor, PRIME_HALF_MAX);
}

/* Ask for the whole window around `center`.
 *
 * This used to take a budget and spend one card per frame, because asking
 * meant decoding and a card was 7-27ms against a 16.7ms frame. Since
 * texload.c, asking only queues: game_get_tex hands the worker two paths and
 * returns, a card already in hand is ignored, so the whole window costs about
 * nothing and dribbling it out a frame at a time only made it land later. */
void prime_toward(app *a, int s, int center)
{
	prime_around(a, s, center, CF_HALF_WINDOW);
}


void free_view_textures(sysview *v)
{
	int k;

	texload_bump();
	for (k = 0; k < v->list.count; k++) {
		if (v->tex && v->tex[k]) {
			SDL_DestroyTexture(v->tex[k]);
			v->tex[k] = NULL;
		}
		if (v->tw) v->tw[k] = 0;
		if (v->th) v->th[k] = 0;
		if (v->cb) v->cb[k] = 0.0f;
	}
}

/* ---- folders on a shelf (src/shelfdir.h) ---- */

/* The list view's pixel folder (folder_icon, with draw_games) */
SDL_Texture *g_folder_icon;

void shelf_open_dir(app *a, int s, const char *dir, const char *select)
{
	sysview *v = &a->view[s];
	int i;

	if (!v->all) return;
	texload_bump();
	shelf_show_dir(v, dir);
	sort_shelf(a, s);
	v->cursor = 0;
	for (i = 0; select && i < v->list.count; i++)
		if (!strcmp(v->list.items[i].file, select)) { v->cursor = i; break; }
	v->lm.set = false;                   /* the list finds its window again */
	cf_reset(&v->cf, v->cursor);
	games_reset(&v->row, v->cursor, v->list.count, plat_now_ms());
}

void shelf_reveal(app *a, int s, const char *file)
{
	sysview *v = &a->view[s];
	char dir[LIB_PATH];
	int k;

	if (v->all && (k = lib_find_entry(v->all, NULL, 0, file)) >= 0) {
		sd_dir_of(&v->all->items[k], dir, sizeof dir);
		if (strcmp(dir, v->dir)) { shelf_open_dir(a, s, dir, file); return; }
	}
	if ((k = lib_find_entry(&v->list, NULL, 0, file)) >= 0) v->cursor = k;
}

/* The folder on the shelf under the cursor, opened (A), or the shelf's own
 * folder left for the one it is in (B), its cursor on the folder it left */
void shelf_into(app *a, sysview *v)
{
	char dir[LIB_PATH];

	snprintf(dir, sizeof dir, "%s", v->list.items[v->cursor].file);
	fprintf(stderr, "shelf: into %s/%s\n", a->sys.systems[a->sys_cursor].folder, dir);
	shelf_open_dir(a, a->sys_cursor, dir, NULL);
}

void shelf_up(app *a, sysview *v)
{
	char was[LIB_PATH], up[LIB_PATH];

	snprintf(was, sizeof was, "%s", v->dir);
	sd_parent(was, up, sizeof up);
	shelf_open_dir(a, a->sys_cursor, up, was);
}

/* Muse's shelf onto another kind: all of them, music, books or videos, from
 * the first card. The textures go first, while the count is still the old
 * kind's, since every card is about to be a different album. */
static const char *const MUSE_SHOW[ML_KINDS + 1] = { "music", "books", "videos", "all" };

void muse_show(sysview *v, ml_kind kind)
{
	g_muse_kind = kind;
	db_set_str(db_dev(), "muse.show", MUSE_SHOW[kind >= 0 && kind <= ML_ALL ? kind : ML_ALL]);
	free_view_textures(v);
	muse_order_view(v);
	v->cursor = 0;
	cf_reset(&v->cf, 0);
}

void free_all_textures(app *a)
{
	/* Anything in flight was asked for against the shelf as it was. */
	texload_bump();
	if (g_panel_tex) { SDL_DestroyTexture(g_panel_tex); g_panel_tex = NULL; }
	zyper_shadows_free();
	home_free(&a->home);
	/* Now Playing's cover is a texture of this renderer too, and the fallback
	 * that runs a game (launch) destroys the renderer after this: left,
	 * g_np.tex would dangle, and np_cover, which sees it made, would draw it.
	 * Forgotten, np_cover makes it again on the frame that next draws it. */
	np_forget();
	if (g_folder_icon) { SDL_DestroyTexture(g_folder_icon); g_folder_icon = NULL; }
	for (int i = 0; i < a->sys.count; i++) {
		for (int k = 0; k < a->view[i].list.count; k++)
			if (a->view[i].tex[k]) {
				SDL_DestroyTexture(a->view[i].tex[k]);
				a->view[i].tex[k] = NULL;
			}
	}
}


/* Whether the radio should come up at boot.
 *
 * Per-device rather than per-card, on the same split as brightness: it is what
 * THIS handheld was last doing. The shipped default seeds it once, so there is
 * no fallback chain left for launch.sh to walk.
 *
 * Written the moment it changes rather than at shutdown, because a handheld
 * is switched off by holding a button or by running the battery flat, and
 * neither of those is a chance to save anything. */
static void wifi_pref_save(bool on)
{
	db_set_int(db_dev(), "wifi", on ? 1 : 0);
	/* launch.sh reads this one before the launcher exists, so the export has
	 * to follow the write rather than wait for a tidy shutdown. */
	db_write_boot_env();
}


/* Diatom watches this file, and it is rewritten per launch: it is the set
 * minus what has already been earned, which only this side knows. Runtime
 * scratch, so per-device rather than shared. */
void chv_active_path(char *out, size_t n)
{
	snprintf(out, n, "%s/cheevos-active.set", P_USERDATA);
}

/* Auto Off: how long without input before the device powers itself down.
 *
 * Seconds, 0 for off. Eric's ladder, and it is deliberately aggressive at the
 * short end - 30s will fire while you read a dialogue box. That is a sound
 * trade only because resume-into-game exists: powering off costs about two
 * seconds and puts you back in the same game with the menu up. Without that
 * it would be hostile. */
const int AUTO_OFF[] = { 0, 30, 60, 120, 300, 600 };

static int auto_off_load(void)
{
	int v = db_get_int(db_dev(), "autooff", -1);
	return v >= 0 ? v : 120;                  /* the default: two minutes */
}

void auto_off_save(int seconds)
{
	db_set_int(db_dev(), "autooff", seconds);
}

/* MENU > Screensaver (src/saver.h): the seconds with nothing pressed on the
 * shelf before the video snaps play, 0 for never. Stepped round on Auto Off's
 * ladder (saver_step); a minute to start with. */
int g_saver_s = SAVER_DEFAULT_S;

static int saver_load(void)
{
	int v = db_get_int(db_dev(), "saver", -1);

	return v >= 0 ? v : SAVER_DEFAULT_S;
}

void saver_save(int seconds)
{
	db_set_int(db_dev(), "saver", seconds);
}

/* MENU > Charge Limit (DESIGN.md 11.26; my picks in the charge limit
 * lab, 2026-11-06): the charge the battery stops at to keep it healthy for
 * longer, 0 for Off, a full charge, as a new card has. On a charger that can
 * stop short (plat_charge_limit_supported): plastron's rk817 driver turns it
 * off at the limit and on again 2 points under, so 80% holds between 78 and
 * 80%. The kernel forgets it at a power-off, so it is set at every start. */
const int CHARGE_LIMITS[] = { 0, 80, 85, 90 };
/* "Hold A for a full charge" on the charging screen: this charge goes to
 * 100%, and the limit is back when the cable comes out (charge_full_end).
 * In /tmp, so a power-off ends it too. */
#define CHG_FULL_ONCE "/tmp/zyperos_charge_full"
int g_charge_limit;

static int charge_limit_load(void)
{
	int v = db_get_int(db_dev(), "chargelimit", 0), k;

	for (k = 0; k < CHARGE_LIMITS_COUNT; k++)
		if (CHARGE_LIMITS[k] == v) return v;
	return 0;
}

/* The limit in force: 0 for none, Off or a full charge asked for */
int charge_limit_now(void)
{
	if (!g_charge_limit || access(CHG_FULL_ONCE, F_OK) == 0) return 0;
	return g_charge_limit;
}

void charge_limit_apply(void)
{
	int l = charge_limit_now();

	if (plat_charge_limit_supported()) plat_charge_limit_set(l ? l : 100);
}

/* A full charge, this time: the limit off until the cable comes out */
void charge_full_once(void)
{
	FILE *f = fopen(CHG_FULL_ONCE, "w");

	if (f) fclose(f);
	charge_limit_apply();
	fprintf(stderr, "charging: a full charge, this time\n");
}

static bool g_full_shot;          /* --charging N tofull: asked for, in a shot */
bool charge_full_active(void) { return g_full_shot || access(CHG_FULL_ONCE, F_OK) == 0; }
int  charge_limit_saved(void) { return g_charge_limit; }

/* The cable out: a full charge asked for is over, and the limit back */
void charge_full_end(void)
{
	if (access(CHG_FULL_ONCE, F_OK) != 0) return;
	remove(CHG_FULL_ONCE);
	charge_limit_apply();
	fprintf(stderr, "charging: the full charge is over, the limit back\n");
}

/* ---------- what the account already has, read while the game runs ------- */

/* The answer to "which of this game's achievements does the account hold",
 * asked at launch and read as soon as it lands - from the game tick during a
 * game, from the main loop on the shelf.
 *
 * It used to be read only when the game ended, waiting there until it had
 * landed. So a game's first session after signing in showed 0 of N in its own
 * menu while the account held 13, and a quit straight after a launch on a slow
 * network sat out the request between the game's wait loop and the shelf's.
 * Now the menu is right about half a second in, and quitting never waits.
 *
 * The unlocks owed still go out after the answer, not before it: settling what
 * the account already holds first is what stops them being sent as duplicates.
 * So a quit that beats the answer leaves the sending to whichever comes first
 * - the answer landing, or something else needing the one async slot, which
 * gives up on the answer and sends anyway. Duplicates that causes are answered
 * as already held, which settles them just the same. */
long g_sync_game;                   /* the game asked about, 0 none */
static char g_sync_set[LIB_PATH * 2];      /* its set, to check the answer against */

/* A send waiting for the answer. */
bool g_sync_flush;
char g_sync_rom[LIB_PATH * 2], g_sync_tag[16];
int  g_sync_cur;

void sync_begin(long game, const char *set_path)
{
	ra_sync_begin(game);
	g_sync_game = ra_sync_pending() ? game : 0;
	snprintf(g_sync_set, sizeof g_sync_set, "%s", set_path ? set_path : "");
}

static void sync_flush_now(void)
{
	if (!g_sync_flush) return;
	g_sync_flush = false;
	ra_flush_start(g_sync_rom, g_sync_tag, g_sync_cur);
}

/* The answer, merged if it is in. Returns how many achievements it added to
 * what this device knew - nonzero means the list Diatom is watching still
 * holds some the account already has. Cheap when nothing has landed: a
 * waitpid that does not wait. */
int sync_poll(void)
{
	int ids[CHV_MAX], n, rc, added = 0;

	if (!g_sync_game) return 0;
	rc = ra_sync_poll(ids, CHV_MAX, &n);
	if (rc == 0) return 0;

	if (rc > 0 && n > 0) {
		/* Against the set the question was about. During a game that is the
		 * one loaded; on the shelf the info screen loads whichever set it is
		 * showing, and nothing on the shelf depends on which one that is. */
		if (chv_game() != g_sync_game) chv_load(g_sync_set);
		if (chv_game() == g_sync_game) added = ra_merge_unlocks(ids, n);
	}
	g_sync_game = 0;
	sync_flush_now();
	return added;
}

/* Something else needs the one async slot now - a launch, the box art
 * scraper. The answer is dropped, and the next launch of that game asks
 * again. */
void sync_abandon(void)
{
	if (g_sync_game) {
		ra_sync_abandon();
		g_sync_game = 0;
		fprintf(stderr, "ra: dropped an account answer that had not landed\n");
	}
	sync_flush_now();
}

/* Where the launcher's own work happens during a game. Ten times a second,
 * from inside the wait loop, and it must stay cheap: that loop is the power
 * button's watchdog.
 *
 * Its one job is the first play of a game. The set is being found behind the
 * game rather than in front of it, and when it lands the player is told the
 * only way anyone can be told during a game - over Diatom's overlay. */
char g_pending_set[LIB_PATH * 2];
char g_pending_active[LIB_PATH * 2];
/* ...whose hash is still being made, on its own thread (src/rahashjob.h) */
bool g_pending_hashing;
/* Auto crossed 7:00 or 19:00 while a game ran, and the file was rewritten
 * for Diatom's bar but nobody has told the shelf: theme() is the new theme
 * and the cards kept are the old one's. tick_theme takes it as a change like
 * its own clock's, so the first screen to ask after on_game_tick set it
 * tells theme_changed - the in-game menu and Muse opened from it, during the
 * game, or the way back from it (launch) before its first frame - and
 * theme_changed puts it down. */
bool g_theme_late;
/* The watch list Diatom was last handed and when, so an account answer that
 * lands early can trim it. 0 when there is no list to trim. */
#define WATCH_RETRIM_MS 5000
unsigned g_watch_ms;
char     g_watch_active[LIB_PATH * 2];
/* The set path of the game actually running, so a fetch that lands during a
 * DIFFERENT game can be recognized and dropped. Written at every launch. */
char g_game_set[LIB_PATH * 2];
/* And whether the launcher has a hash for it at all (ra_hash_support), for
 * the in-game menu's Achievements row. Written beside it. */
ra_hash_kind g_game_no_hash;

/* Auto Off while a game runs. Diatom holds the clock, because it owns the pad
 * and this process is blocked in plat_resident_wait; the launcher's part is
 * telling it the number and keeping that number true as the charger comes and
 * goes. See on_charger, which lives beside battery_level. */
int  g_idle_secs;         /* the Auto Off setting, 0 off */
bool g_idle_charging;     /* what was true when SETIDLE was last sent */

/* "Getting hot" (src/heat.h, DESIGN.md 11.23): the chip's temperature, read
 * every HEAT_POLL_MS by every screen through idle_due and by a game's tick,
 * and said where it can be seen - Zyper's pop-up on the home and the shelves
 * (heat_watch), a notice over a running game (on_game_tick). The words are
 * short enough for the pop-up's slab without scrolling. */
heat_state g_heat;

void heat_tick(void)
{
	heat_poll(&g_heat, plat_now_ms(), plat_chip_temp);
}

/* Said: one line in the log with the reading, pop-up or notice */
void heat_said(const char *where)
{
	fprintf(stderr, "heat: %d.%d C, getting hot (%s)\n", g_heat.mc / 1000,
	        (g_heat.mc % 1000) / 100, where);
}

/* The pop-ups over a game, further down */
static void notice_pump(void);
static void heat_notice(void);

static void on_game_tick(void)
{
	char msg[96];

	/* Developer Mode's SSH, which a game does not end (see dev_ssh_tick) */
	dev_ssh_tick();
	/* System Update behind the game: its looks, a cut download going on,
	 * and its news as a notice */
	tick_update_daily(true);
	/* px2-open's status and quit, and an open that ends this game first */
	if (g_ctl_app) ctl_poll(g_ctl_app, CTL_AT_GAME);
	int rc;

	/* Muse, bounded: read what has arrived, and send at most one PLAY when a
	 * track ends - which is what keeps an album going through a game.
	 *
	 * And the game goes quiet while it plays - Diatom's QUIET, followed
	 * here at ten a second, which is how pausing the music or the album
	 * ending brings the game's sound back within a tenth of a second. Only a
	 * change is sent. */
	muse_poll();
	plat_resident_quiet(musec_playing());
	/* The next pop-up, once the one on screen has had its time */
	notice_pump();
	/* A computer on the cable that has not connected: said over the game
	 * too, until the cable comes out (DESIGN.md 11.36) */
	replug_game_tick();
	/* NES Player 2 (src/p2.h): the hotspot coming up and p2d after it, and
	 * a friend joining or leaving said over the game. Bounded. */
	p2_tick(plat_now_ms());
	/* The chip's temperature, every HEAT_POLL_MS; "Getting hot" over the
	 * game when it passes the trip, ahead of any pop-up waiting */
	heat_tick();
	if (heat_take(&g_heat, plat_now_ms())) heat_notice();
	/* Auto's clock, for the file and for nothing else. Diatom reads the theme
	 * from THEME_FILE as its bar comes up, and the launcher is in this wait
	 * for the whole of a game, so a game that is on across 19:00 would have
	 * its bar in the theme of the hour it started in. theme_tick writes the
	 * file when the theme changes and draws nothing, and it looks at the
	 * clock once a minute, so it costs a compare here. The screens are told
	 * by the next one that asks, through tick_theme: g_theme_late. */
	if (theme_tick()) {
		g_theme_late = true;
		/* and NES Player 2's page, for its next load */
		p2_theme_changed();
	}
	/* The low-battery disc over the game, which Diatom draws as this process
	 * can draw nothing while a game runs, blinking when critical. Sent only
	 * on a change; the level is read every five seconds. */
	plat_resident_low_battery(battery_level());

	/* Play time. Writes at most one unsynced row and usually nothing, and
	 * never an fsync: measured on the card, an fsync is 1.56 ms median but
	 * 16.4 ms at the tail, which is a dropped frame. */
	stats_tick(plat_now_ms());

	/* The charger, followed rather than sampled once at launch. Plugging in
	 * mid-game has to stop the countdown and unplugging has to start a fresh
	 * one, and SETIDLE does both: Diatom restarts its clock on every one it
	 * receives.
	 *
	 * Which is also why this only sends on a CHANGE. Sending it every tick
	 * would restart that clock ten times a second and Auto Off would never
	 * fire at all. */
	/* A cable plugged in mid-game, or a headset that connected or walked out
	 * of range, moves the sound without leaving the game. That is the whole
	 * point of Diatom making this a state rather than a launch argument. */
	g_game_ticking = true;             /* see bt_volume_follow */
	aout_apply(false);
	g_game_ticking = false;
	aout_retry_in_game();

	/* Held on the charger, and while a long task runs behind the game: the
	 * same answer every screen gets (idle_held; 2026-11-11: Auto Off
	 * powered the Pixel down in the middle of an update) */
	if (g_idle_secs > 0) {
		bool ch = idle_held();

		if (ch != g_idle_charging) {
			g_idle_charging = ch;
			plat_resident_line("SETIDLE\tms=%d",
			                   ch ? 0 : g_idle_secs * 1000);
		}
	}

	/* The account's answer, as soon as it lands - see sync_poll.
	 *
	 * When it adds achievements this device did not know were earned, the list
	 * Diatom is watching still has them in it, and one of them firing reads as
	 * a fresh unlock. So the list is handed over again without them - but only
	 * early in a session. Reloading a set starts every partly-met achievement
	 * over (Diatom's diatom_cheevos_load re-initializes the runtime), and
	 * WATCH_RETRIM_MS is ten times the measured answer and still the opening of
	 * a session. Later than that the count is still put right; a repeat unlock
	 * is sent and answered as already held.
	 *
	 * Before the return below, which is about a set still being fetched: the
	 * two never share the slot, because this game's question is only asked
	 * once its set has arrived. */
	if (sync_poll() > 0 && g_watch_ms &&
	    plat_now_ms() - g_watch_ms <= WATCH_RETRIM_MS) {
		if (chv_write_active(g_watch_active))
			plat_resident_line("SETCHEEVOS\tpath=%s\tconsole=%d",
			                   g_watch_active, chv_console());
		else                           /* all of it held: watch nothing */
			plat_resident_line("SETCHEEVOS\tpath=\tconsole=%d", chv_console());
		g_watch_ms = 0;
		fprintf(stderr, "cheevos: the account held some already; watching the rest\n");
	}

	/* A first play's hash, made on its own thread (src/rahashjob.h): the
	 * set is asked for when it lands, for the set the job was started for.
	 * No hash: the file could not be read, or an .m3u's first disc has no
	 * reader, and its rows say "none" (chv_none_mark), as before. */
	if (g_pending_hashing) {
		char h[33], set[sizeof g_pending_set];
		int r = rhj_poll(h, set, sizeof set);

		if (r == 0) return;
		g_pending_hashing = false;
		if (r == 1) {
			snprintf(g_pending_set, sizeof g_pending_set, "%s", set);
			ra_fetch_begin(h, g_pending_set);
		} else {
			if (r == -1) chv_none_mark(set, true);
			g_pending_set[0] = '\0';
		}
		return;
	}
	if (!g_pending_set[0]) return;

	/* Once. Calling it twice advances the state machine twice, which is a
	 * lookup answered and then thrown away. */
	rc = ra_fetch_step();
	if (rc == 0) return;                        /* still working */
	/* RA has nothing for this ROM: said so on its Achievements rows, "none"
	 * rather than "not checked yet" (chv_state_of). A fetch that failed
	 * says nothing, and the next launch with a network asks again. */
	if (rc == RA_FETCH_NOTHING) chv_none_mark(g_pending_set, true);
	if (rc < 0) { g_pending_set[0] = '\0'; return; }
	chv_none_mark(g_pending_set, false);        /* a set now: it has one */

	/* A FETCH CAN OUTLIVE THE GAME IT WAS FOR, and handing over the wrong set
	 * is not a cosmetic mistake. This runs on the game tick, so the game that
	 * started the fetch may have ended and another may be running - and below,
	 * SETCHEEVOS hands the set to Diatom mid-game, which its protocol makes
	 * the normal path precisely because a download can still be in flight.
	 *
	 * Nothing downstream would catch it. rcheevos would evaluate one game's
	 * conditions against another game's memory, and a condition that matched
	 * by coincidence would unlock an achievement the player never earned, in a
	 * game they were not playing, and on_cheevo_unlocked would queue it for
	 * submission to their account.
	 *
	 * Seen 2026-09-10: launching Advance Guardian Heroes drew the set notice
	 * for A Boy and His Blob, an NES game fetched earlier the same evening.
	 * The active set was not poisoned that time, which is luck, not a guard.
	 *
	 * Dropped rather than canceled: there is no ra_fetch_abort, and
	 * ra_fetch_step only runs while g_pending_set is set, so clearing it on
	 * game end - the fix this looked like it wanted - would abandon a live
	 * state machine with nobody to drive it. The fetch has already written the
	 * set to its own cache by now, so the next launch of THAT game finds it
	 * waiting. A stale fetch becomes a cache warm instead of a hazard. */
	if (strcmp(g_pending_set, g_game_set) != 0) {
		fprintf(stderr, "cheevos: set arrived for a game that is no longer "
		                "running; cached, not applied\n");
		g_pending_set[0] = '\0';
		return;
	}

	if (!chv_load(g_pending_set)) { g_pending_set[0] = '\0'; return; }
	sync_begin(chv_game(), g_pending_set);   /* read on a later tick - sync_poll */

	if (chv_write_active(g_pending_active)) {
		plat_resident_line("SETCHEEVOS	path=%s	console=%d",
		                   g_pending_active, chv_console());
		g_watch_ms = plat_now_ms();
		snprintf(g_watch_active, sizeof g_watch_active, "%s", g_pending_active);
		snprintf(msg, sizeof msg, T(S_CHEEVOS_TO_EARN), chv_count());
	} else {
		snprintf(msg, sizeof msg, T(S_CHEEVOS_ALL_EARNED), chv_count());
	}

	/* Rendering is safe here; presenting would not be. Same rule as an
	 * unlock notice, and the same queue. */
	notice_queue(NQ_SET, 0, 3500, msg, chv_game_title(), NULL, 0, 0);
	g_pending_set[0] = '\0';
}

/* ---------- the pop-ups over a game ------------------------------------- */

/* Rendered here, shown by Diatom (src/notice.h), one after another
 * (src/noticeq.h): Diatom holds one overlay and a second replaced the first.
 * The file is in /tmp, which is memory: a progress pop-up comes every few
 * seconds while a count moves, and each was a write to the card. */
#define NOTICE_FILE "/tmp/zyperos-notice.dtov"

noticeq g_nq;

/* What a notice carries besides its words (src/notice.h), by what it is
 * about: an unlock, and mastery, which is one, has its badge and Zyper; a
 * count moving, its badge; the set's arrival has neither. Where the card has
 * no art for a badge the plain one takes its place (src/noticeart.h). */
static unsigned notice_look(nq_kind kind)
{
	switch (kind) {
	/* Zyper at its end unless the look has him off (DESIGN.md 3.5) */
	case NQ_UNLOCK:   return NOTICE_ICON | (look_zyper_off() ? 0 : NOTICE_ZYPER);
	case NQ_PROGRESS: return NOTICE_ICON;
	default:          return 0;
	}
}

/* The next one, if the one on screen has had its time. From the game tick,
 * and as soon as one is queued. */
static void notice_pump(void)
{
	nq_item it;

	if (!nq_next(&g_nq, plat_now_ms(), &it)) return;
	if (it.mark ? notice_render_mark(it.head, it.body, (notice_mark)it.mark,
	                                 NOTICE_FILE, theme_index())
	            : notice_render_ex(it.head, it.body, it.badge[0] ? it.badge : NULL,
	                               notice_look(it.kind), it.value, it.target,
	                               NOTICE_FILE, theme_index()))
		plat_resident_line("OVERLAY\tpath=%s\tms=%d", NOTICE_FILE, it.ms);
}

/* "Getting hot" over a game (DESIGN.md 11.23): the notice with a thermometer
 * where a badge goes, held **hold-heat**, ahead of every pop-up waiting.
 * Zyper is over a game only in an unlock's notice (DESIGN.md 10), not here. */
static void heat_notice(void)
{
	nq_item it;

	heat_said("over a game");
	memset(&it, 0, sizeof it);
	it.kind = NQ_ALERT;
	it.ms = DS_MS_HOLD_HEAT;
	it.mark = NOTICE_MARK_HEAT;
	snprintf(it.head, sizeof it.head, "%s", HEAT_HEAD);
	snprintf(it.body, sizeof it.body, "%s", HEAT_BODY);
	nq_push(&g_nq, &it);
	notice_pump();
}

void notice_queue(nq_kind kind, int id, int ms, const char *head,
                  const char *body, const char *badge, int value, int target)
{
	nq_item it;

	memset(&it, 0, sizeof it);
	it.kind = kind;
	it.id = id;
	it.ms = ms;
	it.value = value;
	it.target = target;
	snprintf(it.head, sizeof it.head, "%s", head ? head : "");
	snprintf(it.body, sizeof it.body, "%s", body ? body : "");
	if (badge && access(badge, R_OK) == 0) snprintf(it.badge, sizeof it.badge, "%s", badge);
	nq_push(&g_nq, &it);
	notice_pump();
}

/* Called from inside plat_resident_wait, mid-game, when the launcher owns
 * nothing and can draw nothing. Recording is all that happens here; the
 * telling happens the next time this process has the screen. */
static void on_cheevo_unlocked(int id)
{
	char p[CFG_STR * 2];
	const cheevo *c = NULL;
	int i;

	/* Recorded with when it was earned, in which mode (softcore, the only
	 * one), with the ROM's hash from the set (chv_note_unlock_at), and
	 * whether there was a network: an unlock earned offline is shown here
	 * the same as any other, and sent when one is back (ra_flush_watch). */
	if (!chv_note_unlock_at(id, (long)time(NULL), false, !net_online())) return;

	/* Written through immediately rather than at exit. A game ends by power
	 * button as often as by menu, and an achievement lost to a flat battery is
	 * one the player has to earn twice. */
	chv_earned_save();

	/* And say so, on screen, now. This process owns no display while a game
	 * runs, so it renders the line and Diatom composites it.
	 * Rendering is safe here; presenting would not be. */
	for (i = 0; i < chv_count(); i++)
		if (chv_at(i)->id == id) { c = chv_at(i); break; }

	{
		char head[64];

		/* Points on the context line, not the name: "First Blood!" is what
		 * the player wants to read, and "1 point" is what it was worth. */
		if (c && c->points > 0)
			snprintf(head, sizeof head, TN(S_CHEEVOS_UNLOCKED_POINTS, c->points),
			         c->points);
		else
			snprintf(head, sizeof head, "%s", T(S_CHEEVOS_UNLOCKED));

		/* With its badge, when the card has it (src/rabadge.h) */
		p[0] = '\0';
		if (c) rb_path(chv_source(), c->badge, p, sizeof p);
		notice_queue(NQ_UNLOCK, id, 4000, head,
		             c && c->title[0] ? c->title : T(S_CHEEVOS_UNLOCKED), p[0] ? p : NULL, 0, 0);
	}
	/* The last one of the set: mastered, said after it */
	if (chv_count() > 0 && chv_earned() == chv_count()) {
		char head[64];

		snprintf(head, sizeof head, T(S_CHEEVOS_MASTERED), chv_points_total());
		notice_queue(NQ_UNLOCK, 0, 4000, head, chv_game_title(), NULL, 0, 0);
	}
}

/* A count moving, from Diatom (state=progress): kept on the achievement for
 * the list, and said over the game - the name, a strip and "74 / 100" - when
 * its turn in the queue comes. Diatom already holds these to one every two
 * seconds per achievement. */
static void on_cheevo_progress(int id, const char *text, int value, int target)
{
	char p[CFG_STR * 2] = "", count[32];
	const cheevo *c = NULL;
	int i;

	if (!chv_note_progress(id, text)) return;
	for (i = 0; i < chv_count(); i++)
		if (chv_at(i)->id == id) { c = chv_at(i); break; }
	if (!c || c->earned || c->earned_now) return;
	/* "74/100" as rcheevos writes it, spaced the way the list writes counts */
	if (strchr(text, '/') && !strchr(text, ' '))
		snprintf(count, sizeof count, "%.*s / %s", (int)(strchr(text, '/') - text),
		         text, strchr(text, '/') + 1);
	else
		snprintf(count, sizeof count, "%s", text);
	rb_path(chv_source(), c->badge, p, sizeof p);
	notice_queue(NQ_PROGRESS, id, 2500, c->title[0] ? c->title : T(S_CHEEVOS_PROGRESS), count,
	             p[0] ? p : NULL, value, target);
}

/* A pop-up that answers a press over the game: rendered here, composited by
 * Diatom, as an unlock notice is, and shorter, since it says a word or two.
 * Past the queue, at once, and the queue waits for it to go. */
static void popup_now(const char *head, const char *body)
{
	char p[CFG_STR * 2];

	snprintf(p, sizeof p, "%s", NOTICE_FILE);
	if (notice_render(head, body, p, theme_index()))
		plat_resident_line("OVERLAY\tpath=%s\tms=1500", p);
	nq_hold(&g_nq, plat_now_ms(), 1500);
}

/* Called from inside plat_resident_wait when Diatom says the speed step
 * changed (SELECT+R2, SELECT+L2), or that a game resumed fast: in halves, 3
 * is 1.5x. The step, not the speed reached: on a heavy SNES or GBA game the
 * top step runs about 2.5x, and that figure moves from scene to scene. */
static void on_speed(int half)
{
	char body[24];

	if (half % 2) snprintf(body, sizeof body, T(S_POPUP_SPEED_HALF), half / 2);
	else          snprintf(body, sizeof body, T(S_POPUP_SPEED_WHOLE), half / 2);
	popup_now(T(S_POPUP_SPEED), body);
}

/* Called from inside plat_resident_wait when R2 turned a PSP game's d-pad
 * into the analog stick (on) or back: said over the game, and kept for the
 * game (rms_stick_put). The answers to the Controls tab's own SETSTICKs
 * never come here (plat_resident_set_stick): its row said it already. */
static void on_stick(bool on)
{
	popup_now(T(S_POPUP_STICK), T(on ? S_POPUP_STICK_ON : S_POPUP_STICK_OFF));
	if (g_quick.tag[0] && g_quick.file[0]) rms_stick_put(&g_quick.sys, g_quick.file, on);
}

/* Called from inside plat_resident_wait for the quick slot (platform.h's
 * PLAT_QUICK_*). SELECT+R1 and SELECT+L1 ask, and the slot is this
 * process's to know (quick_slot_get), so the SAVE or LOAD goes back to
 * Diatom from here, a save with its picture for the slot strip, and the
 * pop-up says the slot when the answer comes. A slot with nothing in it is
 * said at once, with no LOAD: the strip says "Empty" for the same file. */
static void on_quick(int what, const char *path)
{
	char slot[32];

	if (what == PLAT_QUICK_ASK_SAVE || what == PLAT_QUICK_ASK_LOAD) {
		bool load = what == PLAT_QUICK_ASK_LOAD;
		int n;

		if (!g_quick.stem[0]) return;
		if (!g_quick.states) {
			popup_now(T(S_POPUP_NO_STATES), g_quick.name);
			return;
		}
		n = quick_slot_get(g_quick.tag);
		snprintf(slot, sizeof slot, T(S_SLOTS_SLOT), n);
		snprintf(g_quick.path, sizeof g_quick.path, "%s.%d.state", g_quick.stem, n);
		if (load && access(g_quick.path, R_OK) != 0) {
			popup_now(T(S_POPUP_SLOT_EMPTY), slot);
			return;
		}
		g_quick.asked = n;
		g_quick.load = load;
		if (load)
			plat_resident_line("LOAD\tpath=%s", g_quick.path);
		else
			plat_resident_line("SAVE\tpath=%s\tpreview=%s.%d.bmp", g_quick.path,
			                   g_quick.stem, n);
		return;
	}
	/* An answer to what the hotkeys asked; the menu's own are not said */
	if (!g_quick.asked || !path || strcmp(path, g_quick.path)) return;
	snprintf(slot, sizeof slot, T(S_SLOTS_SLOT), g_quick.asked);
	g_quick.asked = 0;
	switch (what) {
	case PLAT_QUICK_SAVED:       popup_now(T(S_POPUP_SAVED), slot); break;
	case PLAT_QUICK_LOADED:      popup_now(T(S_POPUP_LOADED), slot); break;
	case PLAT_QUICK_SAVE_FAILED: popup_now(T(S_POPUP_SAVE_FAILED), slot); break;
	default:                     popup_now(T(S_POPUP_LOAD_FAILED), slot); break;
	}
}

static unsigned g_wifi_asked_at;   /* when menu_wifi last forked, plat_now_ms */

/* ---------- the status corner ---------------------------------------------- */

/* MENU > Clock: the time in 24 hours, on the shelf's corner and in Date &
 * Time's row; 12-hour, the default, is what the launcher always was. Saved in
 * the device's database as Appearance is, under `clock`, "12h" or "24h". */
bool g_clock24;

static void clock24_load(void)
{
	char v[8];

	db_get_str(db_dev(), "clock", v, sizeof v, "12h");
	g_clock24 = !strcmp(v, "24h");
}

void clock24_save(bool on)
{
	g_clock24 = on;
	db_set_str(db_dev(), "clock", on ? "24h" : "12h");
}

/* MENU > USB Accessories on the Pixel 2: Ask, the default, or Automatic,
 * saved as "ask" or "auto" under `usb_accessories` and told to usbrole
 * (plat_usb_power_mode, src/usbpower.h) as the launcher starts and as it
 * changes */
bool g_usb_auto;
/* A plug that waits for an answer now (it may be asked about, usbrole
 * powers nothing by itself): the corner's USB mark and MENU > USB Device,
 * which asks again for it. 0 none. */
unsigned g_usb_wait;
/* --usb-menu: MENU's USB rows on a host shot, which has no usbrole */
bool shot_usb_menu;
static bool shot_usb_said;     /* --usb-menu said: Automatic's message */
static bool shot_usb_offer;    /* --usb-offer: Zyper's offer of Automatic */

/* An accessory powered when the handheld turned off, kept on the card for
 * the next start (usbpower_resume): read once there and gone */
bool g_usb_kept;

static const char *usb_kept_path(void)
{
	static char p[512];

	snprintf(p, sizeof p, "%s/usb-kept", P_USERDATA);
	return p;
}

static void usb_auto_load(void)
{
	char v[8];

	db_get_str(db_dev(), "usb_accessories", v, sizeof v, "ask");
	g_usb_auto = !strcmp(v, "auto");
	plat_usb_power_mode(g_usb_auto);
	/* Back from USB Drive with the cable in: that plug was answered */
	if (access("/tmp/zyperos_usbdrive_back", F_OK) == 0) {
		remove("/tmp/zyperos_usbdrive_back");
		usblink_skip_ask();
		fprintf(stderr, "usb: back from USB Drive: not asked again for this plug\n");
	}
	/* The brightness set on Flash Drive's page (usbdrive), kept as if set
	 * here: the same rungs, saved as the launcher saves its own */
	{
		FILE *f = fopen("/tmp/zyperos_usbdrive_bright", "r");
		int level;

		if (f) {
			if (fscanf(f, "%d", &level) == 1 && level >= 0 && level <= PLAT_BRIGHT_MAX) {
				plat_brightness_set(level);
				fprintf(stderr, "usb: the brightness set on Flash Drive's page: %d\n", level);
			}
			fclose(f);
			remove("/tmp/zyperos_usbdrive_bright");
		}
	}
	g_usb_kept = access(usb_kept_path(), F_OK) == 0;
	if (g_usb_kept) {
		remove(usb_kept_path());
		fprintf(stderr, "usb: an accessory was powered at the last power-off\n");
	}
}

/* Turning off: an accessory powered now (the port a host, no computer
 * seen) is kept for the next start */
void usb_kept_save(void)
{
	usb_power p;
	FILE *f;

	remove(usb_kept_path());
	if (!plat_usb_power(&p) || !p.host || p.computer) return;
	if ((f = fopen(usb_kept_path(), "w"))) {
		fprintf(f, "accessory plug %u\n", p.plug);
		fclose(f);
	}
}

void usb_auto_save(bool on)
{
	g_usb_auto = on;
	db_set_str(db_dev(), "usb_accessories", on ? "auto" : "ask");
	plat_usb_power_mode(on);
}

/* Zyper's offer of Automatic (usbpower_offer_after): the player's USB
 * Accessory answers counted under `usb_accessory_answers`, the offer's
 * answer kept under `usb_auto_offer` ("auto" or "ask"), and the offer due,
 * for the shelf to show once the question is over (usb_offer_show) */
bool g_usb_offer_due;

void usb_offer_count(void)
{
	int got = db_get_int(db_dev(), "usb_accessory_answers", 0);
	unsigned was = got > 0 ? (unsigned)got : 0, n;

	g_usb_offer_due = usbpower_offer_after(was, g_usb_auto, db_has(db_dev(), "usb_auto_offer"), &n);
	if (n != was) db_set_int(db_dev(), "usb_accessory_answers", (int)n);
}

const char *const BATT_IDS[BATT_MODES] = { "bars", "percentage", "time" };
int g_batt_mode;

static void batt_icon_load(void)
{
	char v[16];
	int k;

	db_get_str(db_dev(), "battery_icon", v, sizeof v, "bars");
	g_batt_mode = BATT_BARS;
	for (k = 0; k < BATT_MODES; k++)
		if (!strcmp(v, BATT_IDS[k])) g_batt_mode = k;
}

/* --status, for a shot: what the corner shows in place of the device's
 * clock, radio and battery, which a host does not have. Set by main's
 * argument loop, and only for a shot, as --theme is; `minutes` is
 * the time of day, -1 for a clock never set. `shot` is that a shot is being
 * taken at all: then the clock is --status's or none, since a shot that
 * showed the host's would change from one minute to the next, and shots are
 * compared byte for byte (check-flip). */
shot_status shot_st = { .minutes = -1, .h24 = -1, .eta = -1, .calc = -1, .icon = -1 };

/* Where the corner (its clock's text box and its marks), the clock alone, the
 * last panel and the games title's capitals were drawn this frame, for a
 * shot to hold the corner clear of them (shot_corner_clear). Zero width: not
 * drawn. */
SDL_Rect g_corner_box, g_clock_box, g_menu_panel, g_title_caps;
/* The last menu panel's body: below its heading, inside its pad, for what a
 * screen draws in it beside the rows (About's photo of Zyper) */
SDL_Rect g_menu_body;
static int      g_marks_w;   /* the marks' width as status_marks_width gave it */
/* The battery's number or time as wide as it wrote, before STATUS_PCT_MAX
 * held it to its room: a shot checks it fits (shot_corner_clear) */
static int      g_pct_natural;

/* The screen was cleared: nothing recorded above is on it any more. */
void drawn_forget(void)
{
	g_corner_box = g_clock_box = g_menu_panel = g_title_caps = (SDL_Rect){ 0 };
	g_marks_w = 0;
	g_pct_natural = 0;
}

/* The critical blink, a second on and a second off, slow enough not to pull
 * at the eye: the fill goes in the second off, the outline stays. Returns
 * how long until it changes by itself, in ms, and tells the shelf's loop the
 * same, so a screen that only draws when something moves still blinks it.
 * Diatom blinks its disc over a game on the same rhythm. */
static unsigned status_blink(status_marks *m)
{
	Uint32 now, flip;

	if (m->batt < 0 || status_batt_level(m->batt, m->charging) < 2) return (unsigned)-1;
	now = plat_now_ms();
	flip = 1000 - now % 1000;
	redraw_at(now + flip);
	m->blink_off = (now / 1000) % 2 != 0;
	if (shot_st.set) m->blink_off = shot_st.m.blink_off;   /* a still, asked for */
	return flip;
}

/* Wi-Fi's signal in steps, from `wpa_cli signal_poll`, every two seconds
 * while connected, as the state is. A second fork, my call: neither
 * kernel fills /proc/net/wireless, which would have cost none (cfg80211's
 * wireless extensions are off in plastron's Pixel 2 config and unset in the
 * Brick's). Not on a frame that menu_wifi has just forked on, so the two
 * forks never stack into one 60 ms frame; and, as the state, not asked while
 * the shelf moves (the caller's). A driver that cannot say shows full: a
 * connection is true, a guess at its strength is not. */
static int status_signal(void)
{
	static unsigned next;
	static int steps = STATUS_WIFI_FULL;
	unsigned now = plat_now_ms();

	if ((next == 0 || (int)(now - next) >= 0) && now - g_wifi_asked_at >= 500) {
		char out[512];
		int dbm;

		next = now + 2000;
		steps = wifi_signal_poll(out, sizeof out) && status_rssi_dbm(out, &dbm)
		      ? status_wifi_steps(dbm) : STATUS_WIFI_FULL;
	}
	return steps;
}

/* What the marks show now. Wi-Fi from the menu's two-second cache, but not
 * asked while the shelf moves: the ask forks wpa_cli, about 30 ms out of
 * this 119 MB process, which mid-move is a frame dropped, so the mark keeps
 * what it last knew until the row rests. A headset from the file
 * launch.sh writes (aout_bt_sink), which costs no fork and is up to twenty
 * seconds late, which a mark can afford. The battery from battery_now. */
/* On the cable and held off at the charge limit: the corner's plug. The
 * gauge read every 2 s while the cable is in and a limit is set. */
static bool charge_held(void)
{
	static unsigned at;
	static bool primed, held;
	unsigned now = plat_now_ms();
	ch_reading r;

	if (!g_charge_limit) return false;
	if (!primed || now - at >= 2000) {
		primed = true;
		at = now;
		held = plat_charge_read(&r) && r.held;
	}
	return held;
}

/* What MENU > Battery Icon > Time Left writes in the corner: the kind of it
 * (src/battery_eta.h's batt_corner_kind), the minutes when it is a time, and
 * how long the dots have been going, for the animation's tick. */
typedef struct {
	corner_kind kind;
	int         minutes;
	unsigned    calc_ms;
} corner_view;

/* The gauge's files, read every 2 s (plat_charge_read: the charging screen's
 * own, so the one gauge is read the one way) and kept between. On the cable
 * the time is to full, or to the charge limit when one is set, and is two
 * estimates blended: the straight line from the gauge's first read, and the
 * charging screen's (src/charging.h: CC then CV, after its 30 s settle) as it
 * settles. Off the cable, the gauge's charge over its current from the first
 * read, blended into the falls' as that earns trust (battery_minutes_left).
 * A gauge with no current file (the Brick, a simulator) gives the gauge's
 * half nothing, and the falls carry on alone; on the cable it gives the
 * percentage, since nothing can ever come. */
static corner_view corner_view_now(const status_marks *m)
{
	static unsigned at, cable_since, calc_since;
	static bool primed, was, ok, calcing;
	static ch_reading r;
	static ch_eta eta;
	unsigned now = plat_now_ms();
	corner_in in = { .charging = m->charging, .minutes = -1 };
	corner_view v;

	if (shot_st.set) {
		/* A shot's own: eta= the minutes, calc[=N] the dots, full the charge */
		in.full = shot_st.full;
		in.held = m->held;
		in.minutes = shot_st.eta;
		in.expect = shot_st.calc >= 0;
		v.kind = m->batt < 0 ? CORNER_PCT : batt_corner_kind(&in);
		v.minutes = in.minutes;
		v.calc_ms = 0;
		return v;
	}
	if (m->batt < 0) return (corner_view){ CORNER_PCT, -1, 0 };
	if (!primed || m->charging != was) {
		/* The cable came or went: what was learned of the other is over */
		was = m->charging;
		eta = (ch_eta){ 0 };
		cable_since = now;
		primed = false;
	}
	if (!primed || now - at >= 2000) {
		primed = true;
		at = now;
		ok = plat_charge_read(&r);
		if (ok) {
			r.limit = charge_limit_now();
			if (m->charging) ch_eta_note(&eta, now, &r);
		}
	}
	if (m->charging) {
		if (ok && r.online) {
			int quick = batt_gauge_to_full(r.now_mah, r.full_mah, r.limit, r.ma);
			int slow = ch_eta_minutes(&eta, now, &r);

			in.full = r.full || r.pct >= 100;
			in.held = r.held;
			in.minutes = batt_eta_blend(quick > 0 ? quick : -1, slow > 0 ? slow : -1,
			                            batt_ramp(now - eta.first_at, CH_SETTLE_MS, CH_SETTLE_MS));
			/* Dots for the first CH_SETTLE_MS when a current can be read
			 * and none is going in yet; the percentage when none can be */
			in.expect = r.ma_known && now - cable_since < CH_SETTLE_MS;
		}
	} else {
		int quick = ok && !r.online
		          ? batt_gauge_left(r.now_mah, r.full_mah, r.ma, BATT_EMPTY_PCT) : -1;

		in.minutes = battery_minutes_left_with(quick);
		in.expect = m->batt > BATT_EMPTY_PCT;
	}
	v.kind = batt_corner_kind(&in);
	v.minutes = in.minutes;
	if (v.kind != CORNER_CALC) calcing = false;
	else if (!calcing) { calcing = true; calc_since = now; }
	v.calc_ms = calcing ? now - calc_since : 0;
	return v;
}

/* "45 min", "3 h", "3 h 20 min": to the minute under 10, to 5 under 10
 * hours and to the hour after (batt_eta_round), so a number changes when it
 * means something */
static void corner_time_text(int minutes, char *out, size_t n)
{
	int r = batt_eta_round(minutes);

	if (r < 60)            snprintf(out, n, T(S_CORNER_TIME_MIN), r);
	else if (r % 60 == 0)  snprintf(out, n, T(S_CORNER_TIME_H), r / 60);
	else                   snprintf(out, n, T(S_CORNER_TIME_H_MIN), r / 60, r % 60);
}

/* System Update's page is up: the corner's update mark stays off there */
bool g_up_shown;

static void status_gather(status_marks *m, bool moving)
{
	static int wifi = STATUS_WIFI_NONE;
	int pct;
	bool chg;

	if (shot_st.set) {
		*m = shot_st.m;
		return;
	}
	memset(m, 0, sizeof *m);
	if (!plat_has_wifi()) {
		wifi = STATUS_WIFI_NONE;
	} else if (!moving) {
		wifi_state ws = menu_wifi(NULL, 0);

		wifi = ws == WIFI_OFF       ? STATUS_WIFI_NONE
		     : ws == WIFI_CONNECTED ? status_signal() : STATUS_WIFI_IDLE;
	}
	m->wifi = wifi;
	m->bt = plat_has_bluetooth() && aout_bt_sink()[0];
	m->phones = plat_headphones_present();
	m->batt = battery_now(&pct, &chg) ? pct : -1;
	m->charging = chg;
	m->held = chg && charge_held();
	xs_mark(m);
	m->usb = g_usb_wait != 0;
	/* System Update at work behind the screen, and how far its download is:
	 * not on its own page, which shows it whole */
	if (!g_up_shown && up_working()) {
		const su_view *v = up_view(false);

		m->update = true;
		m->update_pct = v->st.total ? (int)(v->st.done * 100 / v->st.total) : 0;
	}
}

/* The time a shot shows: today at --status's time of day, into *t. False for
 * a shot with no time to show - no --status, or one with no time - which
 * then shows none, since the host's would change the shot every minute. Not
 * a shot: *t as it is, and true. */
bool shot_time(time_t *t)
{
	struct tm tm;

	if (!shot_st.shot) return true;
	if (!shot_st.set || shot_st.minutes < 0) return false;
	localtime_r(t, &tm);
	tm.tm_hour = shot_st.minutes / 60;
	tm.tm_min = shot_st.minutes % 60;
	tm.tm_sec = 0;
	tm.tm_isdst = -1;
	*t = mktime(&tm);
	return true;
}

/* The corner's clock at `t`, "10:08", or false while the clock is one nobody
 * set - and in a shot, unless --status gives a time. */
bool status_clock(time_t t, char *out, size_t n)
{
	if (!shot_time(&t)) return false;
	if (!clock_is_set(t)) return false;
	clock_short(t, g_clock24, out, n);
	return true;
}

/* The top bar on this screen, src/topbar.h: the one header, in one layout,
 * on every screen that has one - the home, the shelves and the menus over
 * them, every screen over a paused game, every MP4 screen. This half gathers
 * what it shows - the radios and the battery (status_gather), the clock in
 * its setting, the battery's words as MENU > Battery Icon says - and keeps
 * the frames coming that make it move: the blink, the next minute, Time
 * Left's minutes and its dots. In `theme`, an index - DS_THEME is a table per
 * file, so theme()'s pointer is not into this one - with `soft` the battery's
 * words' colour: **text-soft**, but **text** over a paused game, where a white
 * frame under scrim.game leaves text-soft at 4.4:1 and text holds 6.0:1.
 * Returns how long until it changes by itself, in ms, and tells the shelf's
 * loop the same. */
static topbar_drawn g_topbar;

unsigned draw_topbar(SDL_Renderer *r, int theme, SDL_Color soft, bool moving)
{
	topbar t = { .theme = theme, .soft = soft };
	struct timespec ts;
	char clk[16], pct[24] = "";
	unsigned wait;

	/* One reading of the clock for what it says and when it next changes */
	clock_gettime(CLOCK_REALTIME, &ts);
	status_gather(&t.marks, moving);
	wait = status_blink(&t.marks);
	/* MENU > Battery Icon > Percentage: "82%". Time Left writes "3 h 20 min"
	 * when it has one, else the same number, and asks for a frame every
	 * minute while it does, as the clock does, so the time counts down. */
	if (g_batt_mode != BATT_BARS && t.marks.batt >= 0) {
		corner_view cv = g_batt_mode == BATT_TIME ? corner_view_now(&t.marks)
		                                          : (corner_view){ CORNER_PCT, -1, 0 };

		if (cv.kind == CORNER_TIME) {
			unsigned minute = 60000;

			corner_time_text(cv.minutes, pct, sizeof pct);
			redraw_at(plat_now_ms() + minute);
			if (minute < wait) wait = minute;
		} else if (cv.kind == CORNER_HELD) {
			snprintf(pct, sizeof pct, "%s", T(S_CORNER_HELD));
		} else if (cv.kind == CORNER_FULL) {
			snprintf(pct, sizeof pct, "%s", T(S_CORNER_FULL));
		} else if (cv.kind == CORNER_CALC) {
			/* Three dots filling in turn where the time will go: a frame
			 * every CORNER_DOT_MS for the first minute of it, then only as
			 * the launcher draws anyway, so a gauge that never answers
			 * does not keep the screen awake */
			t.marks.calc = true;
			t.marks.calc_step = shot_st.set ? shot_st.calc : batt_corner_dots(plat_now_ms());
			if (!shot_st.set && cv.calc_ms < 60000u) {
				redraw_at(plat_now_ms() + CORNER_DOT_MS);
				if (CORNER_DOT_MS < wait) wait = CORNER_DOT_MS;
			}
		} else {
			snprintf(pct, sizeof pct, "%d%%", t.marks.batt > 100 ? 100 : t.marks.batt);
		}
		t.batt = pct;
	}
	if (status_clock(ts.tv_sec, clk, sizeof clk)) {
		/* The next minute. The shelf's one-second heartbeat redraws it
		 * anyway; this is the clock saying when it changes, as every
		 * other thing that moves by itself does. */
		unsigned left = 60000 - (unsigned)((ts.tv_sec % 60) * 1000 + ts.tv_nsec / 1000000);

		t.clock = clk;
		redraw_at(plat_now_ms() + left);
		if (left < wait) wait = left;
	}
	topbar_draw(r, &t, &g_topbar);
	g_corner_box = g_topbar.marks;
	g_clock_box = g_topbar.clock;
	g_marks_w = g_topbar.marks_w;
	g_pct_natural = g_topbar.batt_natural;
	return wait;
}

/* MP4's own shelf of albums. It has the home's header since 2026-11-09 (the
 * clock at the left, the rest at the right; draw_status), as every MP4 screen
 * has, but not the home's song line in it: MP4 is what plays there. */
bool muse_shelf_shown(const app *a)
{
	return a->sys.count > 0 && a->screen == SCREEN_GAMES &&
	       is_muse(&a->sys.systems[a->sys_cursor]);
}

void draw_status(app *a)
{
	/* Moving is any frame asked for within 50 ms, this frame or the last:
	 * a card's step, a marquee's travel, the tint easing to a new system, a
	 * menu over the shelf. A fork then would stop a frame of it. */
	bool moving = home_moving(&a->home, plat_now_ms()) || frames_wanted() ||
	              (a->sys.count > 0 && a->view[a->sys_cursor].cf.active);

	draw_topbar(a->r, theme_index(), UI_TEXT_SOFT, moving);
}

/* topbar_half, as the bar was drawn last: MP4's album title on its shelf,
 * and a video's title over its frame, stand in its row */
int header_half(void)
{
	return topbar_half(&g_topbar);
}


/* Auto's clock, asked by a screen's own loop once a pass: true when the theme
 * changed, and then theme_changed has been told. Every loop that stays on
 * screen asks it - the menus through menu_run_body, the shelf's loop for
 * itself, and every screen that runs its own - so 7:00 or 19:00 arriving
 * while one is up is drawn from its next frame, and the file Diatom and
 * portkeys read follows. A loop that draws every pass needs nothing more; one
 * that draws only when something changed takes a true as a change. A screen
 * drawn once and held, a one-line message, asks before it is drawn.
 *
 * The one way in for the clock: a turn on_game_tick saw during a game
 * (g_theme_late) is a change here like the clock's own, so the in-game menu
 * and Muse opened from it draw their cards in the theme that is on, and the
 * way back from the game asks this before its first frame. */
/* System Update's word that this version started (docs/updates.md): the
 * switch at boot (the image's update-apply.sh) puts a new ZyperOS folder on
 * trial, and a third start without this word goes back to the version
 * before by itself. The word is ten seconds of a launcher whose screens are
 * running, asked from every screen's loop through tick_theme below. */





/* A focus frame: on a still Spotlight home, the focused machine's next atlas
 * frame drawn over the picture the last whole frame left in the screen's
 * texture - ground laid where any of its frames can reach, the sprite, and
 * the row's sprites it reaches drawn again over it - and presented. Nothing
 * else on the home is drawn, and none of it changes: tools/home-shot-check.py
 * has a focus frame and a whole one of the same moment byte for byte the same,
 * for every tile of every system's animation.
 *
 * The sprite moves at about 11 frames a second for as long as the home is
 * up, and a whole frame is some 40 draws the GL driver takes 4.4 ms of CPU to
 * run on the Pixel 2: 6% of a core for a machine that waits (2026-11-07).
 * Not while anything that is not redrawn here is on screen: the song or heat
 * pop-up, the volume bar. A whole frame still comes at least once a second
 * (HEARTBEAT_MS counts whole frames only). */

/* ---- the panel pass, and the panel on screen ---------------------------
 *
 * DESIGN.md 8.2: a panel arrives from 96% and clear and leaves the same way;
 * src/menumotion.h says when. To scale and fade as one - its text, its
 * plate, Zyper and his shadow - the panel is drawn into a texture the size of
 * the screen, at the place it is drawn at rest, and copied back about its
 * middle at the size and strength the motion says. Its lift goes straight on
 * the screen under it (menu_panel). At rest none of this runs, so a panel at
 * rest, and every shot of one, is the picture it always was. The texture
 * is g_panel_tex. */

/* `q` at `s` of its size about its middle */
SDL_FRect panel_scaled(const SDL_Rect *q, float s)
{
	float w = (float)q->w * s, h = (float)q->h * s;

	return (SDL_FRect){ (float)q->x + ((float)q->w - w) / 2.0f,
	                    (float)q->y + ((float)q->h - h) / 2.0f, w, h };
}

/* Drawing goes to the pass's texture, cleared to the frame's own **edge** at
 * no alpha: the copy's linear filtering then blends the frame into nothing
 * at its border, where a clear black would draw it a dark rim. False, and
 * drawing stays on the screen, when there is no texture to draw into; the
 * panel is then drawn whole where it would have faded, a cut and no worse. */
bool panel_pass_begin(app *a)
{
	const ds_rgb e = theme()->edge;

	static bool said;

	if (!g_panel_tex) {
		g_panel_tex = SDL_CreateTexture(a->r, PLAT_TARGET_FORMAT,
		                                SDL_TEXTUREACCESS_TARGET,
		                                ZYPEROS_SCREEN_W, ZYPEROS_SCREEN_H);
		if (!g_panel_tex) {
			if (!said) fprintf(stderr, "menus: no panel pass: %s\n", SDL_GetError());
			said = true;
			return false;
		}
		SDL_SetTextureBlendMode(g_panel_tex, SDL_BLENDMODE_BLEND);
		/* Set on the texture, as every texture drawn scaled here sets it:
		 * the rim-free edge is linear filtering's, whatever the hint says */
		SDL_SetTextureScaleMode(g_panel_tex, SDL_ScaleModeLinear);
	}
	if (SDL_SetRenderTarget(a->r, g_panel_tex) != 0) {
		/* Wherever a failed switch left drawing, it goes back to the screen */
		plat_draw_to_screen();
		if (!said) fprintf(stderr, "menus: no panel pass: %s\n", SDL_GetError());
		said = true;
		return false;
	}
	SDL_SetRenderDrawColor(a->r, e.r, e.g, e.b, 0);
	SDL_RenderClear(a->r);
	return true;
}

/* Back on the screen, the panel's part of the texture copied about its
 * middle. The panel is opaque in its shape and clear outside it, so the copy
 * at `pose.alpha` is the panel at that opacity, exactly. */
void panel_pass_end(app *a, const SDL_Rect *panel, mm_pose pose)
{
	SDL_FRect d = panel_scaled(panel, pose.scale);

	plat_draw_to_screen();
	SDL_SetTextureAlphaMod(g_panel_tex, (Uint8)(pose.alpha * 255.0f + 0.5f));
	SDL_RenderCopyF(a->r, g_panel_tex, panel, &d);
}

/* THE PANEL ON SCREEN, KEPT WHOLE. What leaves on a submenu, a back or a
 * close is the panel the screen showed last, and the screen that draws it
 * leaving is never the one that drew it: a submenu draws its parent's, a
 * parent its submenu's once that loop has gone, the shelf the last one's. So
 * each panel drawn is copied (src/menusnap.h), the text of its rows and
 * Zyper's rig with it, to be drawn again by whoever comes next (snap_draw).
 * A few kilobytes a frame; a list too long to keep is not kept, and that
 * panel is cut rather than faded.
 *
 * Two copies. `g_snap` is the panel on screen. `g_snap_next` is the panel of
 * a screen still waiting for the one on screen to leave, which it draws
 * leaving in its own place: a press on it ends that leave and puts its own
 * panel at rest, and from then on its own is the panel on screen
 * (snap_promote), what the next motion takes off. */

static bool panel_is_tiles(int kind)
{
	return kind == PANEL_TILES || kind == PANEL_TILES_GRID || kind == PANEL_LIST;
}

/* The marks of the tiles tiles_show is showing, a ui_glyph a tile in the
 * rows' order, which snap_record keeps with the panel's copy */
static const int *g_tile_marks;
static int g_tile_marks_n;

static menu_snap  g_snaps[2];
static menu_snap *g_snap = &g_snaps[0], *g_snap_next = &g_snaps[1];

/* A panel's arguments into `s`, as the screen on top drew them now: its
 * heading's caption too (menu_draw_caption), or NULL for none */
static void snap_record(menu_snap *s, int kind, const char *heading,
                        const menu_row *rows, int n, int sel, int fixed_w,
                        unsigned accent, const unsigned *vcolors,
                        bool visits_all, int loop_at, const char *heading_note)
{
	if (menusnap_take(s, heading, rows, n, sel, fixed_w, accent, vcolors,
	                  visits_all, loop_at, g_menu_zyper,
	                  g_menu_zyper_at - anim_now()))
		menusnap_note(s, heading_note);
	s->level = mm_depth();
	s->frame = g_frame_no;
	s->kind  = kind;
	s->tab   = g_menu_tabs ? g_menu_tabs->at : -1;
	s->tab_count = g_menu_tabs ? g_menu_tabs->count : 0;
	s->tab_name  = g_menu_tabs ? g_menu_tabs->name : NULL;
	s->row_mark = (void *)g_row_mark;
	s->header = (void *)g_menu_header;
	s->header_h = g_menu_header_h;
	s->side = (void *)g_menu_side;
	s->side_w = g_menu_side_w;
	if (panel_is_tiles(kind)) {
		int i;

		for (i = 0; i < n && i < MENUSNAP_ROWS; i++)
			s->marks[i] = g_tile_marks && i < g_tile_marks_n ? g_tile_marks[i] : UI_GLYPH_COUNT;
	}
}

/* The panel on screen again, at `pose`, Zyper going on from where he was:
 * on screen in this frame too. */
static void snap_draw(app *a, mm_pose pose)
{
	const zr_rig *z = g_menu_zyper;
	unsigned at = g_menu_zyper_at;
	const menu_snap *s = g_snap;

	if (!s->ok || !(pose.alpha > 0.0f)) return;
	g_snap->frame = g_frame_no;
	if (panel_is_tiles(s->kind)) {
		tiles_panel(a, s->kind, s->heading, s->rows, s->marks, s->n, s->sel, pose);
		return;
	}
	if (s->kind == PANEL_HOT) {
		hot_panel(a, s->heading, s->rows, s->n, pose);
		return;
	}
	if (s->kind == PANEL_P2) {
		p2_panel(a, s->heading, s->rows, s->n, s->accent, pose);
		return;
	}
	if (s->kind == PANEL_MOVE) {
		move_page(a, s->heading, s->rows, s->n, pose);
		return;
	}
	static menu_tabs rest;
	const menu_tabs *tabs = g_menu_tabs;

	g_menu_zyper = s->zyper ? &s->zy : NULL;
	g_menu_zyper_at = anim_now() + s->zy_ahead;
	/* the tab row as it was, at rest, with the names of its own set */
	rest = (menu_tabs){ s->tab, s->tab, 0, 0, s->tab_count, s->tab_name };
	g_menu_tabs = s->tab >= 0 && s->tab_name ? &rest : NULL;
	{
		void (*mark)(app *, const menu_row *, int, int, float) = g_row_mark;
		void (*head)(app *, const SDL_Rect *) = g_menu_header;
		void (*side)(app *, const menu_side *) = g_menu_side;
		int head_h = g_menu_header_h, side_w = g_menu_side_w;

		g_row_mark = (void (*)(app *, const menu_row *, int, int, float))s->row_mark;
		g_menu_header = (void (*)(app *, const SDL_Rect *))s->header;
		g_menu_header_h = s->header_h;
		g_menu_side = (void (*)(app *, const menu_side *))s->side;
		g_menu_side_w = s->side_w;
		g_snap_drawing = true;
		menu_panel(a, s->heading, s->rows, s->n, s->sel, s->fixed_w, s->accent,
		           s->has_vcolors ? s->vcolors : NULL, s->visits_all, s->loop_at,
		           s->note, pose);
		g_snap_drawing = false;
		g_row_mark = mark;
		g_menu_header = head;
		g_menu_header_h = head_h;
		g_menu_side = side;
		g_menu_side_w = side_w;
	}
	g_menu_tabs = tabs;
	g_menu_zyper = z;
	g_menu_zyper_at = at;
}

/* The level of the panel on screen - the one drawn last, if nothing has
 * begun a frame since - or 0 for none. */
static int snap_shown(void)
{
	return g_snap->ok && g_snap->frame == g_frame_no ? g_snap->level : 0;
}

/* Level `lv`'s own panel, kept as it waited, is now the one on screen; or,
 * with no copy of it, none is. The level of the panel on screen. */
static int snap_promote(int lv)
{
	menu_snap *t;

	if (!g_snap_next->ok || g_snap_next->level != lv) {
		g_snap->ok = false;
		return 0;
	}
	t = g_snap;
	g_snap = g_snap_next;
	g_snap_next = t;
	g_snap->frame = g_frame_no;
	return lv;
}

mm_frame g_pframe = { { 1.0f, 0.0f }, { 1.0f, 1.0f }, 1.0f, false };

/* Every screen's panel, a menu's or the sequence card's. Until its own turn
 * comes, the panel on screen before it is drawn leaving in its place, and its
 * own kept aside; from then on its own is drawn, where the motion says, and
 * kept as the panel on screen. */
void panel_show(app *a, int kind, const char *heading,
                const menu_row *rows, int n, int sel, int fixed_w,
                unsigned accent, const unsigned *vcolors,
                bool visits_all, int loop_at, const char *heading_note)
{
	mm_frame f = g_pframe;

	g_pframe = PANEL_REST;
	if (!(f.panel.alpha > 0.0f)) {
		snap_draw(a, f.before);
		snap_record(g_snap_next, kind, heading, rows, n, sel, fixed_w, accent,
		            vcolors, visits_all, loop_at, heading_note);
		return;
	}
	snap_record(g_snap, kind, heading, rows, n, sel, fixed_w, accent, vcolors,
	            visits_all, loop_at, heading_note);
	if (panel_is_tiles(kind)) tiles_panel(a, kind, heading, rows, g_snap->marks, n, sel, f.panel);
	else if (kind == PANEL_HOT) hot_panel(a, heading, rows, n, f.panel);
	else if (kind == PANEL_P2) p2_panel(a, heading, rows, n, accent, f.panel);
	else if (kind == PANEL_MOVE) move_page(a, heading, rows, n, f.panel);
	else menu_panel(a, heading, rows, n, sel, fixed_w, accent, vcolors,
	                visits_all, loop_at, heading_note, f.panel);
}

/* A question in tiles through panel_show: its style as the kind
 * (PANEL_TILES, PANEL_TILES_GRID or PANEL_LIST) and each tile's mark, a
 * ui_glyph in the rows' order (`nmarks` of them; UI_GLYPH_COUNT for none).
 * The rows are the tiles and then tiles_panel's notes. */
void tiles_show(app *a, int kind, const char *heading, const menu_row *rows, int n,
                const int *marks, int nmarks, int sel)
{
	g_tile_marks = marks;
	g_tile_marks_n = nmarks;
	panel_show(a, kind, heading, rows, n, sel, 0, MENU_ZYPEROS, NULL, false, 0, NULL);
	g_tile_marks = NULL;
	g_tile_marks_n = 0;
}

/* Existing panels have no heading caption and keep their exact layout. */
void menu_draw_ex(app *a, const char *heading, const menu_row *rows,
                  int n, int sel, int fixed_w, unsigned accent,
                  const unsigned *vcolors, bool visits_all, int loop_at)
{
	panel_show(a, PANEL_MENU, heading, rows, n, sel, fixed_w, accent, vcolors,
	           visits_all, loop_at, NULL);
}

/* A panel with a caption at the heading's right, `heading_note` - About's
 * page count - and otherwise menu_draw_ex's: it opens, closes and is kept
 * for the next screen to draw leaving as every menu's panel is. */
void menu_draw_caption(app *a, const char *heading, const menu_row *rows,
                       int n, int sel, int fixed_w, unsigned accent,
                       const unsigned *vcolors, bool visits_all, int loop_at,
                       const char *heading_note)
{
	panel_show(a, PANEL_MENU, heading, rows, n, sel, fixed_w, accent, vcolors,
	           visits_all, loop_at, heading_note);
}

void menu_draw(app *a, const char *heading, const menu_row *rows, int n,
               int sel, int fixed_w, unsigned accent)
{
	menu_draw_ex(a, heading, rows, n, sel, fixed_w, accent, NULL, false, 0);
}

void menu_draw_styled(app *a, const menu_style *st, const char *heading,
                      const menu_row *rows, int n, int sel)
{
	g_menu_tabs = st->tabs;
	g_row_mark = st->row_mark;
	g_menu_header = st->header;
	g_menu_header_h = st->header_h;
	g_menu_side = st->side;
	g_menu_side_w = st->side_w;
	if (st->row_icon) g_row_icon = st->row_icon;
	menu_draw_ex(a, heading, rows, n, sel, st->fixed_w,
	             st->follow_tint ? a->tint : st->accent, st->vcolors, st->visits_all, 0);
	if (st->row_icon) g_row_icon = NULL;
	g_menu_side = NULL;
	g_menu_side_w = 0;
	g_menu_header = NULL;
	g_row_mark = NULL;
	g_menu_tabs = NULL;
}

/* A one-line message, DESIGN.md 11.8: a panel holding one row, its text
 * **text-soft** - the message is the panel's content, and a failure is not
 * quieter than a success. The row's `live` is the caller's and is kept, for
 * whether a long message scrolls. */
static void menu_message(app *a, const char *heading, menu_row row,
                         unsigned accent)
{
	static const unsigned msg[1] = { VCOL_MESSAGE };

	menu_draw_ex(a, heading, &row, 1, -1, 0, accent, msg, false, 0);
}

/* Video snaps come with a lookup for a game whose system has its videos on
 * (MENU > Scraper's Systems, snaps_get), and which Videos for takes: every
 * game, the favorites, or the games played (src/snaps.h). A favorite and a
 * game's play time are kept by its system's tag, so the systems a run
 * covers are handed here as it starts (g_snap_sys); without them only "all"
 * gets videos. The play times are folded once a run (stats_summarize). */
const systems_cfg *g_snap_sys;

static bool snap_wanted(const char *folder, const char *file)
{
	const char *tag = NULL;
	int which, i;
	long secs = 0, last = 0;

	if (!snaps_get(folder)) return false;
	if ((which = snaps_for()) == SNAPS_FOR_ALL) return true;
	for (i = 0; g_snap_sys && i < g_snap_sys->count; i++)
		if (!strcmp(g_snap_sys->systems[i].folder, folder)) { tag = g_snap_sys->systems[i].tag; break; }
	if (!tag) return false;
	if (which == SNAPS_FOR_PLAYED && !(stats_lookup(tag, file, &secs, &last) && (secs > 0 || last > 0)))
		return false;
	return snaps_for_game(which, fav_is(tag, file), which == SNAPS_FOR_PLAYED);
}

/* MENU > Scraper's value, from the last count of missing covers: taken as
 * its page closes (scraper_page), never per frame */
char g_scraper_val[32];

/* The two menus behind MENU, their row indices and their buffers: src/sys_menu.h */

/* Build whichever menu the current screen calls for. Returns the row count, so
 * the input loop never needs to know which of the two it is driving. */
/* One answer per two seconds, for every row that needs it.
 *
 * wifi_status forks wpa_cli, and this launcher is a 119 MB process - forking it
 * copies page tables and costs about thirty milliseconds, not the seven a small
 * process pays. menu_build runs every frame, and the ZyperOS menu asked three
 * times: once cached for the Wi-Fi row, then twice more, uncached, for Over The
 * Hare and Box Art. Measured on the device that was 12 fps with the menu open,
 * against a loop that delays 8 ms - two thirds of every frame spent forking.
 *
 * The cache existed and said why; the two rows below it defeated it. So the
 * cache moves out here where it covers all of them, and nothing calls
 * wifi_status directly from a per-frame path again. */
wifi_state menu_wifi(char *ssid, int cap)
{
	static unsigned   next;
	static wifi_state st = WIFI_OFF;
	static char       name[WIFI_SSID_MAX];
	unsigned now = plat_now_ms();

	if (next == 0 || now >= next) {
		st = wifi_status(name, sizeof name, NULL, 0);
		next = now + 2000;
		g_wifi_asked_at = now;
	}
	if (ssid && cap) snprintf(ssid, (size_t)cap, "%s", name);
	return st;
}

/* A way out to the internet, for every row and page that fetches from it
 * (box art, a game's cover and video, System Update, achievements,
 * PortMaster): Wi-Fi joined, or a default route out on another link, a
 * phone's USB tethering among them (phonelink_way_out; the Pixel 9 Pro XL's,
 * 2026-11-13). Never the USB cable's link to a computer, which leads nowhere.
 * *metered: the way out is the phone's, so its mobile data, likely: nothing
 * large starts by itself over it. Read again at most twice a second, as the
 * menus ask every frame (menu_wifi's cache keeps Wi-Fi's cheap). */
bool net_internet(bool *metered)
{
	static unsigned at;
	static bool out, meter;
	unsigned now = plat_now_ms();

	if (!at || now - at >= 500u) {
		char route[4096];     /* about 30 routes; a line is 128 bytes */
		FILE *f = fopen("/proc/net/route", "r");
		size_t n = f ? fread(route, 1, sizeof route - 1, f) : 0;

		if (f) fclose(f);
		route[n] = '\0';
		out = phonelink_way_out(menu_wifi(NULL, 0) == WIFI_CONNECTED, route, &meter);
		at = now ? now : 1;
	}
	if (metered) *metered = out && meter;
	return out;
}

/* USB File Transfer has a link without Wi-Fi: the cable to a computer, or a
 * phone's tethering that has its address (only looked at: phone_link_due
 * set it up). MENU's build and the lists that name its rows ask the same. */
bool xfer_link_up(void)
{
	char pip[64];

	return plat_cable_link(NULL, 0) || (plat_phone_link(false, NULL, 0, pip, sizeof pip) && pip[0]);
}

/* Gather what the device currently is, and hand it to sys_menu_build.
 *
 * The split: this half is allowed to fork wpa_cli and read the app, the other
 * half is not allowed to do anything but produce rows. That is what lets
 * tools/menu-check.c state a whole device situation in a struct initializer
 * and ask what the menu says about it.
 *
 * Returns the row count, so the input loop never needs to know which of the
 * two menus it is driving. */
/* ---- System Update (src/sysupdate.h, docs/updates.md) -------------------
 *
 * The work is the update program's, zyperos-update in the ZyperOS folder:
 * this side starts it, reads the status file it keeps in .userdata/<device>/
 * update, and draws that. The program goes on when the screen is left. */

void up_path(char *buf, size_t n, const char *name)
{
	snprintf(buf, n, "%s/update/%s", P_USERDATA, name);
}

/* The card has the program: a card from before System Update has no row */
bool up_have(void)
{
	char p[512];
	snprintf(p, sizeof p, "%s/zyperos-update", P_ROOT);
	return access(p, X_OK) == 0;
}

/* At work now: it holds the lock file for as long as it runs */
bool up_running(void)
{
	char p[512];
	int fd;
	bool busy;

	up_path(p, sizeof p, "lock");
	fd = open(p, O_RDWR);
	if (fd < 0) return false;
	busy = flock(fd, LOCK_EX | LOCK_NB) != 0;
	close(fd);
	return busy;
}

bool up_read(const char *path, char *buf, size_t n)
{
	FILE *f = fopen(path, "r");
	size_t got;
	if (!f) { buf[0] = 0; return false; }
	got = fread(buf, 1, n - 1, f);
	fclose(f);
	buf[got] = 0;
	return true;
}

void up_first_line(const char *path, char *buf, size_t n)
{
	up_read(path, buf, n);
	buf[strcspn(buf, "\r\n")] = 0;
}

/* A network to look with: a way out (net_internet: Wi-Fi, or a phone's
 * tethering), or a test release's source set on the card (docs/updates.md),
 * which may be on the cable */
static bool up_network(void)
{
	char p[512];

	if (net_internet(NULL)) return true;
	up_path(p, sizeof p, "source");
	return access(p, R_OK) == 0;
}

static su_view g_up;
unsigned g_up_read_at;

/* Everything the screen shows, read again at most four times a second: the
 * build asks every frame, and these files are on the card */
const su_view *up_view(bool now)
{
	unsigned t = plat_now_ms();
	char p[512], text[1024], sysv[64] = "";

	if (!now && g_up_read_at && t - g_up_read_at < 250) return &g_up;
	g_up_read_at = t ? t : 1;
	memset(&g_up, 0, sizeof g_up);
	up_path(p, sizeof p, "status");
	up_read(p, text, sizeof text);
	su_status_parse(text, &g_up.st);
	g_up.running = up_running();
	g_up.wifi = up_network();
	/* Over a phone's mobile data a download cut short waits for A
	 * (su_auto_step) */
	{
		bool metered = false;

		g_up.metered = net_internet(&metered) && metered;
	}
	snprintf(p, sizeof p, "%s/VERSION", P_ROOT);
	up_first_line(p, g_up.installed, sizeof g_up.installed);
	/* Roll Back only to a version the image would not copy straight over
	 * (update/apply.sh's rule): not older than the image's own */
	snprintf(p, sizeof p, "%s/ZyperOS.prev/VERSION", P_CARD);
	up_first_line(p, g_up.prev, sizeof g_up.prev);
	up_read("/etc/zyperos/system", text, sizeof text);
	sscanf(text, "version %63s", sysv);
	if (g_up.prev[0] && sysv[0] && su_version_cmp(g_up.prev, sysv) < 0) g_up.prev[0] = 0;
	/* On a card with two system slots, Roll Back goes to the system in the
	 * other slot (zyperos-update rollback decides; it says which) */
	if (g_up.st.other[0]) snprintf(g_up.prev, sizeof g_up.prev, "%s", g_up.st.other);
	up_path(p, sizeof p, "result");
	up_first_line(p, g_up.result, sizeof g_up.result);
	up_path(p, sizeof p, g_up.st.system ? "latest-system.txt" : "latest.txt");
	{
		FILE *f = fopen(p, "r");
		char line[256];
		while (f && fgets(line, sizeof line, f))
			if (!strncmp(line, "note ", 5)) { g_up.notes = true; break; }
		if (f) fclose(f);
	}
	if (!plat_battery(&g_up.battery, &g_up.charging)) { g_up.battery = 100; g_up.charging = true; }
	if (g_up.st.time > 0) {
		time_t tt = (time_t)g_up.st.time;
		struct tm tm;
		localtime_r(&tt, &tm);
		strftime(g_up.checked, sizeof g_up.checked, g_clock24 ? "%H:%M" : "%I:%M %p", &tm);
		if (g_up.checked[0] == '0' && !g_clock24) memmove(g_up.checked, g_up.checked + 1, strlen(g_up.checked));
	}
	return &g_up;
}

/* At work on an update: downloading, checking what it fetched, copying the
 * folder or writing the system. A look (checking) is not: it is one small
 * request, and Auto Off may come. */
bool up_working(void)
{
	const su_view *v;

	if (!up_have()) return false;
	v = up_view(false);
	if (!v->running) return false;
	return v->st.state == SU_DOWNLOADING || v->st.state == SU_COPYING ||
	       v->st.state == SU_VERIFYING || v->st.state == SU_INSTALLING;
}

/* Start the program: check, download or rollback, on its own, as the device
 * name of the card's .userdata folder */
void up_spawn(const char *cmd)
{
	char prog[512], dev[64];
	const char *slash = strrchr(P_USERDATA, '/');
	char *argv[] = { prog, (char *)cmd, "--card", (char *)P_CARD, "--device", dev, NULL };

	snprintf(prog, sizeof prog, "%s/zyperos-update", P_ROOT);
	snprintf(dev, sizeof dev, "%s", slash ? slash + 1 : P_USERDATA);
	fprintf(stderr, "update: %s\n", cmd);
	plat_spawn_detached(argv, NULL, P_ROOT);
	g_up_read_at = 0;
}

/* System Update's news, said once per release (DESIGN.md 11.17): a new
 * release a look found, and a download done in the background. Zyper's
 * bubble on the shelf (the heat pop-up's, with these words), or a notice
 * over a game; one that comes over a menu waits for the shelf. */
static char g_up_news[2][160];

static void up_news(const char *key, const char *fmt, const char *version, bool in_game)
{
	char told[40] = "";

	db_get_str(db_dev(), key, told, sizeof told, "");
	if (!version[0] || !strcmp(told, version)) return;
	db_set_str(db_dev(), key, version);
	snprintf(g_up_news[0], sizeof g_up_news[0], fmt, version);
	snprintf(g_up_news[1], sizeof g_up_news[1], "%s", T(S_UPDATE_NEWS_WHERE));
	fprintf(stderr, "update: news: %s\n", g_up_news[0]);
	if (in_game) {
		nq_item it = { .kind = NQ_SET, .ms = 5000 };

		snprintf(it.head, sizeof it.head, "%s", T(S_SYSMENU_UPDATE));
		snprintf(it.body, sizeof it.body, "%s", g_up_news[0]);
		nq_push(&g_nq, &it);
		g_up_news[0][0] = '\0';
	}
}

/* The shelf's pass: the news up in Zyper's bubble, when there is some and
 * nothing else of his is up */
static void up_news_watch(void)
{
	unsigned now = plat_now_ms();

	if (!g_up_news[0][0] || g_heatpop.on || g_songpop.on) return;
	heatpop_show(now);
	snprintf(g_heatpop.line1, sizeof g_heatpop.line1, "%s", g_up_news[0]);
	snprintf(g_heatpop.line2, sizeof g_heatpop.line2, "%s", g_up_news[1]);
	g_up_news[0][0] = '\0';
}

/* Unlocks that had waited - earned offline - on the account now: said once,
 * in Zyper's bubble on the shelf, as the update's news is. Counted up until
 * it is said, so two passes close together make one notice. */
static int g_ra_news;

static void ra_sent_news(int n)
{
	if (n <= 0) return;
	g_ra_news += n;
	fprintf(stderr, "ra: %d waiting unlock%s on the account now\n", n, n == 1 ? "" : "s");
}

static void ra_news_watch(void)
{
	unsigned now = plat_now_ms();

	if (g_ra_news <= 0 || g_heatpop.on || g_songpop.on || g_up_news[0][0]) return;
	heatpop_show(now);
	snprintf(g_heatpop.line1, sizeof g_heatpop.line1, TN(S_CHEEVOS_SENT, g_ra_news), g_ra_news);
	snprintf(g_heatpop.line2, sizeof g_heatpop.line2, "%s", T(S_CHEEVOS_SENT_WHERE));
	g_ra_news = 0;
}

/* The update's own clock, every 2 s, from every screen's loop through
 * tick_theme and from a game's tick: su_auto_step's looks and resumes
 * (src/sysupdate.h), and the news, once per release */
void tick_update_daily(bool in_game)
{
	static unsigned last;
	static su_auto au;
	unsigned t = plat_now_ms();
	const su_view *v;

	if (last && t - last < 2000) return;
	last = t ? t : 1;
	if (t - t_boot0 < 30000 || !up_have()) return;
	v = up_view(true);
	if (v->st.state == SU_UNSUPPORTED) return;
	/* The news: a release found, and one ready */
	if (!v->running && v->st.state == SU_AVAILABLE)
		up_news("update.told", T(S_UPDATE_NEWS_AVAILABLE), v->st.version, in_game);
	if (!v->running && v->st.state == SU_READY)
		up_news("update.ready_told", T(S_UPDATE_NEWS_READY), v->st.version, in_game);
	switch (su_auto_step(&au, v, t, (long long)time(NULL))) {
	case SU_AUTO_RESUME:
		fprintf(stderr, "update: the download was cut short: going on\n");
		up_spawn("download");
		break;
	case SU_AUTO_CHECK:
		up_spawn("check");
		break;
	default:
		break;
	}
}


/* ---------- MENU > Random Game -------------------------------------------- */

/* A game picked (src/randgame.h): from every system on the systems row's
 * menu, from the shelf's own games in a system's menu. False, with deny
 * heard, when there is none to pick. The menu closes on a pick and
 * pick_launch shows it and starts it. */
bool random_pick(app *a, bool all, int *shelf, int *game)
{
	/* The game picked last time, so the next is another */
	static int last_shelf = -1, last_game = -1;
	int count[CFG_MAX_SYSTEMS], i;
	bool use[CFG_MAX_SYSTEMS];

	for (i = 0; i < a->sys.count; i++) {
		const system_cfg *s = &a->sys.systems[i];

		/* Every game of the system, in its folders too: the pick is an
		 * index of sys_games, which pick_launch opens the folder of */
		count[i] = sys_games(&a->view[i])->count;
		/* Not Muse, whose cards are albums, nor - from every system -
		 * Favorites, whose games are on their own shelves too */
		use[i] = all ? !is_muse(s) && !a->view[i].owner && !pm_only_offer(a, i)
		             : i == a->sys_cursor && !is_muse(s);
	}
	if (!rg_pick(count, use, a->sys.count, rg_random(), last_shelf, last_game,
	             shelf, game)) {
		uisound_play(DS_SOUND_DENY);
		return false;
	}
	last_shelf = *shelf;
	last_game = *game;
	uisound_play(DS_SOUND_REWARD);
	return true;
}

/* ---------- the WiFi screen ----------------------------------------------- */

/* One frame with a single line on it, drawn before each blocking call.
 *
 * wifi_up can take twenty seconds waiting out the stock service's retry loop,
 * a scan takes several, and an association takes up to twenty more. None of
 * that is asynchronous here, so the loop is not running and input is not read
 * while it happens. Painting the reason first is the difference between a
 * device that is working and a device that has hung: the picture is identical
 * otherwise, and the second reading is the one that gets a power cycle. */
/* One line on a panel over the shelf, for the moments something is happening
 * and there is nothing yet to show. The heading is a parameter because it was
 * not: this began as the Wi-Fi screen's own and said "Wi-Fi" over every
 * message it was given, so signing in to RetroAchievements and fetching a set
 * both announced themselves as Wi-Fi. */
void wait_panel(app *a, const char *heading, const char *msg)
{
	menu_row row = { msg, NULL, false };

	tick_theme(a);
	draw_shelf(a);
	ui_scrim(a->r, 0);
	menu_message(a, heading, row, MENU_ZYPEROS);
	plat_draw_osd(a->r);
	plat_present();
}

/* MENU CLOSES THE WHOLE MENU, from any depth in it. B is back one level.
 * Eric's call, 2026-09-23: MENU used to mean exactly what B meant, so from
 * Play Time or Wi-Fi it went back one screen instead of out, the same button
 * doing one thing to open the menu and another inside it.
 *
 * A menu screen that sees MENU raises this and leaves; every screen under it
 * then leaves on its next pass, until the one MENU opened is gone. It is put
 * down again only OUTSIDE the menus - the shelf's loop, game_menu, Muse - and
 * never when a menu opens: a screen that opens a second one after the first
 * came back has to see it still up, or that second one would stay open. */
bool g_menu_closing;
/* The in-game menu is up, over a paused game (game_menu): nothing may close
 * it from outside, as a plugged-in computer closes the others */
bool g_in_game_menu;

/* ---- the interface sounds' hooks (DESIGN.md 9, src/uisound.h) ----------
 *
 * Each hook is one call where a press is decided, so the rule stays in
 * src/uisound.c and a screen only says what happened. */

/* Whether the last step was held against an end. One for the whole launcher:
 * input is flushed whenever a screen opens, so a hold never carries from one
 * list to another. */
static uis_steps g_steps;

/* A step along a list or shelf. Wrapping is normal movement. */
void step_sound(app *a, in_button b, int from, int to)
{
	bool moved = from != to;

	uisound_cue(uisound_step(&g_steps, moved,
	                         !a->in.pressed[b]));
}

/* A list kept as a plain index (0 to n - 1) by a screen with its own loop,
 * stepped by the rule every menu has (menu_step_index) and heard */
int list_step_heard(app *a, in_button b, int n, int sel)
{
	int to = menu_step_index(n, sel, b == IN_UP ? -1 : +1, a->in.pressed[b]);

	step_sound(a, b, sel, to);
	return to;
}

/* A step that always lands somewhere new, and never goes round: a track
 * skipped, a seek. */
void moved_sound(app *a, in_button b)
{
	step_sound(a, b, 0, 1);
}

/* A step along a menu, heard: a fresh press goes round past either end, a
 * held direction stops at the end (DESIGN.md 11.3, my pick 2026-11-12), so
 * a thumb left on the d-pad never runs the list past where it wanted. */
static int menu_step_heard(app *a, const menu_row *rows, int n, int sel,
                           int dir, in_button b)
{
	int to = a->in.pressed[b] ? menu_step_round(rows, n, sel, dir)
	                          : menu_step_sel(rows, n, sel, dir);

	step_sound(a, b, sel, to);
	return to;
}

/* MENU was pressed in a screen over this one: every screen is closing */
bool menu_closing(void)
{
	return g_menu_closing;
}

/* True when a menu screen should leave: B, MENU, or a MENU pressed deeper in.
 * The press that leaves is answered with back; the screens under it, which
 * leave on the flag with the press already flushed, are silent. */
bool menu_leaving(app *a)
{
	if (a->in.pressed[IN_MENU]) g_menu_closing = true;
	if (a->in.pressed[IN_BACK] || a->in.pressed[IN_MENU])
		uisound_play(DS_SOUND_BACK);
	return a->in.pressed[IN_BACK] || g_menu_closing;
}

/* ---- a screen with a panel, opening and closing (DESIGN.md 8.2) ---------
 *
 * Every loop that draws a menu's panel goes through these, so none of them
 * carries a copy of the motion: panel_open as the screen opens, panel_frame
 * before each frame it draws, panel_close on its way out. Whatever it goes
 * back to draws its panel leaving: the screen under it in its own next
 * frames (menu_draw_ex), the shelf and Muse's shelf after theirs
 * (panel_leave_draw), the paused game before it is handed back
 * (panel_leave_run). src/menumotion.h has the rules.
 *
 * Nothing waits on any of it. Input is read and acted on as it always was,
 * and a press puts the panel where it belongs on the frame it is acted on. */

/* A press acting on level `lv`'s screen now (mm_settle): when `lv` was
 * still waiting for the panel on screen to leave, its own, kept as it waited,
 * is the panel on screen from now. The level of the panel on screen. */
static int panel_settle(int lv)
{
	int shown = snap_shown(), on = mm_settle(lv, anim_now(), shown);

	return on != shown ? snap_promote(on) : on;
}

/* The screen opens, over the one on top, which the press that opened it
 * acted on (panel_settle). Its level, for the others. What an earlier screen
 * at this depth kept as it waited is none of this one's. */
int panel_open(void)
{
	int lv = mm_open(anim_now(), panel_settle(mm_depth()) > 0);

	g_snap_next->ok = false;
	return lv;
}

/* Before a frame of level `lv`'s screen. False when the frame is not to be
 * drawn at all: while MENU closes every screen, the ones under the screen it
 * was pressed in leave without drawing again, and the panel on screen leaves
 * once, over the shelf. Otherwise a press this frame puts the panel at rest,
 * the next menu_draw_ex draws it where the motion says, and `*scrim` is the
 * scrim's strength for the backdrop under it. */
bool panel_frame(app *a, int lv, float *scrim)
{
	int b;

	if (g_menu_closing) return false;
	for (b = 0; b < IN_COUNT; b++)
		if (a->in.pressed[b]) { mm_press(lv); break; }
	g_pframe = mm_at(lv, anim_now());
	*scrim = g_pframe.scrim;
	return true;
}

/* Level `lv`'s screen has gone, and the panel on screen leaves over what
 * comes next - unless `cut`: quit, power, a game stopped leave nothing to see
 * go. The press that closed it is not handed on to the screen under it,
 * which would take it for its own and put its panel at rest. */
static void panel_close_at(app *a, int lv, bool cut, unsigned now)
{
	int shown = cut ? 0 : snap_shown();

	/* A deeper panel is on screen: the screen came back from a submenu and
	 * was still waiting for it to leave, or MENU closed the screens over it
	 * without drawing them again. The press that closes it acts on it. */
	if (shown > lv) shown = panel_settle(lv);
	mm_close(lv, now, shown, g_menu_closing);
	g_snap_next->ok = false;
	memset(a->in.pressed, 0, sizeof a->in.pressed);
	if (mm_leaving(anim_now()).moving) redraw_now();
}

void panel_close(app *a, int lv, bool cut)
{
	panel_close_at(a, lv, cut, anim_now());
}

/* Muse's shelf, which has no panel and is drawn over everything: the panel on
 * screen leaves over it, and is given back as it opened when it closes.
 * Nothing Muse does closes the menu under it (muse_open), MENU included. */
int panel_base(void)
{
	return mm_base(anim_now(), panel_settle(mm_depth()) > 0);
}

void panel_base_close(int lv)
{
	mm_close(lv, anim_now(), 0, false);
}

/* MENU closed the screens over the one on top, and stops there: Muse's
 * menu, which MENU closes and nothing under it (muse_menu). The screen on top
 * comes back as after a back (mm_return). */
void panel_return(void)
{
	mm_return(mm_depth(), anim_now(), snap_shown());
	g_snap_next->ok = false;
}

/* After a base's own frame - the shelf's, Muse's shelf's - the last panel
 * closed onto it leaving, and its scrim, while they last. A press there has
 * acted on the base already, and takes them off at once. True while they
 * are going, for the caller to draw the next frame. */
bool panel_leave_draw(app *a, int game)
{
	mm_frame f;
	int b;

	for (b = 0; b < IN_COUNT; b++)
		if (a->in.pressed[b]) { mm_leaving_cut(); break; }
	f = mm_leaving(anim_now());
	if (!f.moving) return false;
	ui_scrim_k(a->r, game, f.scrim);
	snap_draw(a, f.before);
	return true;
}

/* The same over a base with no loop of its own to draw it in: the paused
 * game, once the in-game menu has closed and before the game is handed back
 * (game_menu). `backdrop` draws what is under it at a scrim's strength.
 *
 * It reads input, as every loop does, and nothing waits on it. Any press
 * ends it there and then, and the game goes on at once: what is pressed is
 * the game's, and Diatom starts reading its own buttons afresh on RESUME. The
 * power button and Auto Off it hands back to the caller, which owns what they
 * mean over a paused game (gm_power): once the game is resumed the wait loop
 * drains the power key's queue as stale, and a press read here and not acted
 * on would be lost. */
leave_end panel_leave_run(app *a, void (*backdrop)(app *, void *, float),
                          void *ctx)
{
	leave_end how = LEAVE_GONE;
	mm_frame f;
	int b;

	for (;;) {
		plat_input_poll(&a->in);
		if (a->in.quit_requested || want_quit) {
			a->running = false;
			how = LEAVE_QUIT;
			break;
		}
		if (a->in.pressed[IN_POWER] || idle_due(a)) { how = LEAVE_POWER; break; }
		for (b = 0; b < IN_COUNT; b++)
			if (a->in.pressed[b]) { mm_leaving_cut(); break; }
		f = mm_leaving(anim_now());
		if (!f.moving) break;
		backdrop(a, ctx, f.scrim);
		snap_draw(a, f.before);
		plat_draw_osd(a->r);
		plat_present();
		ui_frame_rest(8);
	}
	mm_leaving_cut();
	return how;
}

/* A yes/no panel, drawn like wait_panel but answerable.
 *
 * Defaults to "no": the selection starts on the safe row, so a stray press of
 * the button that opened the panel cannot also confirm it. Returns false for
 * BACK, for MENU, and for a power-off, because everything except a deliberate
 * press on the destructive row means "not that".
 *
 * Power is handled here rather than left to the caller. A confirm can sit on
 * screen indefinitely, which makes it exactly the kind of place the device gets
 * put down and the power button pressed. */
/* The question is `nq` rows - one dead row, or a paragraph's lines (Delete
 * Game's, DESIGN.md 11.31) - and the two answers go after it in `rows`,
 * which holds nq + 2. `fixed_w` is the panel's content width, 0 to size it
 * to the rows. `no_label` names the safe answer where Cancel goes, and with
 * a `guard` it is a question that came by itself (ask_settle): a button down
 * as it opened, or pressed in its first half second, counts only once let
 * go. Over the shelf, or over `over`'s backdrop with its power rule: the
 * in-game menu's paused game, where POWER stops the game. confirm_rows_as
 * and confirm_rows_over below are this with the parts they need. */
static bool confirm_rows_full(app *a, const char *heading, menu_row *rows, int nq,
                              const char *yes_label, const char *no_label, int fixed_w, int lv,
                              usbpower_guard *guard, const menu_style *over, void *ctx)
{
	int sel = nq + 1;
	float k;

	rows[nq]     = (menu_row){ yes_label, NULL, true };
	rows[nq + 1] = (menu_row){ no_label, NULL, true };

	for (;;) {
		plat_input_poll(&a->in);
		if (guard) ask_settle(a, guard);
		ui_sounds_follow();
		if (a->in.quit_requested) { a->running = false; return false; }
		if (a->in.pressed[IN_POWER] || idle_due(a)) {
			if (over && over->on_power) over->on_power(a, ctx);
			else power_off(a);
			return false;
		}
		if (menu_leaving(a)) return false;

		/* Only the two answerable rows are reachable; the rows above are the
		 * question. */
		{
			in_button b = in_repeat(&a->in, IN_UP)   ? IN_UP
			            : in_repeat(&a->in, IN_DOWN) ? IN_DOWN : IN_NONE;
			/* The two answers are a list of two, which goes round as every
			 * list does: a fresh press from either end lands on the other,
			 * a held one stops (menu_step_index) */
			if (b != IN_NONE)
				sel = nq + list_step_heard(a, b, 2, sel - nq);
		}
		if (a->in.pressed[IN_ACCEPT]) {
			uisound_play(DS_SOUND_ACCEPT);
			return sel == nq;
		}

		if (!panel_frame(a, lv, &k)) continue;
		tick_tint(a);
		tick_theme(a);
		if (over && over->backdrop) {
			over->backdrop(a, ctx, k);
		} else {
			draw_shelf(a);
			ui_scrim_k(a->r, 0, k);
		}
		menu_draw(a, heading, rows, nq + 2, sel, fixed_w, MENU_ZYPEROS);
		plat_draw_osd(a->r);
		plat_present();
		ui_frame_rest(8);
	}
}

bool confirm_rows_as(app *a, const char *heading, menu_row *rows, int nq,
                     const char *yes_label, const char *no_label, int fixed_w, int lv,
                     usbpower_guard *guard)
{
	return confirm_rows_full(a, heading, rows, nq, yes_label, no_label, fixed_w, lv, guard, NULL, NULL);
}

bool confirm_rows_over(app *a, const char *heading, menu_row *rows, int nq,
                       const char *yes_label, int fixed_w, int lv,
                       const menu_style *over, void *ctx)
{
	return confirm_rows_full(a, heading, rows, nq, yes_label, T(S_PANEL_CANCEL), fixed_w, lv,
	                         NULL, over, ctx);
}

bool confirm_rows(app *a, const char *heading, menu_row *rows, int nq,
                  const char *yes_label, int fixed_w, int lv)
{
	return confirm_rows_full(a, heading, rows, nq, yes_label, T(S_PANEL_CANCEL), fixed_w, lv,
	                         NULL, NULL, NULL);
}

static bool confirm_body(app *a, const char *heading, const char *msg,
                         const char *yes_label, int lv)
{
	menu_row rows[3];

	rows[0] = (menu_row){ msg, NULL, false };
	return confirm_rows(a, heading, rows, 1, yes_label, 0, lv);
}

bool confirm_panel(app *a, const char *heading, const char *msg,
                   const char *yes_label)
{
	int lv = panel_open();
	bool yes = confirm_body(a, heading, msg, yes_label, lv);

	panel_close(a, lv, !a->running);
	return yes;
}

/* ---- the menu runner -------------------------------------------------- */

/* Wi-Fi is the largest menu: the switch, WIFI_MAX_NETS networks, a rule and a
 * note. Distinct from MENU_MAX_ROWS, which is the system menu's own count and
 * is much smaller. */
/* And the Video Snaps page, a row a console (sys_menu_snaps_rows). */
#define MENU_RUN_ROWS_ (WIFI_MAX_NETS + 8 > SN_ROWS ? WIFI_MAX_NETS + 8 : SN_ROWS)
/* And the Achievements tab, a row an achievement: a set's whole list */
#define MENU_RUN_ROWS (MENU_RUN_ROWS_ > CHV_MAX ? MENU_RUN_ROWS_ : CHV_MAX)
/* Every ZyperOS menu row has to fit through the runner too, or the bottom of
 * the menu is cut off with no sign of it. */
_Static_assert(MENU_RUN_ROWS >= MENU_MAX_ROWS, "MENU_RUN_ROWS < MENU_MAX_ROWS");
/* ...and the in-game menu's Controls tab's (src/ctltab.h) */
_Static_assert(MENU_RUN_ROWS >= CT_MAX_ROWS, "MENU_RUN_ROWS < CT_MAX_ROWS");

/* How a screen ended, for the callers that have to act differently. The
 * in-game menu is the one that does: B and MENU mean Continue there, and it
 * has to send RESUME - but the runner owns those two buttons and flushes the
 * input on the way out, so asking a->in afterwards would always say no. */
/* menu_exit, in src/panel.h */

/* Fill `rows`, name the screen, and return how many rows. MUST NOT touch the
 * renderer or the event loop, and the part that actually decides what the rows
 * SAY must be a separate function in a file the check can link - src/wifi_menu.c
 * and src/sys_menu.c are those files. The design has failed if that stops
 * being true.
 *
 * Reading the device from here is allowed and unavoidable: a row that reports
 * the network has to ask every frame. What makes it checkable is that the
 * asking is confined to this half - wifi.c is stubbed on a host build, and the
 * system menu's asking lives in menu_build rather than in sys_menu_build.
 *
 * Called every frame, so it is also where live state is reflected - there is no
 * separate "refresh".
 *
 * The heading comes from here because it is a function of the same state the
 * rows are: the system menu is titled with the system whose rows it is showing.
 * Passing it to the runner separately meant keeping two things in step that are
 * really one thing. */
/* menu_build_fn, in src/panel.h */

/* A result can be reached through Muse over a paused game. Keep that
 * menu's backdrop and power rule through nested menus, until it returns.
 * A stop inside a result must unwind even if its caller returns FAILED or
 * CHANGED, rather than DONE. */
static const menu_style *g_message_over;
static void *g_message_ctx;
bool g_message_stopped;

/* Read every frame through the caller's pointer, so a screen that keeps its
 * style inside its own context can change it between frames. The info screen
 * does: its accent belongs to the system that OWNS the game, and toggling a
 * favorite on the Favorites shelf can hand it a different one. */

/* A key the runner did not consume, and the row it happened on. Free to open a
 * confirm, a keyboard or a wait panel: each runs its own loop and returns.
 * `sel` indexes the rows the last build produced. */
/* menu_key_fn, in src/panel.h */

/* Zyper's answer to how a press went, in the menu he is in (`z`, NULL
 * elsewhere): a hop and a pant for something changed, the cocked head for a
 * failure, nothing for the rest. menu_send answers with it and so does a
 * --zyper shot, so the rule the menu keeps is the one a shot shows. */
static void menu_zyper_answer(zr_rig *z, unsigned now, menu_result r)
{
	if (!z) return;
	if (r == MENU_CHANGED) zr_works(z, now);
	else if (r == MENU_FAILED) zr_fails(z, now);
}

/* A key to the screen, and Zyper's answer to it: any press drops what he had
 * booked, and the answer comes from the moment the handler returns - which,
 * for a sign-in, is when its screen has gone and he can be seen again. */
static menu_result menu_send(app *a, zr_rig *z, menu_key_fn on_key, void *ctx,
                             in_button key, int sel)
{
	menu_result r;

	if (z) zr_press(z, plat_now_ms());
	r = on_key(a, ctx, key, sel);
	menu_zyper_answer(z, plat_now_ms(), r);
	return r;
}

/* Left or right, given to the screen, and answered (DESIGN.md 9). A row whose
 * value steps round always moves, so it is heard on the press, before the
 * screen's handler, which may take a while (MENU_SND_RING); a row that plays
 * its own is left to it (MENU_SND_OWN). Any other - a value that stops at its
 * ends, or a row with nothing to step - can only be heard after: move when
 * its value changed, edge when it did not, the rows built again to see, as
 * the next frame would build them. Each goes through menu_send, so Zyper
 * answers a side key as he answers any other. */
static menu_result menu_side_key(app *a, zr_rig *z, menu_build_fn build,
                                 menu_key_fn on_key, void *ctx,
                                 const menu_row *rows, int sel,
                                 unsigned how, in_button b)
{
	static menu_row now[MENU_RUN_ROWS];
	const char *heading;
	char was[128];
	menu_result r;
	int n;

	if (how & MENU_SND_OWN) return menu_send(a, z, on_key, ctx, b, sel);
	if (how & MENU_SND_RING) {
		moved_sound(a, b);
		return menu_send(a, z, on_key, ctx, b, sel);
	}
	snprintf(was, sizeof was, "%s",
	         rows[sel].value && !ROW_IS_NOTE(rows[sel]) ? rows[sel].value : "");
	r = menu_send(a, z, on_key, ctx, b, sel);
	if (r == MENU_DONE || !a->running || want_quit || g_message_stopped) return r;
	n = build(ctx, now, MENU_RUN_ROWS, &heading);
	if (n > MENU_RUN_ROWS) n = MENU_RUN_ROWS;
	uisound_cue(uisound_step(&g_steps,
	                         sel >= n || strcmp(was, now[sel].value && !ROW_IS_NOTE(now[sel])
	                                                 ? now[sel].value : ""), !a->in.pressed[b]));
	return r;
}

/* The loop itself. Called only through menu_run below, which owns the flush on
 * either side of it. */
/* A panel screen's picture, for pace_due (src/pace.h): its heading and
 * rows, the cursor, and what the frame around them shows - the scrim's
 * strength, the tint, the theme, the replug message */
unsigned pace_rows(const char *heading, const menu_row *rows, int n, int sel, float k,
                   unsigned tint)
{
	unsigned h = pace_menu(5381u, heading, rows, n, sel);

	h = pace_int(h, (int)(k * 1000.0f));
	h = pace_int(h, (int)tint);
	h = pace_int(h, theme_index());
	h = pace_int(h, (int)replug_shown());
	return h;
}

/* Something on screen whose every frame counts, whatever the screen: the
 * panel opening or closing, the volume bar, a pop-up, art coming in, or a
 * button down */
bool pace_busy(const app *a)
{
	int b;

	for (b = 0; b < IN_COUNT; b++)
		if (a->in.pressed[b] || a->in.down[b]) return true;
	return g_pframe.moving || plat_osd_until() != REDRAW_NEVER || texload_ready() ||
	       g_songpop.on || g_heatpop.on;
}

/* The memory's slower clock, said once a change, for whichever screen is up:
 * the shelf's loop and the panels' share it (plat_ram_idle) */
static bool g_ram_idle;

void ram_idle_set(bool idle)
{
	if (idle == g_ram_idle) return;
	g_ram_idle = idle;
	plat_ram_idle(idle);
}

/* Zyper's pose beside the heading, as a frame shows it: a quarter unit, a
 * quarter degree, a fiftieth of a blink or of his tongue. A breath moves
 * him 2 units in 1.2 s, so a still menu draws a few frames a second for
 * it; a hop or a blink changes it every frame, and draws every frame. */
static unsigned zyper_key(const zr_rig *z, unsigned t)
{
	zr_pose p = zr_pose_at(z, t);
	unsigned h = 977u;

	h = pace_int(h, (int)lroundf(p.tilt * 4.0f));
	h = pace_int(h, (int)lroundf(p.tongue * 50.0f));
	h = pace_int(h, (int)lroundf(p.ears * 4.0f));
	h = pace_int(h, (int)lroundf(p.blink * 50.0f));
	h = pace_int(h, (int)lroundf(p.dy * 4.0f));
	h = pace_int(h, (int)lroundf(p.squash * 200.0f));
	return pace_int(h, (int)lroundf(p.mouth * 50.0f));
}

static menu_exit menu_run_body(app *a, const menu_style *st,
                               menu_build_fn build, menu_key_fn on_key,
                               void *ctx, int lv)
{
	menu_row rows[MENU_RUN_ROWS];
	const char *heading = NULL;
	int sel = st->start_sel, n = 0, b;
	float k;
	/* A menu over the shelf draws when its picture changes (src/pace.h).
	 * One over a game's paused frame or Now Playing (st->backdrop) draws
	 * every pass, as before: its backdrop is the screen's own. */
	pace pc;

	pace_init(&pc, plat_now_ms());
	/* Zyper's moves, in the menu that has him, on this loop's clock: it
	 * draws every pass, so the breath and the blinks need nothing more */
	zr_rig zy, *z = st->zyper ? &zy : NULL;

	if (z) {
		unsigned t = plat_now_ms();

		zr_init(z, t, t);
		zr_open(z, t);
	}
	while (a->running && !want_quit && !g_message_stopped) {
		if (z) zr_tick(z, plat_now_ms());
		n = build(ctx, rows, MENU_RUN_ROWS, &heading);
		if (n > MENU_RUN_ROWS) n = MENU_RUN_ROWS;
		if (n <= 0) return MENU_LEFT_GONE;

		/* The list is rebuilt every frame and can shrink under the cursor -
		 * a rescan finding fewer networks, a row going away. Clamp first,
		 * then land on something live. */
		if (sel >= n) sel = n - 1;
		if (sel < 0) sel = 0;
		if (!rows[sel].live && !st->visits_all) {
			sel = menu_step_sel(rows, n, sel, +1);
			if (!rows[sel].live) sel = menu_step_sel(rows, n, sel, -1);
		}
		if (st->sel_out) *st->sel_out = sel;

		plat_input_poll(&a->in);
		/* Where the sound goes, followed in the menus too: a headset connected
		 * from the Bluetooth screen, or a cable plugged in, only took the sound
		 * once the menus were left - seen 2026-09-26. It sends only on a change.
		 * And Muse is read here as well: its answers - where it now is, and the
		 * end of a track, which is when the next is sent - waited for the menus
		 * to be left, so the row read stale and an album stopped at the end of a
		 * song while a menu was open. #45. */
		muse_poll();
		aout_apply(false);
		/* Auto's clock, in every screen this runs - the menus, and the
		 * in-game menu over a paused game too. 7:00 or 19:00 arriving while
		 * one is open is drawn from the next frame, which this loop draws
		 * every pass, and the file Diatom and portkeys read follows. */
		tick_theme(a);
		if (a->in.quit_requested) { a->running = false; return MENU_LEFT_GONE; }
		/* px2-open's quit over the in-game menu: its game is stopping */
		if (g_ctl_close_menu) return MENU_LEFT_GONE;
		if (a->in.pressed[IN_POWER] || idle_due(a)) {
			if (!st->on_power) { power_off(a); return MENU_LEFT_GONE; }
			if (st->on_power(a, ctx) == MENU_DONE) return MENU_LEFT_GONE;
		}
		if (menu_leaving(a))
			return MENU_LEFT_BACK;
		/* A computer plugged in while a menu is open: the menus close back
		 * to the shelf, whose loop asks Computer Connected at once
		 * (2026-11-12: the question waited, unseen, until the player closed
		 * MENU by hand). Not over a game's or a port's menu: a game is
		 * never interrupted, and is asked about back on the shelf. */
		if (!g_in_game_menu && !g_port_menu && usblink_wants_ask(db_dev(), plat_now_ms())) {
			fprintf(stderr, "usb: a computer on the cable with a menu open: back to the shelf to ask\n");
			g_menu_closing = true;
			return MENU_LEFT_BACK;
		}
		/* The in-game menu too: its game is paused underneath, and Muse
		 * borrows its backdrop and its power rule for as long as it is up.
		 * Rebuilt on the way back, because anything may have changed while
		 * Muse covered it - a Bluetooth headset connecting, for one. */
		/* Not over a port's: MP4 is stopped for a port (launch.sh), and
		 * this process starts none for its menu */
		if (a->in.pressed[IN_SELECT] && !g_port_menu) {
			if (muse_open(a, st, ctx)) return MENU_LEFT_GONE;
			continue;
		}

		/* A move is a press too: it drops what Zyper had booked */
		/* Every row a stop where the screen says so (visits_all): stepped
		 * as a plain list, round on a press */
		if (in_repeat(&a->in, IN_UP)) {
			sel = st->visits_all ? list_step_heard(a, IN_UP, n, sel)
			                     : menu_step_heard(a, rows, n, sel, -1, IN_UP);
			if (z) zr_press(z, plat_now_ms());
		}
		if (in_repeat(&a->in, IN_DOWN)) {
			sel = st->visits_all ? list_step_heard(a, IN_DOWN, n, sel)
			                     : menu_step_heard(a, rows, n, sel, +1, IN_DOWN);
			if (z) zr_press(z, plat_now_ms());
		}

		/* Volume and brightness keep working here, as they do everywhere.
		 *
		 * They were missed when this runner was written, so the Wi-Fi screen
		 * spent a day as the one screen in the launcher where the volume keys
		 * did nothing. That is the exact failure this runner exists to make
		 * impossible, arriving by the one route it could still come from: the
		 * runner forgetting, rather than a screen forgetting. Every other loop
		 * in this file carries these four lines by hand. */
		if (in_repeat(&a->in, IN_VOLUP))    plat_volume_nudge(+1);
		if (in_repeat(&a->in, IN_VOLDN))    plat_volume_nudge(-1);
		if (in_repeat(&a->in, IN_BRIGHTUP)) plat_brightness_nudge(+1);
		if (in_repeat(&a->in, IN_BRIGHTDN)) plat_brightness_nudge(-1);

		/* Left and right reach the screen on REPEAT, not just on press, so a
		 * row whose value cycles can be held down. Everything else is edge
		 * triggered: holding A on a row that opens a screen should open it
		 * once. Sent before the press loop below, which skips them. */
		if (on_key && in_repeat(&a->in, IN_LEFT))
			if (menu_side_key(a, z, build, on_key, ctx, rows, sel,
			                  menu_sound_at(st->sound, n, sel), IN_LEFT) == MENU_DONE)
				return MENU_LEFT_SCREEN;
		if (on_key && in_repeat(&a->in, IN_RIGHT))
			if (menu_side_key(a, z, build, on_key, ctx, rows, sel,
			                  menu_sound_at(st->sound, n, sel), IN_RIGHT) == MENU_DONE)
				return MENU_LEFT_SCREEN;
		if (!a->running || want_quit || g_message_stopped) return MENU_LEFT_GONE;
		/* A step can move the cursor too: a Controls row whose change put a
		 * warning above it stays the row under it (menu_cursor_to) */
		if (g_menu_sel_to >= 0) {
			sel = g_menu_sel_to < n ? g_menu_sel_to : n - 1;
			g_menu_sel_to = -1;
		}

		/* Everything the runner did not consume goes to the screen, so a
		 * screen-specific key needs no runner change to exist. */
		for (b = 0; b < IN_COUNT; b++) {
			if (!a->in.pressed[b]) continue;
			if (b == IN_LEFT || b == IN_RIGHT) continue;
			if (b == IN_UP || b == IN_DOWN || b == IN_BACK || b == IN_MENU
			    || b == IN_POWER || b == IN_VOLUP || b == IN_VOLDN
			    || b == IN_BRIGHTUP || b == IN_BRIGHTDN) continue;
			/* A, answered before the screen has it, since what it opens
			 * runs its own loop: accept, or deny on a row that can do
			 * nothing, or nothing with A; a row that plays its own is
			 * left to its screen. */
			if (b == IN_ACCEPT && !(menu_sound_at(st->sound, n, sel) & MENU_SND_OWN))
				uisound_play(on_key && (rows[sel].live || st->visits_all) &&
				             !(menu_sound_at(st->sound, n, sel) & MENU_SND_NO_A)
				             ? DS_SOUND_ACCEPT : DS_SOUND_DENY);
			if (on_key && menu_send(a, z, on_key, ctx, (in_button)b, sel) == MENU_DONE)
				return MENU_LEFT_SCREEN;
			/* The handler moved the cursor: MENU's tab changed under it */
			if (g_menu_sel_to >= 0) {
				sel = g_menu_sel_to;
				g_menu_sel_to = -1;
			}
			if (!a->running || want_quit || g_message_stopped) return MENU_LEFT_GONE;
			/* The screen it opened was left with MENU: out of this one too,
			 * without drawing it again first. */
			if (g_menu_closing) return MENU_LEFT_BACK;
			/* A handler may have run a modal and changed everything under
			 * us; rebuild before drawing rather than drawing stale rows. */
			break;
		}

		if (!panel_frame(a, lv, &k)) continue;
		tick_tint(a);
		if (!st->backdrop) {
			unsigned nowp = plat_now_ms();
			unsigned key;
			/* usbrole's state, followed every pass, as every frame's
			 * overlay followed it while frames came every pass */
			power_follow();
			key = pace_rows(heading, rows, n, sel, k, a->tint);
			/* Zyper: his pose now, and every frame while the next one's
			 * differs (a hop, a blink, a breath's step), through the
			 * redraw clock: his breath is no press, and leaves the
			 * memory's slow clock alone, as the home's sprite does;
			 * woken for his next blink or booked move */
			bool busy = pace_busy(a);
			unsigned cap = 80;

			if (z && zyper_key(z, nowp) != zyper_key(z, nowp + 17)) redraw_at(nowp);

			if (z) {
				key = pace_int(key, (int)zyper_key(z, nowp));
				if ((int)(z->next_blink - nowp) > 0 && z->next_blink - nowp < cap)
					cap = z->next_blink - nowp;
				if (z->booked && (int)(z->booked_at - nowp) > 0 && z->booked_at - nowp < cap)
					cap = z->booked_at - nowp;
			}
			if (!pace_due(&pc, key, nowp, g_redraw_at, busy)) {
				ram_idle_set(pace_ram_idle(&pc, nowp));
				plat_input_wait(pace_rest(&pc, nowp, g_redraw_at, cap));
				continue;
			}
			ram_idle_set(false);
			g_redraw_at = REDRAW_NEVER;
			pace_drawn(&pc, key, nowp);
			draw_shelf(a);
			ui_scrim_k(a->r, 0, k);
		} else {
			st->backdrop(a, ctx, k);
		}
		g_menu_zyper = z;
		g_menu_zyper_at = plat_now_ms();
		menu_draw_styled(a, st, heading, rows, n, sel);
		g_menu_zyper = NULL;
		plat_draw_osd(a->r);
		plat_present();
		ui_frame_rest(8);
	}
	return MENU_LEFT_GONE;
}

/* One screen, one loop. Owns polling, power, idle, quit, back, volume,
 * brightness, the cursor and the whole frame, so that no screen can forget any
 * of them - which every menu defect this project has had came down to.
 *
 * The flush is here rather than in the body: the press that opens a screen must
 * not also close it, and the press that closes one must not arrive again at
 * whatever is underneath. This file does that by hand about twenty times, and
 * the body has nine ways out, so putting it at each of them would eventually
 * mean missing one. */
menu_exit menu_run(app *a, const menu_style *st,
                   menu_build_fn build, menu_key_fn on_key, void *ctx)
{
	menu_exit how;
	int lv;
	const menu_style *previous = g_message_over;
	void *previous_ctx = g_message_ctx;

	if (!previous) g_message_stopped = false;
	if (!previous || st->backdrop || st->on_power) {
		g_message_over = st;
		g_message_ctx = ctx;
	}

	plat_input_flush();
	memset(&a->in, 0, sizeof a->in);
	/* A cursor move is for the runner it was asked of: none left over from
	 * a screen that ended on the handler's press reaches this one, or the
	 * one under it */
	g_menu_sel_to = -1;

	/* And it opens and closes (panel_open): quit and power, and nothing left
	 * to show, leave nothing to see go. */
	lv = panel_open();
	how = menu_run_body(a, st, build, on_key, ctx, lv);
	g_menu_sel_to = -1;
	panel_close(a, lv, how == MENU_LEFT_GONE);

	plat_input_flush();
	memset(&a->in, 0, sizeof a->in);
	g_message_over = previous;
	g_message_ctx = previous_ctx;
	return how;
}

/* A result, unlike wait_panel's single frame before a blocking operation.
 * Its hold starts after the panel arrives, on the same wall clock. Its
 * close completes here, so a result cannot hand queued keys to its parent.
 * The parent's own arrival remains the parent's to draw. */
bool timed_message(app *a, const char *heading, const menu_row *rows,
                   int n, int fixed_w, unsigned accent)
{
	message_time clock;
	message_phase phase;
	bool closed = false, power = a->in.pressed[IN_POWER];
	bool quit = a->in.quit_requested;
	int lv = panel_open();
	unsigned now = anim_now();
	mm_frame f = mm_at(lv, now);
	static const unsigned colors[1] = { VCOL_MESSAGE };

	if (!g_message_over) g_message_stopped = false;
	message_begin(&clock, now, n, f.moving
	              ? DS_MS_QUICK + (f.before.alpha > 0.0f ? DS_MS_EXIT : 0) : 0);
	/* The opening press is already consumed. Poll the queue rather than
	 * flushing it: POWER or quit during a blocking call must still act. */
	memset(&a->in, 0, sizeof a->in);
	snap_record(g_snap_next, PANEL_MENU, heading, rows, n, -1, fixed_w,
	            accent, n == 1 ? colors : NULL, false, 0, NULL);
	for (;;) {
		bool press = false, back;
		int b;
		float k;

		plat_input_poll(&a->in);
		muse_poll();
		aout_apply(false);
		tick_theme(a);
		now = anim_now();
		quit = quit || a->in.quit_requested || want_quit || !a->running;
		power = power || a->in.pressed[IN_POWER] || idle_due(a);
		for (b = 0; b < IN_COUNT; b++)
			if (a->in.pressed[b]) { press = true; break; }
		back = menu_leaving(a);
		phase = message_step(&clock, now, press, back, power, quit);
		if (phase == MESSAGE_QUIT || phase == MESSAGE_POWER) {
			g_message_stopped = true;
			if (phase == MESSAGE_QUIT) a->running = false;
			else if (g_message_over && g_message_over->on_power)
				g_message_over->on_power(a, g_message_ctx);
			else power_off(a);
			if (!closed) panel_close(a, lv, true);
			mm_leaving_cut();
			break;
		}
		if (in_repeat(&a->in, IN_VOLUP))    plat_volume_nudge(+1);
		if (in_repeat(&a->in, IN_VOLDN))    plat_volume_nudge(-1);
		if (in_repeat(&a->in, IN_BRIGHTUP)) plat_brightness_nudge(+1);
		if (in_repeat(&a->in, IN_BRIGHTDN)) plat_brightness_nudge(-1);
		if (phase != MESSAGE_SHOW && !closed) {
			if (back) panel_settle(lv);
			panel_close_at(a, lv, false, clock.close_at);
			closed = true;
		}
		/* MENU leaves every parent without another frame. The outermost
		 * owner draws this panel's exit once, as for every other submenu. */
		if (g_menu_closing) break;
		if (phase == MESSAGE_DONE) {
			/* A press during the exit ends it now. Do not leave a second
			 * copy of that exit for the parent to draw after returning. */
			if (press) { mm_press(mm_depth()); mm_leaving_cut(); }
			break;
		}
		if (closed) {
			f = mm_at(mm_depth(), now);
			if (!(f.before.alpha > 0.0f)) f = mm_leaving(now);
			k = f.scrim;
		} else {
			if (!panel_frame(a, lv, &k)) continue;
		}
		tick_tint(a);
		if (g_message_over && g_message_over->backdrop)
			g_message_over->backdrop(a, g_message_ctx, k);
		else { draw_shelf(a); ui_scrim_k(a->r, 0, k); }
		if (closed) snap_draw(a, f.before);
		else menu_draw_ex(a, heading, rows, n, -1, fixed_w, accent,
		                  n == 1 ? colors : NULL, false, 0);
		plat_draw_osd(a->r);
		plat_present();
		ui_frame_rest(8);
	}
	/* All queued events went through the poll above, including on the last
	 * frame. Clear held/repeat state too; do not blindly drain a new POWER. */
	memset(&a->in, 0, sizeof a->in);
	/* The owner may return immediately too (Box Art). The result's exit
	 * already finished; its saved panel must not leave a second time. */
	if (!g_menu_closing && g_snap->level == lv) g_snap->ok = false;
	return !g_message_stopped && !g_menu_closing;
}

bool result_message(app *a, const char *heading, const char *text)
{
	menu_row row = { text, NULL, false };
	int w = menu_std_width(a);

	/* A message wider than the panel goes onto lines, balanced, rather than
	 * sliding where a word is cut ("neogeo is a BIOS other games need. It
	 * stays.", "PortMaster runs every port. It stays." in French) */
	if (ui_text_width(ui_font(UI_F_MENU), text) > w) {
		char lines[3][LB_LINE];
		menu_row rows[3];
		int n = ui_balance_lines(ui_font(UI_F_MENU), text, w, 3, lines), i;

		for (i = 0; i < n; i++) rows[i] = (menu_row){ lines[i], NULL, false };
		if (n > 1) return timed_message(a, heading, rows, n, w, MENU_ZYPEROS);
	}
	return timed_message(a, heading, &row, 1, 0, MENU_ZYPEROS);
}

/* The shelf under the keyboard and the sign-in messages. kb_prompt's loop is
 * src/keyboard.c's, which knows no app and so no theme_changed; it calls this
 * every pass, and Auto's clock is asked here for it. */
typedef struct {
	app *a;
	int level;
	bool closed, shown;
} keyboard_transition;

static kb_pose keyboard_panel(void *ctx, kb_stage stage, bool pressed)
{
	keyboard_transition *tr = ctx;
	app *a = tr->a;
	const kb_pose gone = {1, 0, false};
	mm_frame f;
	unsigned now = anim_now();

	if (stage == KB_PANEL_OPEN) {
		tr->level = panel_open();
		return gone;
	}
	if (stage == KB_PANEL_LEAVE || stage == KB_PANEL_CUT) {
		if (!tr->closed) {
			int shown = stage == KB_PANEL_CUT ? 0 :
			            tr->shown ? tr->level : snap_shown();

			mm_close(tr->level, now, shown, false);
			g_snap_next->ok = false;
			tr->closed = true;
		}
		if (stage == KB_PANEL_CUT) {
			mm_press(mm_depth());
			mm_leaving_cut();
			tr->shown = false;
		}
		return gone;
	}
	if (stage == KB_PANEL_FINISH) {
		/* No keyboard text or image ever enters g_snap. Its private
		 * textures die in kb_prompt before this callback returns. */
		mm_leaving_cut();
		tr->shown = false;
		return gone;
	}
	if (tr->closed) {
		if (pressed) {
			mm_press(mm_depth());
			mm_leaving_cut();
			tr->shown = false;
		}
		f = mm_at(mm_depth(), now);
		if (!(f.before.alpha > 0.0f)) f = mm_leaving(now);
		if (!tr->shown || !(f.before.alpha > 0.0f)) return gone;
	} else {
		if (pressed) mm_press(tr->level);
		f = mm_at(tr->level, now);
	}

	ui_sounds_follow();
	tick_theme(a);
	draw_shelf(a);
	ui_scrim_k(a->r, 0, f.scrim);
	if (tr->closed)
		return (kb_pose){f.before.scale, f.before.alpha, true};
	if (f.before.alpha > 0.0f) snap_draw(a, f.before);
	if (f.panel.alpha > 0.0f) tr->shown = true;
	return (kb_pose){f.panel.scale, f.panel.alpha, f.moving};
}

static bool keyboard_idle(void *ctx)
{
	return idle_due(((keyboard_transition *)ctx)->a);
}

kb_result keyboard_prompt(app *a, const char *title, char *buf, int cap)
{
	keyboard_transition tr = { .a = a };
	kb_result result = kb_prompt(a->r, &a->in, title, buf, cap,
	                              keyboard_panel, keyboard_idle, &tr);
	if (a->in.quit_requested) a->running = false;
	memset(&a->in, 0, sizeof a->in);
	return result;
}

static menu_result wifi_key(app *a, void *ctx, in_button key, int sel)
{
	wifi_ui *w = ctx;
	char status[96], ssid[WIFI_SSID_MAX], ip[64];
	int k = sel - 1;                    /* row 0 is the switch */
	bool net_row = sel >= 1 && sel <= w->n;

	/* Y scans, with the radio on; X forgets, on a network that was joined.
	 * Anywhere else they can do nothing, and say so. */
	if (key == IN_Y) uisound_play(w->on ? DS_SOUND_ACCEPT : DS_SOUND_DENY);
	if (key == IN_X)
		uisound_play(w->on && net_row && w->nets[k].known ? DS_SOUND_ACCEPT
		                                                  : DS_SOUND_DENY);
	if (key == IN_Y && w->on) { wifi_begin_scan(w); return MENU_STAY; }

	/* Forget a saved network. Only saved rows offer it - there is nothing to
	 * forget about one the device has never joined, and a button that silently
	 * does nothing on some rows is the same defect as a menu entry wired to
	 * nothing.
	 *
	 * Forgetting the CONNECTED network is allowed, deliberately. Refusing would
	 * be safer - it drops the device off the LAN, taking Wi-Fi File Transfer and ssh
	 * with it, and a handheld has no keyboard to climb back out with. But
	 * wanting to forget the network you are on is reasonable, and the honest
	 * place to say what it costs is the confirm. */
	if (key == IN_X && w->on && net_row && w->nets[k].known) {
		char cur[WIFI_SSID_MAX], msg[160];
		bool joined = wifi_status(cur, sizeof cur, NULL, 0) == WIFI_CONNECTED
		              && !strcmp(cur, w->nets[k].ssid);

		if (joined)
			snprintf(msg, sizeof msg, T(S_WIFI_FORGET_JOINED), w->nets[k].ssid);
		else
			snprintf(msg, sizeof msg, T(S_WIFI_FORGET_ASK), w->nets[k].ssid);

		if (confirm_panel(a, T(S_MENU_WIFI), msg, T(S_WIFI_FORGET))) {
			snprintf(status, sizeof status,
			         wifi_forget(w->nets[k].ssid) ? T(S_WIFI_FORGOT)
			                                      : T(S_WIFI_FORGET_FAILED),
			         w->nets[k].ssid);
			if (!result_message(a, T(S_MENU_WIFI), status)) return MENU_STAY;
			if (w->on) wifi_begin_scan(w);
		}
		return MENU_STAY;
	}

	if (key != IN_ACCEPT) return MENU_STAY;

	/* The switch. Saved on every change rather than on the way out: the way out
	 * of a handheld is often the power button. */
	if (sel == 0) {
		if (w->on) {
			wifi_down();
			wifi_pref_save(false);
			w->on = false;
			w->n = 0;
		} else {
			wait_panel(a, T(S_MENU_WIFI), T(S_WIFI_TURNING_ON));
			w->on = wifi_up();
			wifi_pref_save(w->on);
			if (!w->on) {
				if (!result_message(a, T(S_MENU_WIFI), T(S_WIFI_DID_NOT_COME_UP)))
					return MENU_STAY;
			}
			if (w->on) wifi_begin_scan(w);
		}
		return MENU_STAY;
	}

	if (net_row) {
		char psk[80] = "";
		bool ok;

		/* A saved network already has its passphrase in wpa_supplicant.conf,
		 * so asking again would be asking the user to retype something the
		 * device is holding. An open network has none to ask for. */
		if (w->nets[k].secured && !w->nets[k].known) {
			kb_result kr = keyboard_prompt(a, w->nets[k].ssid, psk, (int)sizeof psk);
			if (kr == KB_POWER) { power_off(a); return MENU_DONE; }
			if (kr != KB_ACCEPT) return MENU_STAY;
		}

		wait_panel(a, T(S_MENU_WIFI), T(S_WIFI_CONNECTING));
		ok = wifi_connect(w->nets[k].ssid, psk[0] ? psk : NULL);
		/* Wiped as soon as it has been handed over. It still exists in the
		 * supplicant's config, which is the point, but there is no reason for
		 * a copy to sit in the launcher's stack afterwards. */
		memset(psk, 0, sizeof psk);

		if (ok) {
			/* Connecting is turning it on, whatever the switch said. */
			wifi_pref_save(true);
			w->on = true;
			wifi_status(ssid, sizeof ssid, ip, sizeof ip);
			snprintf(status, sizeof status, T(S_WIFI_CONNECTED_TO), ssid);
		} else {
			snprintf(status, sizeof status, T(S_WIFI_CONNECT_FAILED), w->nets[k].ssid);
		}
		if (!result_message(a, T(S_MENU_WIFI), status)) return MENU_STAY;
		/* Out on success, because the job is done and the menu row behind this
		 * screen already names the network - staying in the list makes you back
		 * out by hand to see the result. On failure stay, because the next
		 * thing wanted is another try or another network, and both are here. */
		if (ok) return MENU_DONE;
		wifi_begin_scan(w);
	}
	return MENU_STAY;
}

void wifi_screen(app *a)
{
	wifi_ui w = { 0 };

	/* Entering does not switch the radio on. It used to, which made the screen
	 * impossible to leave in the off state: you opened it to turn wifi OFF and
	 * the act of opening it turned wifi on. */
	w.on = wifi_status(NULL, 0, NULL, 0) != WIFI_OFF;
	if (w.on) wifi_begin_scan(&w);

	menu_run(a, &(menu_style){ .accent = MENU_ZYPEROS,
	                           .fixed_w = menu_std_width(a) },
	         wifi_build, wifi_key, &w);
}


/* The button hints under a question's tiles (keyhints.h), packed into one
 * string so they travel as the legend row and in the panel's snapshot:
 * Left/Right move, A chooses, and B's own answer */
const char *tiles_hints(const char *b_word)
{
	static char packed[4][256];
	static int turn;
	key_hint h[3] = {
		{ KEYHINT_LEFT_RIGHT, T(S_KEYHINT_MOVE) },
		{ "A", T(S_KEYHINT_CHOOSE) },
		{ "B", b_word },
	};
	char *out = packed[turn++ % 4];

	keyhints_pack(h, 3, out, sizeof packed[0]);
	return out;
}

static const char *tiles_hints_charge_only(void) { return tiles_hints(T(S_KEYHINT_CHARGE_ONLY)); }

/* A list's (choice_list_panel): A chooses and B's own answer; Up and Down
 * move, as in every list, so no hint says so */
const char *list_hints(const char *b_word)
{
	static char packed[256];
	key_hint h[2] = { { "A", T(S_KEYHINT_CHOOSE) }, { "B", b_word } };

	keyhints_pack(h, 2, packed, sizeof packed);
	return packed;
}

/* The tiles' button hints shown: off while a question that came by itself
 * takes no button yet */
bool g_tiles_hints = true;

/* A question that came by itself, this pass (usbpower_guard): a button
 * down when it appeared, or pressed in its first half second, counts only
 * once let go and pressed again; POWER always. Its hints from then. */
void ask_settle(app *a, usbpower_guard *g)
{
	unsigned down = 0, pressed = 0, ok;
	int b;

	for (b = 0; b < IN_COUNT && b < 32; b++) {
		if (a->in.down[b]) down |= 1u << b;
		if (a->in.pressed[b]) pressed |= 1u << b;
	}
	ok = usbpower_guard_pass(g, plat_now_ms(), down, pressed);
	g_tiles_hints = usbpower_guard_hints(g, plat_now_ms());
	for (b = 0; b < IN_COUNT && b < 32; b++) {
		if (b == IN_POWER) continue;
		if (!(ok & (1u << b))) a->in.pressed[b] = false;
		/* Held over: no repeat from it either */
		if (g->held & (1u << b)) a->in.down[b] = false;
	}
}

void ask_guard_start(app *a, usbpower_guard *g)
{
	unsigned down = 0;
	int b;

	for (b = 0; b < IN_COUNT && b < 32; b++)
		if (a->in.down[b] && b != IN_POWER) down |= 1u << b;
	usbpower_guard_start(g, plat_now_ms(), down);
}

/* A question in tiles at `pose` (src/ds/tiles.c), through the menus' panel
 * pass: its style from its kind (tiles_show), its legend the Charge Only
 * hints where no note gives one (Computer Connected's), its hints off while a
 * question that came by itself takes no button yet (ask_settle) */
static bool tiles_pass_begin(void *ctx) { return panel_pass_begin(ctx); }
static void tiles_pass_end(void *ctx, const SDL_Rect *panel, mm_pose pose) { panel_pass_end(ctx, panel, pose); }

void tiles_panel(app *a, int kind, const char *heading, const menu_row *tiles,
                 const int *marks, int n, int sel, mm_pose pose)
{
	const ds_pass pass = { panel_scaled, tiles_pass_begin, tiles_pass_end, a };

	ds_tiles_draw(a->r, &(ds_tiles){
		.style = kind == PANEL_TILES_GRID ? DS_TILES_GRID : kind == PANEL_LIST ? DS_TILES_LIST : DS_TILES_ROW,
		.heading = heading, .rows = tiles, .n = n, .marks = marks, .sel = sel,
		.legend = tiles_hints_charge_only(), .hints = g_tiles_hints }, pose, &pass);
}



/* Refresh in place, including copies on Favorites. Sort only after saving
 * each shelf's selected file and owner; titles are never selection keys. */
void refresh_game_titles(app *a)
{
	int si, i;

	for (si = 0; si < a->sys.count; si++) {
		sysview *v = &a->view[si];
		char file[LIB_PATH];
		int owner, selected;
		bool changed = false;

		if (is_muse(&a->sys.systems[si]) || v->list.count <= 0) continue;
		owner = shelf_owner(a, si, v->cursor);
		snprintf(file, sizeof file, "%s", v->list.items[v->cursor].file);
		/* A folder's name is its own (src/shelfdir.h); every game of a
		 * shelf with folders, and the shelf's copies of its folder's */
		for (i = 0; v->all && i < v->all->count; i++)
			lib_refresh_title(db_lib(), a->sys.systems[si].folder, &v->all->items[i]);
		for (i = 0; i < v->list.count; i++) {
			int o = shelf_owner(a, si, i);
			if (v->list.items[i].folder) continue;
			if (lib_refresh_title(db_lib(), a->sys.systems[o].folder, &v->list.items[i]))
				changed = true;
		}
		if (!changed) continue;
		free_view_textures(v);
		sort_shelf(a, si);
		selected = lib_find_entry(&v->list, v->owner, owner, file);
		if (selected >= 0) v->cursor = selected;
		cf_reset(&v->cf, v->cursor);
	}
}

/* MENU > Side Lights: a switch for each part of what the lights show
 * (src/sidelights.h, sys_menu_lights_rows). Each change goes to the card at
 * once and the gauge reads it again, so the lights follow the row. The row
 * under the cursor plays its part on the lights, On or Off, while the
 * cursor stays on it (side_lights_preview_step, each pass of the menu). */
typedef struct {
	unsigned char sound[LP_COUNT];
	int sel;                      /* the menu's cursor (menu_style.sel_out) */
	side_lights_preview pv;
} lights_page;

static int lights_build(void *ctx, menu_row *rows, int max, const char **heading)
{
	lights_page *pg = ctx;
	side_light_part ids[LP_COUNT];
	char line[48];
	int n, i;

	(void)max;
	*heading = T(S_PANEL_SIDE_LIGHTS);
	n = sys_menu_lights_rows(plat_side_lights(), rows, ids);
	for (i = 0; i < n; i++) pg->sound[i] = MENU_SND_RING;
	if (n > 0 && side_lights_preview_step(&pg->pv, ids[pg->sel < 0 ? 0 : pg->sel >= n ? n - 1 : pg->sel],
	                                      plat_now_ms(), line, sizeof line))
		plat_side_lights_say(line);
	return n;
}

static menu_result lights_key(app *a, void *ctx, in_button key, int sel)
{
	menu_row rows[LP_COUNT];
	side_light_part ids[LP_COUNT];
	side_lights on = plat_side_lights();
	int n = sys_menu_lights_rows(on, rows, ids);

	(void)a; (void)ctx;
	if (key != IN_LEFT && key != IN_RIGHT && key != IN_ACCEPT) return MENU_STAY;
	if (sel < 0 || sel >= n) return MENU_STAY;
	plat_side_lights_set(side_lights_flip(on, ids[sel]));
	return MENU_STAY;
}

/* A language on from the next frame: its words (src/i18n.h) and its faces
 * (src/ui.h), every panel measured again for them, and kept: the setting,
 * and the boot screen's words (tick_boot_theme) */
void language_apply(app *a, lang l, bool keep)
{
	i18n_set(l);
	ui_fonts_for(l);
	ui_text_memo_clear();
	a->menu_w = 0;
	/* The one shelf whose name is a word and not a system's: Favorites,
	 * named when the shelves were built (src/shelfbuild.c) */
	if (a->sys.count > 0 && !strcmp(a->sys.systems[0].tag, "FAV"))
		snprintf(a->sys.systems[0].name, sizeof a->sys.systems[0].name, "%s",
		         T(S_LIBRARY_FAVORITES));
	if (!keep) return;
	db_set_str(db_dev(), "language", lang_code(l));
	boot_theme_changed();
	fprintf(stderr, "language: %s\n", lang_code(l));
}

/* MENU > Language (DESIGN.md 11.30; my pick in the languages lab,
 * 2026-11-07): the five, each in its own words and its own face, a tick on
 * the one that is on and the cursor on it. A chooses it and goes back to the
 * menu, which is drawn in it from the next frame; B leaves it as it was. */
static int language_build(void *ctx, menu_row *rows, int max, const char **heading)
{
	unsigned char *sound = ctx;
	int i, on = (int)i18n_lang();

	*heading = T(S_MENU_LANGUAGE);
	for (i = 0; i < LANG_COUNT && i < max; i++) {
		/* Each name in its own face: ui_text draws Chinese in the Chinese
		 * face whatever is on (src/ui.h) */
		rows[i] = (menu_row){ lang_name((lang)i), i == on ? MENU_TICK : NULL, true };
		sound[i] = 0;
	}
	return i;
}

static menu_result language_key(app *a, void *ctx, in_button key, int sel)
{
	(void)ctx;
	if (key != IN_ACCEPT || sel < 0 || sel >= LANG_COUNT) return MENU_STAY;
	if ((lang)sel != i18n_lang()) language_apply(a, (lang)sel, true);
	return MENU_DONE;
}

void language_screen(app *a)
{
	static unsigned char sound[LANG_COUNT];

	menu_run(a, &(menu_style){ .accent = MENU_ZYPEROS, .fixed_w = menu_std_width(a),
	                           .sound = sound, .start_sel = (int)i18n_lang() },
	         language_build, language_key, sound);
}

void lights_screen(app *a)
{
	lights_page pg = { .sel = 0 };
	char line[48];

	menu_run(a, &(menu_style){ .accent = MENU_ZYPEROS, .fixed_w = menu_std_width(a), .sound = pg.sound,
	                           .sel_out = &pg.sel },
	         lights_build, lights_key, &pg);
	/* The lights back to what they show on their own */
	if (side_lights_preview_end(&pg.pv, line, sizeof line)) plat_side_lights_say(line);
}

/* The approved Details layout: system information, then storage and the
 * battery controller's capacity estimate. The same rows serve --about shots. */
int menu_measure(const menu_row *rows, int n, const char *heading);

/* The widest row of one built menu. */
int menu_measure(const menu_row *rows, int n, const char *heading)
{
	TTF_Font *fm = ui_font(UI_F_MENU), *fh = ui_font(UI_F_LABEL);
	int gap = menu_row_h(), w = 0, i;
	bool two_col = false;

	/* The same three row kinds menu_draw knows about. This function is a
	 * second copy of that measuring pass and the two must agree, or a panel is
	 * sized by one rule and drawn by another. */
	for (i = 0; i < n; i++)
		if (rows[i].value && !ROW_IS_NOTE(rows[i]) && !ROW_IS_HR(rows[i])) two_col = true;
	for (i = 0; i < n; i++) {
		int rw;

		if (!rows[i].label) continue;          /* a rule measures nothing */
		rw = ui_text_width(fm, rows[i].label);
		if (two_col && rows[i].value && !ROW_IS_NOTE(rows[i]))
			rw += gap + menu_value_w(fm, rows[i].value);
		if (rw > w) w = rw;
	}
	if (heading) {
		int hw = ui_text_width(fh, heading);
		if (hw > w) w = hw;
	}
	return w;
}

/* One width for both shelf menus, every system, and every value their rows can
 * cycle to. The panel is a frame the lists sit inside rather than something
 * that resizes to whatever is selected: without this, cycling display mode from
 * "Fill" to "Integer tall" widens the slab under the cursor, and walking from
 * the systems row into a system resizes it again.
 *
 * Measured across all of that rather than picked, so a longer label, another
 * system or another font widens the frame instead of overflowing it.
 * Cached because it is a few hundred text measurements and none of its inputs
 * change while a menu is open. */
/* Where a console shelf's menu has the row `which`, or -1: the enum's numbers
 * are the full list's, and a shelf's list is what sys_menu_system_rows makes
 * of it (no Box Art without a radio, Game Language only on the DS). */
static int sm_pos(const app *a, int sys, sm_row which)
{
	sys_ui dev = { .no_wifi = !plat_has_wifi(), .online = net_internet(NULL),
	               .dslang = dslang_row_shown(a, sys) ? "" : NULL,
	               .no_strength = !shader_has_strength(a->view[sys].shader) };
	sm_row ids[SM_ROWS];
	int n = sys_menu_system_rows(&dev, ids), k;

	for (k = 0; k < n; k++)
		if (ids[k] == which) return k;
	return -1;
}

static int menu_shelf_width(app *a)
{
	menu_row rows[MENU_MAX_ROWS];
	menu_bufs bufs;
	const char *heading;
	int w, n, i, k;

	if (a->menu_w) return a->menu_w;

	/* MENU's every tab, so the panel is one width on all of them, and on
	 * the tab with Shader every value of it, "per system" too, so stepping
	 * it does not resize the panel under it */
	{
		int t, at = g_tabs.at, mw;

		w = 0;
		for (t = 0; t < g_tabs.count; t++) {
			g_tabs.at = t;
			n = menu_build(a, SCREEN_SYSTEMS, a->sys_cursor, rows, &bufs, &heading);
			mw = menu_measure(rows, n, heading);
			if (mw > w) w = mw;
			for (k = 0; k < n; k++)
				if (rows[k].label == T(S_GAMEMENU_SHADER)) break;
			if (k < n) {
				const char *keep = rows[k].value;
				int j;

				for (j = 0; j <= SHADER_COUNT; j++) {
					if (j < SHADER_COUNT && !shader_offered(j)) continue;
					rows[k].value = j < SHADER_COUNT ? shader_label(j) : T(S_GAMEMENU_SHADER_MIXED);
					mw = menu_measure(rows, n, heading);
					if (mw > w) w = mw;
				}
				rows[k].value = keep;
				/* And Strength under it, there or not, at its widest */
				for (j = 0; j < 2 && n < MENU_MAX_ROWS; j++) {
					rows[n] = (menu_row){ T(S_GAMEMENU_STRENGTH),
					                      j ? "100%" : T(S_GAMEMENU_SHADER_MIXED), true };
					mw = menu_measure(rows, n + 1, heading);
					if (mw > w) w = mw;
				}
			}
		}
		g_tabs.at = at;
	}

	/* Every value that can be cycled on this menu, at its widest, so the
	 * panel does not resize under the row being cycled. Sort By joined
	 * Display Mode in that category the moment it stopped being a
	 * placeholder - "Recently Added" is 187px wider than "Name".
	 *
	 * Muse's menu and Favorites' are their own rows, Sort By second and no
	 * Display Mode, so the enum's row numbers are only the rest's. Written
	 * through them on Muse's, this measured Rescan Folder carrying a sort
	 * order, a row no menu has. */
	for (i = 0; i < a->sys.count; i++) {
		bool muse = is_muse(&a->sys.systems[i]);
		bool own  = muse || a->view[i].owner;
		int  srow = own ? 1 : sm_pos(a, i, SM_SORT), show = -1;
		int  drow = own ? -1 : sm_pos(a, i, SM_DISPLAY);
		int  hrow = own ? -1 : sm_pos(a, i, SM_SHADER);
		int  crow = own ? -1 : sm_pos(a, i, SM_CORE);
		int  lrow = own ? -1 : sm_pos(a, i, SM_DSLANG);
		int  vrow = muse ? -1 : a->view[i].owner ? 3 : sm_pos(a, i, SM_VIEW);

		n = menu_build(a, SCREEN_GAMES, i, rows, &bufs, &heading);
		if (muse) {
			sm_muse_row ids[SM_MUSE_ROWS];
			int nn = sys_menu_muse_rows(muse_rows_kind(), muse_kinds() > 1,
			                            !plat_has_wifi() && !net_internet(NULL), ids);

			for (k = 0; k < nn; k++) {
				if (ids[k] == SMM_SORT) srow = k;
				if (ids[k] == SMM_SHOW) show = k;
			}
			/* The longer of Show's two values, so turning it over does not
			 * change the menu's width under the cursor. */
			if (show >= 0) {
				int mw;

				rows[show].value = "Audiobooks";
				mw = menu_measure(rows, n, heading);
				if (mw > w) w = mw;
			}
		}
		for (k = 0; drow >= 0 && k < DMODE_COUNT; k++) {
			int mw;
			rows[drow].value = dmode_label(k);
			mw = menu_measure(rows, n, heading);
			if (mw > w) w = mw;
		}
		if (drow >= 0) rows[drow].value = dmode_label(0);
		/* And each shader, for the same reason */
		for (k = 0; hrow >= 0 && k < SHADER_COUNT; k++) {
			int mw;
			if (!shader_offered(k)) continue;
			rows[hrow].value = shader_label(k);
			mw = menu_measure(rows, n, heading);
			if (mw > w) w = mw;
		}
		if (hrow >= 0) rows[hrow].value = shader_label(0);
		/* And Strength under it, there or not */
		if (hrow >= 0 && n < MENU_MAX_ROWS) {
			int mw;
			rows[n] = (menu_row){ T(S_GAMEMENU_STRENGTH), "100%", true };
			mw = menu_measure(rows, n + 1, heading);
			if (mw > w) w = mw;
		}
		/* And each core a shelf can choose, for the same reason. */
		if (crow >= 0 && cfg_core_count(&a->sys.systems[i]) > 1) {
			char alt[CFG_STR], lb[CFG_STR];
			const char *keep = rows[crow].value;

			for (k = 0; cfg_core_at(&a->sys.systems[i], k, alt, sizeof alt); k++) {
				int mw;
				rows[crow].value = core_label(alt, lb, sizeof lb);
				mw = menu_measure(rows, n, heading);
				if (mw > w) w = mw;
			}
			rows[crow].value = keep;
		}
		/* And each language the Nintendo DS's game can speak, Auto too */
		if (lrow >= 0) {
			char lb[64];
			const char *keep = rows[lrow].value;

			for (k = 0; k < DSL_CHOICES; k++) {
				int mw;
				rows[lrow].value = dslang_label((dsl_choice)k, lb, sizeof lb);
				mw = menu_measure(rows, n, heading);
				if (mw > w) w = mw;
			}
			rows[lrow].value = keep;
		}
		/* View's two values, the longer kept */
		if (vrow >= 0 && vrow < n) {
			const char *keep = rows[vrow].value;
			int mw;

			rows[vrow].value = T(S_SYSMENU_VIEW_COVERS);
			if ((mw = menu_measure(rows, n, heading)) > w) w = mw;
			rows[vrow].value = T(S_SYSMENU_VIEW_LIST);
			if ((mw = menu_measure(rows, n, heading)) > w) w = mw;
			rows[vrow].value = keep;
		}
		for (k = 0; k < (muse ? ML_ORDERS : SORT_COUNT); k++) {
			int mw;
			rows[srow].value = muse ? ml_order_label((ml_order)k, false) : sort_label(k);
			mw = menu_measure(rows, n, heading);
			if (mw > w) w = mw;
			if (muse) {
				int kind;

				for (kind = ML_BOOKS; kind < ML_KINDS; kind++) {
					rows[srow].value = ml_order_label_kind((ml_order)k, (ml_kind)kind);
					mw = menu_measure(rows, n, heading);
					if (mw > w) w = mw;
				}
			}
		}
	}
	a->menu_w = w;
	return w;
}

/* The width every menu reached from the shelf uses.
 *
 * Menus sized themselves to their own rows, so the panel changed size on
 * nearly every screen. Measured on the device 2026-09-08: the ZyperOS menu drew
 * a 794px panel, Play Time 791, Bluetooth 737, Wi-Fi 640 - so opening Wi-Fi
 * from the menu shrank the panel by 154px, about 15% of the display, and grew
 * it again on the way out.
 *
 * Sizing to content was right while a label that did not fit was cut, because
 * cutting a name is a loss. A long label scrolls now, so a panel no longer has
 * to grow to hold one, and the trade is a little empty air in a narrow menu
 * against a panel that stops jumping.
 *
 * 800px of PANEL, stated rather than measured, so it does not drift when a
 * row's text changes. What menu_draw takes is the content width and it adds
 * menu_pad() on each side, so the pad comes off here - by calling the same
 * function menu_draw calls, because the pad follows the menu font and a
 * hardcoded number is wrong the day the font changes.
 *
 * The floor is not decoration. menu_shelf_width is the widest row in the
 * shelf menus, and a panel narrower than that sets one of them scrolling. In
 * Josefin Sans it was "Bluetooth" and the paired headset's name - a row that
 * grows with a device nobody here chose the name of - measured on the device
 * at 699, and the panel drew at 800. Lexend sets words about 8% wider, and
 * "Audio Output" and "auto (speaker)" alone are 741 (700 in Josefin,
 * measured 2026-11-04) against the 692 of content 800 leaves, so the shelf
 * menus draw about 850 wide: within DESIGN.md 5's rule that content stops at
 * the screen less two insets and two paddings (868), and not the 800 it
 * names. max() is the whole reason a longer name costs air instead of a cut,
 * and the reason the panel is wider now rather than a row scrolling.
 *
 * Not for the one-row notices - "Scanning...", "Could not start". Those are
 * messages, and a message stretched to menu width reads as a menu that failed
 * to load. Not for the in-game menu either, which sits over a paused game. */
int menu_std_width(app *a)
{
	int content = 800 - menu_pad() * 2;
	int floor   = menu_shelf_width(a);
	/* No wider than menu_draw makes a panel: a language whose rows run long
	 * (German) measured past it, and About's story, wrapped at that, slid */
	int most    = ZYPEROS_SCREEN_W - MENU_MARGIN * 2 - menu_pad() * 2;
	int w       = floor > content ? floor : content;

	return w < most ? w : most;
}

/* What --shot-at presses at 0 in a --menu shot (shot_motion): a step onto
 * its row; or with --open, --close and --swap, the top menu opening on the
 * shelf, closing onto it, or giving way to MENU > Controls. `shot_lv` is the
 * level the menu motion's frames are drawn at, 0 when there is none, and
 * `shot_pressed` says the press has come. */
enum { SHOT_STEP, SHOT_OPEN, SHOT_CLOSE, SHOT_SWAP };
static int      shot_press = SHOT_STEP;
int             shot_lv;
static bool     shot_pressed;
static unsigned shot_t0;             /* the press, on the held clock */

/* --zyper [works|fails]: the top menu's --open, for Zyper in it, with a
 * success or a failure at 500 ms, once the opening hop is down. */
static int      shot_zyper_did;      /* 1 works, -1 fails, 0 neither */

/* Zyper's moment in a --menu shot, on --shot-at's clock: from the menu's
 * open with --open and --zyper, and otherwise from a second before the press,
 * the menu having opened then. 1000, at rest before any blink can come,
 * without --shot-at. */
static unsigned shot_zyper_ms(void)
{
	if (!g_clock_held) return 1000;
	return (shot_press == SHOT_OPEN ? 0u : 1000u) + (anim_now() - shot_t0);
}

/* How the shelf's menu is drawn from `screen`: ZyperOS's own on the systems
 * row, the top one, which is the one Zyper is in, and a system's on a game
 * list, which is where it gains rows that belong to that system. The menu
 * and its --menu shot both take it from here. */
menu_style zyperos_menu_style(app *a, int screen)
{
	return (menu_style){
		/* Measured across every system and every display mode, so the panel
		 * does not resize while Display Mode is being cycled on it. */
		.fixed_w     = menu_std_width(a),
		.accent      = MENU_ZYPEROS,
		/* On a game shelf this menu is about THAT system, so it wears the
		 * shelf's color - which tick_tint is still moving while it is open. */
		.follow_tint = screen != SCREEN_SYSTEMS,
		.zyper       = screen == SCREEN_SYSTEMS,
		/* The ZyperOS menu's tabs; a system's menu is one list */
		.tabs        = screen != SCREEN_GAMES ? &g_tabs : NULL,
		/* and the themes' chips on Display's Theme row */
		.row_mark    = screen != SCREEN_GAMES ? look_row_mark : NULL,
	};
}

static void zyperos_menu_draw(app *a, int sel)
{
	menu_row rows[MENU_MAX_ROWS];
	menu_bufs bufs;
	const char *heading;
	int n = menu_build(a, a->screen, a->sys_cursor, rows, &bufs, &heading);
	const menu_style st = zyperos_menu_style(a, a->screen);
	unsigned zms = shot_zyper_ms();
	float k = 1.0f;
	zr_rig zy;

	/* Where it is in its open, as menu_run_body asks; at rest without one */
	if (shot_lv) panel_frame(a, shot_lv, &k);
	ui_scrim_k(a->r, 0, k);
	/* Zyper, on the shot's own clock from 0, as menu_run_body keeps him */
	if (st.zyper) {
		zr_init(&zy, 0, 1);
		zr_open(&zy, 0);
		if (shot_zyper_did && zms >= 500) {
			zr_tick(&zy, 500);
			zr_press(&zy, 500);
			menu_zyper_answer(&zy, 500, shot_zyper_did > 0 ? MENU_CHANGED : MENU_FAILED);
		}
		zr_tick(&zy, zms);
		g_menu_zyper = &zy;
		g_menu_zyper_at = zms;
	}
	g_menu_tabs = st.tabs;
	g_row_mark = st.row_mark;
	menu_draw(a, heading, rows, n, sel, st.fixed_w, st.follow_tint ? a->tint : st.accent);
	g_row_mark = NULL;
	g_menu_tabs = NULL;
	g_menu_zyper = NULL;
}

int sysmenu_build(void *ctx, menu_row *rows, int max,
                  const char **heading)
{
	sysmenu_ctx *c = ctx;
	int n = menu_build(c->a, c->screen, c->a->sys_cursor, rows, &c->bufs,
	                   heading);

	return n > max ? max : n;
}

/* Left and right cycle the value on a row that has one; A opens whatever the
 * row leads to. Everything else the runner has already dealt with. */
/* ---------- a game picked in a menu --------------------------------------- */

/* A game Random Game or Search Games picked inside a menu, started once the
 * menu has gone (zyperos_menu_for): the shelf is put on it, the way the
 * systems row and A get there, shown for `hold_ms` - Random Game's moment to
 * see what came up, in which B or MENU stays on the shelf at the game - and
 * launched as A there would. Shared by the two, word for word. */
game_pick g_pick = { -1, -1, 0 };

void pick_launch(app *a)
{
	int shelf = g_pick.shelf, game = g_pick.game, was = a->sys_cursor;
	Uint32 until = plat_now_ms() + g_pick.hold_ms;

	char file[LIB_PATH];

	g_pick.shelf = -1;
	if (shelf < 0 || shelf >= a->sys.count || game < 0 ||
	    game >= sys_games(&a->view[shelf])->count)
		return;
	/* The game by its file: the index is of every game of the system, and
	 * the shelf may show one folder of them (src/shelfdir.h) */
	snprintf(file, sizeof file, "%s", sys_games(&a->view[shelf])->items[game].file);
	fprintf(stderr, "pick: %s/%s\n", a->sys.systems[shelf].folder, file);
	if (a->screen == SCREEN_GAMES && shelf != was)
		evict_far(&a->view[was], TEX_KEEP_FAR);
	a->sys_cursor = shelf;
	home_reset(&a->home, shelf, a->sys.count, plat_now_ms());
	if (a->view[shelf].all) shelf_reveal(a, shelf, file);
	else a->view[shelf].cursor = game;
	enter_system(a);
	do {
		plat_input_poll(&a->in);
		if (a->in.quit_requested) { a->running = false; return; }
		if (a->in.pressed[IN_POWER]) { power_off(a); return; }
		if (a->in.pressed[IN_BACK] || a->in.pressed[IN_MENU]) {
			uisound_play(DS_SOUND_BACK);
			memset(&a->in, 0, sizeof a->in);
			return;
		}
		tick_tint(a);
		render(a);
		ui_frame_rest(8);
	} while ((int)(until - plat_now_ms()) > 0 && a->running && !want_quit);
	memset(&a->in, 0, sizeof a->in);
	launch(a);
}

/* Copy the paused frame's preview beside a manual save, so the slot strip can
 * show what is inside each slot. The pause preview IS the frame the save
 * serializes - Diatom wrote it on the way into the menu - so a straight copy
 * is the truthful thumbnail, no protocol round trip needed. */
/* The SOURCE is opened first, and the destination only if that worked.
 *
 * Opening both up front truncates the destination whenever the source is
 * missing - so a copy that cannot happen destroys the file it was going to
 * replace. Harmless while the only caller copied a preview a pause had just
 * written; not harmless once the Auto card is written this way, where the
 * thing being overwritten is the last good picture of a save. */
void copy_file(const char *from, const char *to)
{
	FILE *a, *b;
	char buf[16384];
	size_t n;

	if (!from || !*from || !to || !*to) return;
	if (!(a = fopen(from, "rb"))) return;
	if (!(b = fopen(to, "wb"))) { fclose(a); return; }

	while ((n = fread(buf, 1, sizeof buf, a)) > 0) fwrite(buf, 1, n, b);
	fclose(a);
	fclose(b);
}


/* ---------- main ---------------------------------------------------------- */

/* --shot <file.png> [--screen games|systems] draws one frame, writes it out
 * and exits. This is how the shelf gets looked at without a device in hand:
 * the host build renders exactly what the handheld renders. */
const char *shot_path;
static int shot_screen = -1;
/* --folder DIR: the shelf on that folder of its games (src/shelfdir.h);
 * --game FILE: its cursor on that game or folder, opening the folder it is
 * in; --view list|covers: MENU > View for the shot, whatever is saved */
static const char *shot_folder, *shot_game, *shot_view;
/* --shelf-press KEYS: the shelf's own presses before the shot, comma
 * separated - a, b, x, y, up, down, left, right, l1, r1, start - each one pass of
 * update_games, as the device's loop makes them (never A on a game: that
 * starts it) */
static const char *shot_shelf_press;
static int shot_menu, shot_menu_sel;
/* --menu-steps N: N presses of Down (Up when N is negative) on --menu's
 * list before the shot, a second apart on a held clock, so a list that
 * goes round can be looked at past its end (DESIGN.md 11.3) */
static int shot_menu_steps;
/* --menu-tab-from M: the tab MENU's move comes from, or -1 */
static int shot_tab_from = -1, shot_tab_ms;
/* --look ACCENT,LIGHT,DARK,CORNERS,ZYPER: the look for the shot; and
 * --look-page theme|hours [row]: Theme's page or Auto Hours' (DESIGN.md
 * 11.42), the cursor on `row` */
static const char *shot_look, *shot_look_page;
static int shot_look_row;
static int shot_kb, shot_kb_layer;
static keyboard_transition shot_keyboard;
static bool shot_keyboard_panel;
/* --kbsel ROW COL: the keyboard's selected key, rows 0-3 the grid and row 4
 * the action row, with COL its key (src/kbnav.h). It opens on the letters'
 * first key, as kb_prompt does. */
static int shot_kb_row = 1, shot_kb_col;
static const char *shot_notice;
static const char *shot_notice_head = "Unlocked · 5 points";
/* --notice-badge PNG and --notice-progress VALUE TARGET: the same notice with
 * a badge, or as a progress notice (notice_render_ex) */
static const char *shot_notice_badge;
static int shot_notice_value, shot_notice_target;
/* --notice-kind unlock|progress|set: what the notice is about, which says
 * what it carries (notice_look): an unlock has the plain badge where no
 * --notice-badge is given, and Zyper; progress, the plain badge; set, neither */
static nq_kind shot_notice_kind = NQ_SET;
/* --notice-heat: the notice with the thermometer where a badge goes, as
 * "Getting hot" is over a game (DESIGN.md 11.23) */
static int shot_notice_mark;
/* --popup saved|loaded|save_failed|load_failed|slot_empty|no_states|dslang|corestate|stick_on|stick_off|speed N:
 * the pop-up of the SELECT hotkeys, as on_quick and on_speed put it over a
 * game (notice_render, no badge, no Zyper); N is the step for speed, in
 * halves (3 is 1.5x) */
static const char *shot_popup;
static int shot_popup_half = 2;
/* --lowbatt MS [charging|off]: the low-battery start screen MS ms in
 * (DESIGN.md 11.24); with charging, a charger plugged in at 2.4 s; with off,
 * MS ms into its send-off */
static int shot_lowbatt = -1, shot_lowbatt_kind;
/* --heatpop MS: Zyper's "Getting hot" on the home, MS ms in (DESIGN.md 11.23) */
static int shot_heatpop = -1;
/* --ranews N: Zyper's bubble saying N achievements earned offline went */
static int shot_ranews;
/* --cheevos-waiting N: the list's heading with N of its unlocks waiting */
int shot_cheevos_waiting;
/* --upnews available|ready: his bubble with System Update's news instead */
static const char *shot_upnews;
/* --charging MS [hold|unplugged|off|pending|kept|limit|keptfull|soon]: the
 * charging screen MS ms in (DESIGN.md 11.25), a wall charger putting 1480 mA
 * into a 62% battery; with hold, POWER held half way; unplugged, the cable
 * out; off, MS ms into its send-off; pending, the charger not named yet;
 * kept, held at an 85% charge limit (11.26); limit, charging toward it;
 * keptfull, held at 100%, charged full before the limit was set; soon, a
 * 92% battery, full in minutes */
static int shot_charging = -1, shot_charging_kind;
static int shot_cheevos;
static int shot_cheevos_sel;
/* --ingame: --cheevos-screen as the in-game menu opens it, over a paused
 * game (a made-up frame), rather than over the shelf */
static int shot_ingame;
/* How far between two systems the shelf is caught, 0 at rest. The shot is one
 * settled frame, so anything that only happens DURING a move - the label
 * crossfade, a card mid-slide - cannot be looked at without this. */
static float shot_sysmove;
/* The same for the games shelf, which is the one with a long list under it.
 * Eleven systems make a rail whose segments tile the track; three hundred games
 * make an 18px segment moving two and a half pixels a card, and the two do not
 * look alike at any fraction of a move. */
static float shot_gamemove;
/* --shot-at MS: a motion started for the shot and drawn MS milliseconds in,
 * on a clock held still for it (anim_now, cf_clock), the one clock of every
 * shot that moves: with --menu, the focus plate moving onto the menu's row
 * from the row above it (the list gives too when the window scrolls), or with
 * --open, --close or --swap the top menu opening on the shelf, closing onto
 * it, or giving way to MENU > Controls; with --zyper, Zyper in the top menu
 * that long after it opened; with --launch, the zoom into the focused game;
 * with --poweroff, the power-off; otherwise the shelf on screen stepping one
 * card forward. Frame strips are made from these, a shot for each time. -1,
 * without it, is a shot at rest, drawn as it always was. Unlike --sysmove
 * and --gamemove, which stage a fraction of a move, this presses: the plate,
 * the menu and the shelf are told to move as the buttons tell them, and the
 * frames between are drawn as the device draws them, so whatever follows
 * over time - the plate, the list's give, the label's crossfade, the
 * background tint - is where it is that far in. */
static int shot_at = -1;
/* --launch: the launch zoom at --shot-at's time (0 without it), of the
 * focused game of the focused system, from either screen: the systems screen
 * has the same game focused that its shelf would open on, and the shot reads
 * its card on the frame, as a shot reads every card. */
static int shot_launch;
static const char *shot_cheevos_game = "Hagane: The Final Conflict";
/* --cheevos-badge SET NAME: every row with that badge (src/rabadge.h), the
 * ones not earned greyed, as the list draws them; a '#' in NAME is the row's
 * number, 1 to 8, so each row can have a picture of its own. Without it every
 * row has the plain badge, as a set the card has no art for does. */
static const char *shot_badge_set, *shot_badge_name;
static bool shot_badge_got[8];

static SDL_Texture *shot_row_badge(int row)
{
	char name[64], *hash;

	if (row < 0 || row >= 8) return NULL;
	snprintf(name, sizeof name, "%s", shot_badge_name ? shot_badge_name : "");
	if ((hash = strchr(name, '#'))) *hash = (char)('1' + row);
	return rb_icon(g_badge_r, shot_badge_set, name, !shot_badge_got[row], theme());
}
static int shot_syn;       /* --synopsis: the card a scraped game opens */
static int shot_info;
static int shot_delete_ask, shot_delete_now;

static int shot_controls = -1;
/* --sgdb [0|1|2]: MENU > SteamGridDB with no key, a key that works, or one
 * they refused - a stand-in key, masked as a real one is */
static int shot_sgdb = -1;
/* --scraper [3|4]: the Video Snaps page; ScreenScraper's page. MENU >
 * Scraper itself is --scraper-page (scraper_page_shot). */
static int shot_scraper = -1;
static int shot_about = -1;
/* --about-scroll F: the story page scrolled F (0 to 1) of the way down, as
 * Up and Down would leave it. Left alone it is the top, where it opens. */
static float shot_about_scroll = -1;
/* --language [ROW]: MENU > Language's panel over the shelf, the cursor on
 * ROW (the language that is on when none is given) */
static int shot_language = -2;
/* --music-page [ROW [STEPS]]: MENU > Menu Music's page, the cursor on ROW,
 * then STEPS presses of Down, a frame each, so the list goes round and the
 * note under it comes into the window; `--music-page foot` draws it with no
 * cursor, the note pinned under the rows as a card's footer is, so a check
 * can measure it whatever the window shows */
static int shot_music_page = -2, shot_music_steps;
/* --update: MENU > System Update's page as the status file in
 * ZYPEROS_USERDATA/update says (the update lab's states) */
static int shot_update = -1;
static int shot_hare;
static int shot_usb = -1;   /* --usb-ask [N]: the question, tile N focused */
/* --datetime [N]: Date & Time's page, Time Zone on Auto with nothing found
 * yet (0), on Auto with the zone it found (1), with a long city (2), on a
 * zone of the list (3), or Auto's message as it is chosen (4); Time Zone's
 * list open on its first zone (5), on its longest city in the middle (6), on
 * Auto with the zone it found (7), and with a long city (8) */
static int shot_datetime = -1;
static int shot_dev = -1;   /* --dev-page [N]: Developer Mode's page with 1, 12 or no keys (0..2), or the message that says it is on (3) */
static bool shot_usb_dev = true;   /* --usb-ask N plain: without the development image's tile */
/* --usb-power [N]: What did you plug in?, row N focused (1, Computer,
 * where it starts; 2, Phone); --usb-power said: the message after
 * Computer; --usb-power phone: the message after Phone; --usb-power
 * iphone: an iPhone's */
static int shot_power = -1;
static int shot_usbdrive = -1;   /* --usb-drive N */
/* --xfer-ask [N] [game]: File Transfer's question while files move, tile N
 * focused; with "game", the one a game asked for gets */
static int shot_xfer_ask = -1;
static bool shot_xfer_game;
static int shot_hot, shot_hot_in;
static const char *shot_hot_ssid   = "ZyperOS-7A2B";
static const char *shot_hot_pass   = "q7mx-3kpa";
static const char *shot_hot_phones = "1 joined";
static const char *shot_hot_now    = "receiving Contra (USA).zip";
static const char *shot_hare_addr = "192.168.1.42";
static const char *shot_hare_who  = "1 connected";
static const char *shot_hare_head = "Contra (USA).zip";   /* the file the Now row names */
static const char *shot_hare_move;                         /* --hare-move: the tiles' state */
/* --hare-steps usb|wifi STATE: File Transfer's steps (linksteps.h) at one
 * state, its hint shown where the state has one: unplugged, nocomputer,
 * noanswer, nodriver, address, noaddress, browser, nobrowser, nonetwork */
static const char *shot_steps_way, *shot_steps_state, *shot_steps_addr;
static bool shot_steps_noname;      /* ... noname: as when mDNS could not start */
static const char *shot_wait, *shot_wait_msg;   /* NULL: library.scanning's */
static int shot_osd_kind, shot_osd_val; /* --osd volume|brightness LEVEL */
/* --tour STEP [MS [DONE]]: one frame of the tutorial, src/tour.h's step by
 * its name (or "skip", the shelf with the message a skip leaves), MS into
 * it, its press landed DONE ms ago or not at all (-1). --tour-wrong KEY [MS]
 * a press of the wrong button KEY, MS ago; --fn-volume the Pixel 2's keys,
 * whose brightness is MENU+VOL. --tour-pick N [MS]: in a choice step, the
 * focus moved to choice N MS ago (default long since); --tour-music a card
 * with menu music, so the strip counts its step. */
static int shot_tour = -2;          /* -2 none, -1 the skip message */
static int shot_tour_ms = 2000, shot_tour_done = -1;
static int shot_tour_wrong = -1, shot_tour_wrong_ms = 300;
static int shot_tour_pick = -1, shot_tour_pick_ms = 5000;
static bool shot_tour_music;
static int shot_fn_volume;
/* By the step, so a step moved in src/tour.h keeps its name */
static const char *const TOUR_STEP_NAMES[TOUR_STEPS] = {
	[TOUR_LANG] = "lang", [TOUR_WELCOME] = "welcome", [TOUR_MOVE] = "move", [TOUR_A] = "a",
	[TOUR_B] = "b", [TOUR_X] = "x", [TOUR_Y] = "y", [TOUR_MENU] = "menu",
	[TOUR_SELECT] = "select", [TOUR_VOL] = "vol", [TOUR_BRIGHT] = "bright",
	[TOUR_POWER] = "power", [TOUR_THEME] = "theme", [TOUR_HOME] = "home",
	[TOUR_SOUNDS] = "sounds", [TOUR_MUSIC] = "music", [TOUR_FINISH] = "finish" };

/* --theme light|dark: the theme the shot is drawn in, whatever the saved
 * Appearance is; -1 for the saved one. */
static int shot_theme = -1;
static int shot_home = -1;
/* --lang CODE: the language the shot is drawn in (src/i18n.h; xx, the
 * pseudo-locale, on a host), whatever MENU > Language saved; NULL for it */
static const char *shot_lang;
/* --cheer MS: the cheer for the first games, MS into it, over the home */
static int shot_cheer = -1;
/* --songpop "HEAD|BODY" [MS]: Zyper's pop-up with a song (DESIGN.md 11.22),
 * MS into its life, 1500 by default: in, his hop and pant over, held */
static const char *shot_songpop;
static int shot_songpop_ms = 1500;
static const char *shot_song;
static bool shot_song_paused;
/* --status's words into shot_st, the corner's stand-ins; false on a word it
 * does not know. Wi-Fi is off and the battery has no reading until a word
 * says otherwise, as on a host. */
static bool shot_status_parse(const char *spec)
{
	char buf[128], *w, *save;

	shot_st.set = true;
	shot_st.m = (status_marks){ .wifi = STATUS_WIFI_NONE, .batt = -1 };
	snprintf(buf, sizeof buf, "%s", spec);
	for (w = strtok_r(buf, " ,", &save); w; w = strtok_r(NULL, " ,", &save)) {
		int h, mi;

		if (sscanf(w, "%d:%d", &h, &mi) == 2 && h >= 0 && h < 24 && mi >= 0 && mi < 60)
			shot_st.minutes = h * 60 + mi;
		else if (!strcmp(w, "24h")) shot_st.h24 = 1;
		else if (!strcmp(w, "12h")) shot_st.h24 = 0;
		else if (!strcmp(w, "wifi=off")) shot_st.m.wifi = STATUS_WIFI_NONE;
		else if (!strcmp(w, "wifi=idle")) shot_st.m.wifi = STATUS_WIFI_IDLE;
		else if (!strncmp(w, "wifi=", 5) && w[5] >= '1' && w[5] <= '3' && !w[6])
			shot_st.m.wifi = w[5] - '0';
		else if (!strcmp(w, "bt")) shot_st.m.bt = true;
		else if (!strncmp(w, "batt=", 5)) shot_st.m.batt = atoi(w + 5);
		else if (!strcmp(w, "chg")) shot_st.m.charging = true;
		/* on the cable, held at the charge limit: the plug */
		else if (!strcmp(w, "held")) shot_st.m.charging = shot_st.m.held = true;
		else if (!strcmp(w, "blinkoff")) shot_st.m.blink_off = true;
		/* what MENU > Battery Icon > Time Left writes: eta=200 is 3 h 20 min,
		 * on the cable the time to full; without it, the percentage */
		else if (!strncmp(w, "eta=", 4) && w[4] >= '0' && w[4] <= '9') shot_st.eta = atoi(w + 4);
		/* Time Left before it has a time: calc is the dots two filled, calc=N
		 * N of them (0 to 3); full is Charged, on the cable */
		else if (!strcmp(w, "calc")) shot_st.calc = 2;
		else if (!strncmp(w, "calc=", 5) && w[5] >= '0' && w[5] <= '3' && !w[6]) shot_st.calc = w[5] - '0';
		else if (!strcmp(w, "full")) shot_st.m.charging = shot_st.full = true;
		/* MENU > Battery Icon for this shot, not the saved one: icon=bars,
		 * icon=percentage or icon=time */
		else if (!strncmp(w, "icon=", 5)) {
			int k;

			for (k = 0; k < BATT_MODES && strcmp(w + 5, BATT_IDS[k]); k++) ;
			if (k == BATT_MODES) return false;
			shot_st.icon = k;
		}
		/* File Transfer behind the shelf: xfer=on, nothing moving; xfer=62,
		 * files moving and the ring that far round */
		else if (!strcmp(w, "xfer=on")) shot_st.m.xfer = true;
		else if (!strcmp(w, "usb=on")) shot_st.m.usb = true;
		/* System Update downloading behind: update=42, the ring that far */
		else if (!strncmp(w, "update=", 7) && w[7] >= '0' && w[7] <= '9') {
			shot_st.m.update = true;
			shot_st.m.update_pct = atoi(w + 7);
		}
		/* headphones in the jack: their mark */
		else if (!strcmp(w, "phones=on")) shot_st.m.phones = true;
		else if (!strncmp(w, "xfer=", 5) && w[5] >= '0' && w[5] <= '9') {
			shot_st.m.xfer = shot_st.m.xfer_moving = true;
			shot_st.m.xfer_pct = atoi(w + 5);
		}
		else return false;
	}
	return true;
}

/* --flip: the shot drawn whole in the theme --theme starts it in, so every
 * card on it is made in that theme, and then the theme turned to the other
 * one the way Auto turns it (shot_flip_theme), and the shot taken there.
 * `make check-flip` holds it against a shot started in the second theme: a
 * card the turn left in the old theme, and the settled frame did not replace,
 * is the difference. */
static int shot_flip;
/* Set by take_shot when the frame it drew fails a check of its own (see
 * shot_cards_have_feet), and the run then exits 3: a shot is also how
 * `make check-flip` looks at the systems row, the games shelf and Now
 * Playing's cover, and a picture that is wrong in a way a byte comparison of
 * two wrong pictures cannot see is one it must not pass. */
static int shot_failed;
/* --portload NAME: the still a port's loading screen leaves for splash, for
 * the game called NAME, drawn to the shot's file in place of a shelf. */
static const char *shot_portload;
/* --poweroff: the power-off animation's frame --shot-at's time in (0
 * without it), in place of a shelf */
static int shot_poweroff;
static const char *shot_kb_text = "correct horse";
/* The title was hardcoded to "Wi-Fi password", so the one other thing that
 * uses this keyboard - signing in to RetroAchievements - could not be
 * rendered at all. A harness narrower than the thing it checks. */
static const char *shot_kb_title = "Wi-Fi password";
static int shot_slots, shot_slot_sel;
static int shot_jump;               /* letter-jumps to apply before drawing */
/* --nowplaying ALBUM [TRACK [SECONDS]], and --paused with it. */
static int   shot_np = -1, shot_np_track, shot_np_paused;
/* --locked: the lock screen over it, its hold three fifths done. --video
 * [full|controls]: Now Playing as a video, a test picture for the frame. */
static int   shot_locked, shot_video = -1;
static float shot_np_at = 83.0f;
/* --mode NAME plays whatever the shot shows in that play mode: the mark beside
 * the track count is drawn from it. */
static const char *shot_mode;
static float shot_slot_aspect = 4.0f / 3.0f;

/* A stand-in for a paused game frame, at whatever shape was asked for: the
 * slot carousel frames the picture to its own aspect, so looking at that
 * without a device means being able to hand it one. */
static SDL_Texture *fake_frame(SDL_Renderer *r, float aspect)
{
	int h = 224, w = (int)(224 * aspect + 0.5f);
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32,
	                                                SDL_PIXELFORMAT_ARGB8888);
	SDL_Texture *t;
	int x, y;

	if (!s) return NULL;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			Uint32 *px = (Uint32 *)((Uint8 *)s->pixels + (size_t)y * s->pitch);
			int checker = ((x / 16) + (y / 16)) & 1;
			px[x] = SDL_MapRGBA(s->format,
			                    (Uint8)(30 + 180 * x / w),
			                    (Uint8)(40 + 150 * y / h),
			                    (Uint8)(checker ? 150 : 90), 255);
		}
	t = SDL_CreateTextureFromSurface(r, s);
	SDL_FreeSurface(s);
	return t;
}

static void shot_draw_slots(app *a)
{
	slot_view sv = { .aspect = shot_slot_aspect, .saving = 0 };
	int i;

	for (i = 0; i <= GM_SLOTS; i++) {
		/* An afternoon in August, a slot an hour apart */
		struct tm tm = { .tm_mon = 7, .tm_mday = 20 + i, .tm_hour = 13 + i,
		                 .tm_min = i * 7, .tm_sec = i * 9 };

		/* Slot 5 left empty, so the empty state is in the picture too. */
		if (i == 5) continue;
		sv.have[i] = 1;
		slot_when(sv.when[i], sizeof sv.when[i], &tm);
		sv.thumb[i] = fake_frame(a->r, shot_slot_aspect);
	}
	ui_scrim(a->r, 1);
	slot_draw(a, &sv, shot_slot_sel);
	for (i = 0; i <= GM_SLOTS; i++)
		if (sv.thumb[i]) SDL_DestroyTexture(sv.thumb[i]);
}

/* --gamemenu: the menu over a paused game, as gm_backdrop and the runner draw
 * it, on a made-up frame: the one place the status corner is drawn in Salt &
 * Pepper whatever the theme. On a games shelf, whose system and game it
 * builds its rows for. */
/* --gamemenu [row] port: the menu over a port instead (src/portmenu.h), as
 * port_menu shows it over OpenBOR; --backdrop PNG puts a picture of a real
 * game under it in place of the made-up frame, at the screen's size. */
static int shot_gamemenu, shot_gamemenu_sel, shot_gamemenu_tab;
static const char *shot_backdrop;
/* The Controls tab's (src/ctltab_screen.h), with --gm-tab 2: --gm-all,
 * Applies To on All <System>; --gm-pick, the choices of the button under
 * the cursor; --gm-ask, Reset Buttons' question; --gm-keys N, page N of the
 * Hotkeys row's pages. */
static bool shot_gm_all, shot_gm_pick, shot_gm_ask;
static int  shot_gm_keys = -1;
/* With --shot-at, its motion: --gm-tab-from N, L1 or R1 from tab N to
 * --gm-tab's; --gm-press KEYS, the Controls tab's presses (up, down, left,
 * right, comma separated) from the row --gamemenu names. The tab's move,
 * when one was pressed (shot_motion); and whether the handheld was lit
 * yet, so the frames of a motion go on from the one before. */
static int  shot_gm_from = -1;
static const char *shot_gm_press;
static unsigned shot_gm_t0;
static bool shot_gm_open;

/* --gamemenu's Controls tab, beside the handheld, or a page it opens */
static void shot_draw_ctltab(app *a, menu_tabs *tabs)
{
	static menu_row rows[CT_MAX_ROWS];
	unsigned vcol[CT_MAX_ROWS];
	int ids[CT_MAX_ROWS], n, sel;
	menu_style over = { .fixed_w = gm_width(a), .follow_tint = true };
	menu_style st = over;
	ctlscr_game g;

	gm_ctl_game(a, NULL, &g);
	g.over = &over;
	g_gm_mem.scope_all = shot_gm_all;
	if (!shot_gm_open) ctlscr_open();
	shot_gm_open = true;
	n = ctlscr_rows(a, &g, rows, vcol, NULL, ids);
	sel = shot_gamemenu_sel < n ? shot_gamemenu_sel : n - 1;
	/* Off a quiet row, as the runner steps off one */
	if (!rows[sel].live) {
		sel = menu_step_sel(rows, n, sel, +1);
		if (!rows[sel].live) sel = menu_step_sel(rows, n, sel, -1);
	}
	if (shot_gm_pick) {
		ctlscr_shot_pick(a, &g, ids[sel]);
		return;
	}
	if (shot_gm_ask) {
		char q[LIB_NAME + 256], lines[DEL_ASK_LINES][CHV_WRAP_COLS];
		menu_row ask[DEL_ASK_LINES + 2];
		int nq;

		ctlscr_question(&g, q, sizeof q);
		nq = store_ask_rows(a, q, lines, ask);
		ask[nq] = (menu_row){ T(S_GAMEMENU_RESET_BUTTONS_YES), NULL, true };
		ask[nq + 1] = (menu_row){ T(S_PANEL_CANCEL), NULL, true };
		menu_draw(a, T(S_GAMEMENU_RESET_BUTTONS), ask, nq + 2, nq + 1, menu_std_width(a),
		          MENU_ZYPEROS);
		return;
	}
	if (shot_gm_keys >= 0) {
		ctl_game cg = game_controls_of(a);
		const menu_row *pr;
		ctl_buf cb;
		char count[16];
		int pages, np, loop_at, width;

		cg.no_buttons = true;
		ctl_set_fn_volume(shot_fn_volume || plat_brightness_on_volume());
		pages = ctl_game_pages(&cg);
		width = game_controls_width(a, &cg);
		np = controls_page_rows(&cg, shot_gm_keys, width, &cb, &pr, &loop_at);
		snprintf(count, sizeof count, "%d / %d", shot_gm_keys < pages ? shot_gm_keys + 1 : 1, pages);
		menu_draw_caption(a, cb.head, pr, np, -1, width, a->tint, NULL, false, loop_at,
		                  pages > 1 ? count : NULL);
		return;
	}
	st.tabs = tabs;
	st.side = ctlscr_side;
	st.side_w = CTLSCR_SIDE_W;
	st.vcolors = vcol;
	menu_draw_styled(a, &st, gm_tab_name(tabs->at), rows, n, sel);
}

static void shot_draw_gamemenu(app *a)
{
	menu_row rows[GM_MAX_ROWS];
	gm_bufs b;
	SDL_Texture *bg = shot_backdrop ? IMG_LoadTexture(a->r, shot_backdrop)
	                                : fake_frame(a->r, 4.0f / 3);
	gm_ui u;
	menu_tabs tabs = { .count = GT_TABS, .name = gm_tab_name_i };
	int n;

	gm_gather(a, &u);
	tabs.at = tabs.from = shot_gamemenu_tab % GT_TABS;
	/* The tab move a shot pressed (shot_motion), from where it was */
	if (shot_gm_t0 && shot_gm_from >= 0) {
		tabs.from = shot_gm_from % GT_TABS;
		tabs.dir = (tabs.at - tabs.from + GT_TABS) % GT_TABS == 1 ? +1 : -1;
		tabs.t0 = shot_gm_t0;
	}
	gm_backdrop(a, &(gm_ctx){ .a = a, .bg = bg }, 1.0f);
	if (!u.port && tabs.at == GT_CONTROLS) {
		shot_draw_ctltab(a, &tabs);
		if (bg) SDL_DestroyTexture(bg);
		return;
	}
	/* The Achievements tab, as gm_menu_build makes it and menu_run draws it:
	 * the set on the card's game, or --achtab's state with a made-up set */
	if (!u.port && tabs.at == GT_ACHIEVEMENTS) {
		static achtab_bufs ab;
		static menu_row arows[MENU_RUN_ROWS];
		menu_style st = { 0 };
		at_facts f;
		int an, asel;

		if (shot_badge_set) achtab_shot_badges(shot_badge_set, shot_badge_name);
		gm_at_facts(&f);
		st.fixed_w = gm_width(a);
		st.follow_tint = true;
		st.tabs = &tabs;
		an = achtab_rows(a, &f, gm_at_source(), &ab, arows, MENU_RUN_ROWS, st.fixed_w, &st);
		asel = shot_gamemenu_sel < an ? shot_gamemenu_sel : an - 1;
		if (!st.visits_all && !arows[asel].live) asel = -1;
		menu_draw_styled(a, &st, gm_tab_name(tabs.at), arows, an, asel);
		if (bg) SDL_DestroyTexture(bg);
		return;
	}
	n = gm_rows(&u, (gm_tab)tabs.at, rows, &b);
	if (n == 0) rows[n++] = MENU_NOTE("");
	g_menu_tabs = u.port ? NULL : &tabs;
	/* The handheld leaving with the Controls tab's name */
	g_menu_side = u.port ? NULL : ctlscr_side;
	menu_draw(a, u.port ? NULL : gm_tab_name(tabs.at), rows, n,
	          shot_gamemenu_sel < n ? shot_gamemenu_sel : n - 1,
	          gm_width(a), a->tint);
	g_menu_side = NULL;
	g_menu_tabs = NULL;
	if (bg) SDL_DestroyTexture(bg);
}

/* --player2 KIND: NES Player 2's page over a paused game (a made-up frame),
 * in a state made up as the hot panel's shot makes its network (--hotspot):
 * hotspot, its code with nobody yet; connected, player 2 playing; cable, a
 * computer on the cable; off and off-cable, before A on each link; none,
 * neither plugged in; failed; starting, the hotspot's start; closing and
 * closing-cable, the stop on each link. With --gamemenu the in-game menu
 * instead, its Player 2 row saying that state. */
static void shot_draw_player2(app *a)
{
	SDL_Texture *bg = shot_backdrop ? IMG_LoadTexture(a->r, shot_backdrop)
	                                : fake_frame(a->r, 4.0f / 3);

	paused_backdrop(a, bg, 1.0f);
	p2_show(a, &g_shot_p2_view);
	if (bg) SDL_DestroyTexture(bg);
}

/* --gamecontrols [page] [TAG]: the in-game menu's Controls page over a
 * paused game, page `page`, for the game under the cursor on the shelf of
 * the system tagged TAG (the shelf the shot is on when none is given), as
 * game_controls_screen draws it. --fn-volume names the Pixel 2's
 * brightness chord. */
static int shot_gamecontrols = -1;
static const char *shot_gamecontrols_tag;

static void shot_draw_gamecontrols(app *a)
{
	SDL_Texture *bg = shot_backdrop ? IMG_LoadTexture(a->r, shot_backdrop)
	                                : fake_frame(a->r, 4.0f / 3);
	const menu_row *rows;
	ctl_buf b;
	ctl_game g;
	char count[16];
	int i, n, pages, width, loop_at;

	for (i = 0; shot_gamecontrols_tag && i < a->sys.count; i++)
		if (!strcmp(a->sys.systems[i].tag, shot_gamecontrols_tag)) {
			a->sys_cursor = i;
			a->tint = a->sys.systems[i].accent;
			break;
		}
	g = game_controls_of(a);
	ctl_set_fn_volume(shot_fn_volume || plat_brightness_on_volume());
	width = game_controls_width(a, &g);
	n = controls_page_rows(&g, shot_gamecontrols, width, &b, &rows, &loop_at);
	pages = ctl_game_pages(&g);
	snprintf(count, sizeof count, "%d / %d",
	         shot_gamecontrols < pages ? shot_gamecontrols + 1 : 1, pages);
	paused_backdrop(a, bg, 1.0f);
	menu_draw_caption(a, b.head, rows, n, -1, width, a->tint, NULL, false, loop_at,
	                  pages > 1 ? count : NULL);
	if (bg) SDL_DestroyTexture(bg);
}

/* --handheld-box [tab|tour]: the handheld measured in every pose the
 * Controls tab (ctlscr_box_check) and the tutorial (tourdev_box_check) give
 * it, every frame of every turn, against its kept texture's box: a texture
 * too small cuts the figure off on the device and on the host alike. A
 * "HANDHELD-BOX:" line on stderr for each pose out of its box, and a count
 * (tools/controls-shots-check.sh, tools/tour-shot-check.py). */
static const char *shot_handheld_box;

static void handheld_box_report(void)
{
	int tab = 0, tour = 0, bad = 0;

	if (strcmp(shot_handheld_box, "tour")) bad += ctlscr_box_check(&tab);
	if (strcmp(shot_handheld_box, "tab")) bad += tourdev_box_check(&tour);
	fprintf(stderr, "handheld-box: %d poses of the Controls tab and %d of the tutorial measured, "
	        "%d out of their box\n", tab, tour, bad);
}

/* --controls-fit [game|home]: every Controls page measured with the font
 * against the panel that draws it - the home's five on both devices, and
 * every system's in a game (an .fds game's too), or one of the two sets. A
 * row wider than the panel is cut or slides; a page taller than it scrolls
 * itself, which is wrong for a table read a button at a time. A
 * "CONTROLS-FIT:" line on stderr for each page that does not fit, and a count
 * of the pages read (tools/controls-shots-check.sh). */
static const char *shot_controls_fit;

static int controls_fit_one(app *a, const char *where, const char *head,
                            const char *count, const menu_row *rows, int n)
{
	int w = menu_std_width(a), mw = menu_measure(rows, n, head), i;
	int rws = 0, rules = 0, notes = 0, room, bad = 0;
	/* the heading is centered with the count's room kept free either side
	 * of it (panel_show) */
	int inset = count ? ui_text_width(ui_font(UI_F_META), count) + 2 * DS_GRID : 0;
	int hw = ui_text_width(ui_font(UI_F_LABEL), head);

	if (hw > w - 2 * inset) {
		fprintf(stderr, "CONTROLS-FIT: %s: heading \"%s\" %d wide, room for %d beside \"%s\"\n",
		        where, head, hw, w - 2 * inset, count ? count : "");
		bad = 1;
	}

	for (i = 0; i < n; i++) {
		if (!rows[i].label) rules++;
		else if (ROW_IS_NOTE(rows[i])) notes++;
		else rws++;
	}
	room = menu_list_fit(0, rules, notes);
	if (mw > w) {
		fprintf(stderr, "CONTROLS-FIT: %s: %d wide, the panel holds %d\n", where, mw, w);
		bad = 1;
	}
	if (rws > room) {
		fprintf(stderr, "CONTROLS-FIT: %s: %d rows and %d notes, room for %d rows\n",
		        where, rws, notes, room);
		bad = 1;
	}
	return bad;
}

/* The Ports shelf's: PortMaster's own pages, on the card or not, and every
 * port on this card's (src/portdoc.h), but a port's About, which is prose
 * and scrolls as a synopsis does. The pages read, into *pages. */
static int controls_fit_ports(app *a, const char *dev, int *pages)
{
	static port_doc doc;
	char dir[CFG_STR * 2], path[CFG_STR * 3], where[160], count[16];
	DIR *d;
	struct dirent *e = NULL;
	int bad = 0, p;
	bool pm = true;

	snprintf(dir, sizeof dir, "%s/Ports", P_ROMS);
	d = opendir(dir);
	while (pm || (d && (e = readdir(d)))) {
		const char *name = pm ? PM_SCRIPT : e->d_name;
		size_t l = strlen(name);
		ctl_game g;
		int np;

		if (!pm && (name[0] == '.' || l < 4 || strcasecmp(name + l - 3, ".sh") ||
		            !strcasecmp(name, PM_SCRIPT)))
			continue;
		pm = false;
		snprintf(path, sizeof path, "%s/%s", dir, name);
		port_doc_read(path, &doc);
		g = (ctl_game){ "", "Ports", NULL, "", NULL, "sh", ctl_head_fits, a, true, NULL, &doc, false,
		                CTL_STICK_TABLE };
		np = ctl_game_pages(&g);
		for (p = 0; p < np; p++) {
			menu_row rows[CTL_MAX_ROWS];
			ctl_buf b;
			int n = ctl_game_rows(&g, p, rows, &b);

			if (ctl_game_prose(&g, p)) continue;
			snprintf(where, sizeof where, "%s, the port %s page %d", dev, doc.title, p);
			snprintf(count, sizeof count, "%d / %d", p + 1, np);
			bad += controls_fit_one(a, where, b.head, np > 1 ? count : NULL, rows, n);
			(*pages)++;
		}
	}
	if (d) closedir(d);
	return bad;
}

static void controls_fit_report(app *a)
{
	static systems_cfg all;
	char path[CFG_STR * 2], where[96], head[80], count[16];
	menu_row rows[CTL_MAX_ROWS];
	bool home = strcmp(shot_controls_fit, "game") != 0;
	bool game = strcmp(shot_controls_fit, "home") != 0;
	int dev, p, i, e, pages = 0, bad = 0;

	snprintf(path, sizeof path, "%s/systems.cfg", P_ROOT);
	if (!cfg_load_systems(path, &all)) all.count = 0;
	for (dev = 0; dev < 2; dev++) {
		ctl_set_fn_volume(dev == 1);
		ctl_set_battery_lights(dev == 1);
		for (p = 0; home && p < CTL_PAGES; p++) {
			int n = ctl_rows((ctl_page)p, rows);

			snprintf(head, sizeof head, "Controls: %s", ctl_page_name((ctl_page)p));
			snprintf(count, sizeof count, "%d / %d", p + 1, CTL_PAGES);
			snprintf(where, sizeof where, "%s, the home's %s", dev ? "Pixel 2" : "Brick",
			         ctl_page_name((ctl_page)p));
			bad += controls_fit_one(a, where, head, count, rows, n);
			pages++;
		}
		for (i = 0; game && i < all.count; i++) {
			const system_cfg *s = &all.systems[i];

			/* e: an .fds game too; st: each D-pad as Stick a game can have
			 * (CTL_STICK_*), on a system whose stick is one switch a game */
			for (e = 0; e < 2; e++) for (int st = CTL_STICK_TABLE; st <= CTL_STICK_ON; st++) {
				ctl_game g = { s->tag, s->name, home_label(s->tag, s->name), s->map,
				               plat_turbo_map(s->tag), e ? "fds" : "", ctl_head_fits, a,
				               is_port(s), NULL, NULL, false, CTL_STICK_TABLE };
				int np;

				if (e && strcmp(s->tag, "NES")) continue;
				if (st && (!rms_stick_offered(s) || !strcmp(s->stick, "toggle"))) continue;
				g.stick = st;
				np = ctl_game_known(s->tag) ? ctl_game_pages(&g) : 0;
				for (p = 0; p < np; p++) {
					ctl_buf b;
					int n = ctl_game_rows(&g, p, rows, &b);

					snprintf(where, sizeof where, "%s, %s%s%s page %d",
					         dev ? "Pixel 2" : "Brick", s->tag, e ? " (fds)" : "",
					         st == CTL_STICK_ON ? " (stick on)" : st == CTL_STICK_OFF ? " (stick off)" : "",
					         p);
					snprintf(count, sizeof count, "%d / %d", p + 1, np);
					bad += controls_fit_one(a, where, b.head, np > 1 ? count : NULL, rows, n);
					pages++;
				}
			}
		}
		if (game) bad += controls_fit_ports(a, dev ? "Pixel 2" : "Brick", &pages);
	}
	fprintf(stderr, "controls-fit: %d pages measured, %d do not fit\n", pages, bad);
}

/* --press left|right|a: that key on --menu's row before the menu is drawn,
 * as the player would press it, through the menu's own handler - for a row
 * that changes a setting (Clock, Appearance, Auto Off), not one that opens a
 * screen. The setting is saved as it would be, in the card the shot runs on. */
static int shot_key = -1;   /* not shot_press, which is --shot-at's */
/* --menu's row by its label, found when the menu is built */
static const char *shot_menu_label;

/* The status corner clear of what stands near it on the frame just drawn
 * (DESIGN.md 11.12): drawn when --status gives it something to show, and
 * no clock unless --status gives one; its clock 16 from
 * its marks and its marks at x 1000;
 * the home sprite below it; and a games title's
 * capitals and a panel's top a grid step under its row. The corner's box is
 * its clock's text box and its marks, a little wider than their ink. NULL
 * when it is clear, else what it meets. */
static const char *shot_corner_clear(app *a)
{
	static char why[200];
	const SDL_Rect c = g_corner_box;
	const int under = g_corner_box.y + g_corner_box.h + DS_GRID;
	SDL_Rect card;

	if (g_clock_box.w && !shot_st.set) {
		snprintf(why, sizeof why, "a clock drawn with no --status to give one: "
		         "the host's, which changes from one minute to the next");
		return why;
	}
	if (c.w <= 0) {
		/* --status gave it something to show, and every screen a shot can
		 * draw stands under the corner */
		if (shot_st.set && (shot_st.minutes >= 0 || status_marks_width(&shot_st.m)))
			return "no corner drawn, though --status gave it something to show";
		return NULL;
	}
	if (g_clock_box.w && g_clock_box.x != TOPBAR_CLOCK_X) {
		snprintf(why, sizeof why, "the top bar's clock starts at x %d, not at %d",
		         g_clock_box.x, TOPBAR_CLOCK_X);
		return why;
	}
	if (c.x + c.w != TOPBAR_RIGHT) {
		snprintf(why, sizeof why, "the top bar's marks end at x %d, not at %d",
		         c.x + c.w, TOPBAR_RIGHT);
		return why;
	}
#ifdef I18N_PSEUDO
	/* The pseudo-locale is 40% longer on purpose, and meant to overflow */
	if (i18n_lang() != LANG_PSEUDO)
#endif
	if (g_pct_natural > STATUS_PCT_MAX) {
		snprintf(why, sizeof why, "the battery's text is %d units wide, over the %d "
		         "its room holds: it would run into the marks", g_pct_natural, STATUS_PCT_MAX);
		return why;
	}
	if (a->sys.count > 0 && a->screen == SCREEN_SYSTEMS) {
		card = a->home_layout == HOME_GALLERY ? (SDL_Rect){320,128,384,384}
		                                        : (SDL_Rect){536,112,448,448};
		if (SDL_HasIntersection(&c, &card)) {
			snprintf(why, sizeof why, "the corner (x %d..%d) meets the focused card "
			         "(x %d..%d)", c.x, c.x + c.w, card.x, card.x + card.w);
			return why;
		}
	}
	if (g_title_caps.w > 0 && g_title_caps.x < c.x + c.w &&
	    g_title_caps.x + g_title_caps.w > c.x && g_title_caps.y < under) {
		snprintf(why, sizeof why, "the title's capitals start at y %d, under %d",
		         g_title_caps.y, under);
		return why;
	}
	/* And the header's clock at the left: MP4's album title stands in its
	 * row, between the clock and the marks (header_half) */
	if (g_title_caps.w > 0 && g_clock_box.w &&
	    g_title_caps.x < g_clock_box.x + g_clock_box.w + DS_GRID &&
	    g_title_caps.y < g_clock_box.y + g_clock_box.h + DS_GRID) {
		snprintf(why, sizeof why, "the title's capitals (x %d..%d) meet the clock (x %d..%d)",
		         g_title_caps.x, g_title_caps.x + g_title_caps.w, g_clock_box.x,
		         g_clock_box.x + g_clock_box.w);
		return why;
	}
	if (g_menu_panel.w > 0 && g_menu_panel.x < c.x + c.w &&
	    g_menu_panel.x + g_menu_panel.w > c.x && g_menu_panel.y < under) {
		snprintf(why, sizeof why, "a panel's top is at y %d, under %d", g_menu_panel.y,
		         under);
		return why;
	}
	return NULL;
}

/* Now Playing without a daemon, for --shot. A cover not yet on the card is
 * drawn as missing rather than asked for: there is nobody to ask. */
static void np_shot(app *a)
{
	mu_now mn = { 0 };
	const ml_album *al;
	char next[128] = "";
	int ar, t;

	if (shot_np < 0 || shot_np >= g_muse.nalbums) return;
	al = &g_muse.albums[shot_np];
	t = shot_np_track < al->n ? shot_np_track : al->n - 1;
	for (ar = 0; ar < g_muse.nartists - 1; ar++)
		if (shot_np < g_muse.artists[ar].first + g_muse.artists[ar].n) break;
	mn.state = shot_np_paused ? MU_PAUSED : MU_PLAYING;
	snprintf(mn.title, sizeof mn.title, "%s", g_muse.tracks[al->first + t].name);
	snprintf(mn.artist, sizeof mn.artist, "%s", g_muse.artists[ar].name);
	snprintf(mn.album, sizeof mn.album, "%s", al->name);
	mn.at = shot_np_at;
	mn.len = 236.0;
	mn.index = t;
	mn.count = al->n;
	if (t + 1 < al->n)
		snprintf(next, sizeof next, "%s", g_muse.tracks[al->first + t + 1].name);

	{
		char base[LIB_PATH * 2], path[LIB_PATH * 2 + 8];

		ml_cover_base(g_muse_root, &g_muse, shot_np, base, sizeof base);
		snprintf(path, sizeof path, "%s.jpg", base);
		if (!file_nonempty(path)) {
			snprintf(path, sizeof path, "%s.png", base);
			if (!file_nonempty(path) && g_cov)
				g_cov[shot_np].st = ml_folder_image(g_muse_root, &g_muse, shot_np,
				                                    path, sizeof path)
				                  ? COV_FOLDER : COV_NONE;
		}
	}
	np_cover(a, shot_np);
	if (shot_video >= 0) {
		g_np_view = shot_video;
		mp4v_test_picture(a->r, 640, 360);
		mn.vw = 640;
		mn.vh = 360;
		np_draw_video(a, &mn, 1);
	} else {
		np_draw(a, &mn, next);
	}
	if (shot_locked) {
		mp4lock l = { 0 };

		mp4lock_engage(&l, 1000, false);
		l.held_from = 1000;
		l.now = 1000 + DS_MS_HOLD_UNLOCK * 3 / 5;
		ui_scrim(a->r, 0);
		lock_panel(a, &l);
	}
}

/* For a shot: every cover not on the card is a cover the music does not
 * carry, since there is no daemon to ask - so those albums draw their
 * generated card rather than an empty slot. */
static void muse_covers_settle(void)
{
	char base[LIB_PATH * 2], p[LIB_PATH * 2 + 8];
	int i;

	for (i = 0; g_cov && i < g_muse.nalbums; i++) {
		ml_cover_base(g_muse_root, &g_muse, i, base, sizeof base);
		snprintf(p, sizeof p, "%s.jpg", base);
		if (file_nonempty(p)) continue;
		snprintf(p, sizeof p, "%s.png", base);
		if (!file_nonempty(p))
			g_cov[i].st = ml_folder_image(g_muse_root, &g_muse, i, p, sizeof p)
			            ? COV_FOLDER : COV_NONE;
	}
}

/* --look-page: Theme's page or Auto Hours', over the shelf, as menu_run
 * draws them, the cursor on --look-page's row */
static void shot_draw_look(app *a)
{
	menu_row rows[LK_ROWS];
	const char *heading;
	hours_view hv = { .a = a };
	bool hours = !strcmp(shot_look_page, "hours");
	int n = hours ? hours_build(&hv, rows, 2, &heading) : look_build(NULL, rows, LK_ROWS, &heading);

	ui_scrim(a->r, 0);
	if (!hours) {
		g_row_mark = look_row_mark;
		g_menu_header = look_header;
		g_menu_header_h = 150;
	}
	menu_draw(a, heading, rows, n, shot_look_row < n ? shot_look_row : 0,
	          menu_std_width(a), MENU_ZYPEROS);
	g_row_mark = NULL;
	g_menu_header = NULL;
}

/* Everything a shot draws, in the theme that is on. */
int   g_shot_saver = -1;          /* --saver [cover]: 0 video, 1 cover */
float g_shot_saver_alpha = 1;     /* --saver-alpha F: how far it has faded up */
const char *g_shot_saver_pic;     /* --saver-pic FILE: its picture, a screenshot */

static void shot_compose(app *a)
{
	drawn_forget();
	ui_fit_frame();
	if (g_shot_saver >= 0) {
		saver_shot(a);
		return;
	}
	if (shot_keyboard_panel) {
		kb_pose pose = keyboard_panel(&shot_keyboard, KB_PANEL_FRAME, false);

		if (shot_keyboard.closed && !pose.moving) {
			draw_shelf(a);
			zyperos_menu_draw(a, shot_menu_sel);
		} else {
			kb_preview_panel(a->r, shot_kb_title, shot_kb_text,
			                 shot_kb_layer, shot_kb_row, shot_kb_col, pose);
		}
		return;
	}
	if (shot_poweroff) {
		poweroff_frame(a, shot_at > 0 ? (unsigned)shot_at : 0u);
		return;
	}
	muse_covers_settle();
	if (shot_menu && shot_menu_label) {
		menu_row rows[MENU_MAX_ROWS];
		menu_bufs b;
		const char *heading;
		lang was = i18n_lang();
		int n, k;

		/* The name as typed is English's, and the row is the same row in
		 * every language: found in English, drawn in the shot's */
		i18n_set(LANG_EN);
		n = menu_build(a, a->screen, a->sys_cursor, rows, &b, &heading);
		for (k = 0; k < n && !(rows[k].label && !strcmp(rows[k].label, shot_menu_label)); k++) ;
		/* In MENU, on whichever tab has it (sys_menu.h) */
		if (k == n && a->screen != SCREEN_GAMES) {
			int t, at = g_tabs.at;

			for (t = 0; t < PT_TABS && k == n; t++) {
				g_tabs.at = t;
				n = menu_build(a, a->screen, a->sys_cursor, rows, &b, &heading);
				for (k = 0; k < n && !(rows[k].label && !strcmp(rows[k].label, shot_menu_label)); k++) ;
			}
			if (k == n) {
				g_tabs.at = at;
				n = menu_build(a, a->screen, a->sys_cursor, rows, &b, &heading);
				k = n;
			}
		}
		i18n_set(was);
		if (k == n) {
			fprintf(stderr, "--menu: no row called %s\n", shot_menu_label);
			shot_failed = 1;
		}
		shot_menu_sel = k < n ? k : 0;
		shot_menu_label = NULL;   /* found once, for --flip's second draw too */
	}
	if (shot_menu && shot_key >= 0) {
		sysmenu_ctx c;

		memset(&c, 0, sizeof c);
		c.a = a;
		c.screen = a->screen;
		sysmenu_key(a, &c, (in_button)shot_key, shot_menu_sel);
		shot_key = -1;   /* once: --flip composes the shot more than once */
		/* L1 or R1 moved MENU's tab and its cursor, as the runner would;
		 * the shot is the tab at rest */
		if (g_menu_sel_to >= 0) {
			shot_menu_sel = g_menu_sel_to;
			g_menu_sel_to = -1;
			g_tabs.t0 = 0;
		}
	}
	draw_shelf(a);
	if (shot_music_page > -2) {
		bgm_view v;
		menu_row rows[BGM_ROWS + 3];
		const char *heading;
		int n, k, sel = shot_music_page;

		memset(&v, 0, sizeof v);
		bgm_scan_now(&v.list);
		n = bgm_build(&v, rows, BGM_ROWS + 3, &heading);
		/* Each step a frame, the window following the cursor as it does
		 * on the device; the last one is the shot, drawn over the shelf
		 * again */
		for (k = 0; k <= shot_music_steps; k++) {
			if (k) draw_shelf(a);
			ui_scrim(a->r, 0);
			menu_draw(a, heading, rows, n, sel, menu_std_width(a), MENU_ZYPEROS);
			if (sel >= 0) sel = menu_step_round(rows, n, sel, 1);
		}
		bgm_free(&v.list);
		return;
	}
	if (shot_language > -2) {
		static unsigned char snd[LANG_COUNT];
		menu_row rows[LANG_COUNT];
		const char *heading;
		int n = language_build(snd, rows, LANG_COUNT, &heading);

		ui_scrim(a->r, 0);
		menu_draw(a, heading, rows, n, shot_language >= 0 ? shot_language : (int)i18n_lang(),
		          menu_std_width(a), MENU_ZYPEROS);
		return;
	}
	if (shot_media) {
		media_shot(a);
		return;
	}
	if (shot_storage) {
		storage_shot(a);
		return;
	}
	if (shot_update >= 0) {
		su_page p;
		char notes[UP_NOTES][UP_NOTE_W];
		const char *np[UP_NOTES];
		int nn = up_notes(notes), i;

		for (i = 0; i < UP_NOTES; i++) np[i] = notes[i];

		su_page_build(up_view(true), &p);
		update_page_draw(a, T(S_UPDATEPAGE_TITLE), &p, np, nn, NULL);
		return;
	}
	if (g_pm_shot >= 0) {
		static system_cfg ports = { .folder = "Ports", .core = "port" };
		char notes[PM_NOTES][120];
		su_page p;
		int nn;

		SDL_Texture *icon = pm_icon_tex(a);
		const pm_view *v = pm_view_read(&ports, true);

		pm_page_build(v, &p, notes, &nn);
		{
			const char *np[PM_NOTES];
			int i;

			for (i = 0; i < PM_NOTES; i++) np[i] = notes[i];
			update_page_draw(a, T(v->repair ? S_PORTMASTER_REINSTALL_TITLE : S_PORTMASTER_TITLE),
			                 &p, np, nn, icon);
		}
		if (icon) SDL_DestroyTexture(icon);
		return;
	}
	if (shot_about >= 0) {
		static about_view v;
		static bool made;

		/* Once: a shot that draws more than one frame keeps the photo */
		if (!made) {
			made = true;
			about_refresh(&v, 2700000u);
			about_pages(a, &v);
			v.width = about_width(a, &v);
			if (shot_about_scroll >= 0) {
				int over = v.nstory - menu_notes_fit();

				v.story_scroll = over > 0 ? (int)(shot_about_scroll * over * menu_note_h()) : 0;
			}
		}
		ui_scrim(a->r, 0);
		about_draw(a, &v, shot_about);
		return;
	}
	if (shot_menu && shot_tab_from >= 0) {
		if (!g_clock_held) { g_clock_held = true; g_clock_at = plat_now_ms(); }
		g_tabs.from = shot_tab_from;
		g_tabs.dir = (g_tabs.at - shot_tab_from + PT_TABS) % PT_TABS == 1 ? +1 : -1;
		g_tabs.t0 = g_clock_at - (unsigned)shot_tab_ms;
		if (!g_tabs.t0) g_tabs.t0 = 1;
		g_menu_epoch++;
		shot_tab_from = -1;
	}
	if (shot_menu && shot_menu_steps) {
		menu_row rows[MENU_MAX_ROWS];
		menu_bufs b;
		const char *h;
		int n = menu_build(a, a->screen, a->sys_cursor, rows, &b, &h);
		int k, d = shot_menu_steps > 0 ? 1 : -1;

		if (!g_clock_held) { g_clock_held = true; g_clock_at = plat_now_ms(); }
		for (k = 0; k < (shot_menu_steps > 0 ? shot_menu_steps : -shot_menu_steps); k++) {
			zyperos_menu_draw(a, shot_menu_sel);
			g_clock_at += 1000;
			shot_menu_sel = menu_step_round(rows, n, shot_menu_sel, d);
		}
		/* and the last press drawn, then let settle: the shot is the list
		 * at rest after it */
		zyperos_menu_draw(a, shot_menu_sel);
		g_clock_at += 1000;
		shot_menu_steps = 0;    /* once: --flip composes the shot twice */
		draw_shelf(a);
	}
	/* After --close's press the menu is the shelf's to draw leaving, and
	 * after --swap's it has given way to the page below */
	if (shot_menu) {
		if (shot_pressed && shot_press == SHOT_CLOSE) panel_leave_draw(a, 0);
		else if (!shot_pressed || shot_press != SHOT_SWAP)
			zyperos_menu_draw(a, shot_menu_sel);
	}
	if (shot_look_page) shot_draw_look(a);
	if (shot_slots) shot_draw_slots(a);
	if (shot_gamemenu && a->sys.count > 0) shot_draw_gamemenu(a);
	else if (g_shot_p2 && a->sys.count > 0) shot_draw_player2(a);
	if (shot_gamecontrols >= 0 && a->sys.count > 0) shot_draw_gamecontrols(a);
	if (shot_controls_fit) controls_fit_report(a);
	if (shot_handheld_box) handheld_box_report();
	/* Over **scrim.menu**, as the screens they picture are: kb_prompt's
	 * backdrop, wait_panel and the achievement list opened over the shelf. */
	if (shot_cheevos && shot_ingame) {
		/* as opened from the in-game menu: over a paused game */
		SDL_Texture *bg = fake_frame(a->r, 4.0f / 3);

		paused_backdrop(a, bg, 1.0f);
		if (bg) SDL_DestroyTexture(bg);
	} else if (shot_kb || shot_wait || shot_cheevos) {
		ui_scrim(a->r, 0);
	}
	if (shot_cheer >= 0) {
		cheer_ctx c;

		zr_init(&c.z, 0, 0);
		zr_works(&c.z, 0);
		c.now = (unsigned)shot_cheer;
		eh_cheer_draw(a->r, games_on_card(a), (unsigned)shot_cheer, cheer_zyper, &c);
	}
	if (shot_kb) {
		if (shot_at >= 0)
			kb_preview_press(a->r, shot_kb_title, shot_kb_text,
			                 shot_kb_layer, shot_kb_row, shot_kb_col, (unsigned)shot_at);
		else kb_preview(a->r, shot_kb_title, shot_kb_text,
		                shot_kb_layer, shot_kb_row, shot_kb_col);
	}
	/* The in-game notice is not drawn by this process - Diatom composites it
	 * over the game - so it cannot be screenshotted like the rest. Rendered
	 * to its wire format and read straight back, which is the same pixels
	 * Diatom will show and therefore the thing worth looking at. */
	/* The achievements list, with sample data. It is only reachable from the
	 * in-game menu, so without this it can only be judged on a device with a
	 * game running - and it took two rounds of that to notice its heading was
	 * clipped at both ends. */
	/* The one-line panel behind "Scanning...", "Connecting...", "Signing
	 * in..." and "Looking up this game...". Five flows use it and none of them
	 * could be rendered. */
	if (shot_wait) {
		menu_row row = { shot_wait_msg ? shot_wait_msg : T(S_LIBRARY_SCANNING), NULL, false };

		menu_message(a, shot_wait, row, MENU_ZYPEROS);
	}
	if (shot_info == 4) info_buttons_preview(a, shot_gm_pick);
	else if (shot_info) info_preview(a, true, shot_info == 2 ? T(S_GAMEMENU_DELETE)
	                                     : shot_info == 3 ? T(S_GAMEMENU_RENAME) : NULL);
	if (shot_delete_ask) delete_ask_preview(a);
	if (shot_controls >= 0 && (shot_press != SHOT_SWAP || shot_pressed)) {
		menu_row rows[CTL_MAX_ROWS];
		char head[80], count[16];
		ctl_page p = (ctl_page)(shot_controls % CTL_PAGES);
		float k = 1.0f;
		int n;

		ctl_set_fn_volume(plat_brightness_on_volume());
		ctl_set_battery_lights(plat_battery_lights());
		n = ctl_rows(p, rows);

		snprintf(head, sizeof head, T(S_CONTROLS_HEADING), ctl_page_name(p));
		snprintf(count, sizeof count, "%d / %d", (int)p + 1, CTL_PAGES);
		/* --swap's page, arriving over the menu that gave way to it */
		if (shot_press == SHOT_SWAP && shot_lv) panel_frame(a, shot_lv, &k);
		ui_scrim_k(a->r, 0, k);
		menu_draw_caption(a, head, rows, n, -1, menu_std_width(a), MENU_ZYPEROS, NULL, false,
		                  0, count);
	}
	if (shot_sgdb >= 0) {
		static const char *const st[] = { NULL, "works", "refused" };
		menu_row rows[SG_ROWS];
		sg_row ids[SG_ROWS];
		int n = sys_menu_sgdb_rows(shot_sgdb ? "\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2" "1a2b"
		                                     : NULL, st[shot_sgdb], rows, ids);

		ui_scrim(a->r, 0);
		menu_draw(a, "SteamGridDB", rows, n, 0, menu_std_width(a), MENU_ZYPEROS);
	}
	if (shot_scraper == 3) {
		static snaps_bufs b;
		snaps_console c[5] = { { "SNES", true, 212000000ull }, { "Game Boy", true, 96000000ull },
		                       { "PlayStation", false, 0 }, { "Genesis", true, 154000000ull },
		                       { "NES", true, 188000000ull } };
		snaps_ui u = { .play = true, .ss_in = true, .ss_have = true,
		               .sound = SNAP_MUTED,
		               .c = c, .nc = 5, .total = 650000000ull };
		menu_row rows[SN_ROWS];
		sn_row ids[SN_ROWS];
		int con[SN_ROWS], n = sys_menu_snaps_rows(&u, &b, rows, ids, con);

		ui_scrim(a->r, 0);
		menu_draw(a, T(S_SNAPS_HEADING), rows, n, 4, menu_std_width(a), MENU_ZYPEROS);
	} else if (shot_scraper == 4) {
		menu_row rows[SSP_ROWS];
		ssp_row ids[SSP_ROWS];
		int n = sys_menu_ss_rows(true, "tarik", "19796 left today", rows, ids);

		ui_scrim(a->r, 0);
		menu_draw(a, "ScreenScraper", rows, n, 0, menu_std_width(a), MENU_ZYPEROS);
	}
	if (shot_np >= 0) np_shot(a);
	if (shot_usb >= 0) {
		/* The question a computer gets, at rest, as usb_ask asks it: all three tiles,
		 * or with --usb-ask N plain the two a card without the development image has. In the language that is on, since the
		 * overflow is in the translation */
		menu_row tiles[4];
		int what[4], marks[4];
		int nt = usb_ask_tiles(tiles, what, marks, shot_usb_dev);

		ui_scrim(a->r, 0);
		tiles_panel(a, PANEL_TILES_GRID, T(S_XFER_COMPUTER_CONNECTED), tiles, marks, nt, shot_usb % nt,
		            PANEL_REST.panel);
	}
	if (shot_scraper_page >= 0) {
		scraper_page_shot(a);
		return;
	}
	if (shot_usbdrive >= 0 && shot_usbdrive < UD_FRAMES) {
		usb_drive_draw(a, shot_usbdrive);
		return;
	}
	/* usbdrive's atlas, and where its glyphs are, on stdout */
	if (shot_usbdrive == UD_FRAMES) {
		usb_drive_atlas(a, NULL, NULL, stdout);
		return;
	}
	if (shot_power >= 0) {
		menu_row rows[UP_TILES + 1];

		ui_scrim(a->r, 0);
		if (shot_power < UP_TILES) {
			int nt = usb_power_rows(rows);

			tiles_panel(a, PANEL_LIST, T(S_XFER_USB_DEVICE_CONNECTED), rows, UP_MARK, nt, shot_power,
			            PANEL_REST.panel);
		} else {
			/* The answer's message over the shelf, as the overlay draws it */
			g_replug.say = shot_power == UP_TILES ? USBPOWER_SAY_COMPUTER
			             : shot_power == UP_TILES + 1 ? USBPOWER_SAY_PHONE : USBPOWER_SAY_IPHONE;
			replug_draw_at(a->r);
		}
	}
	if (shot_usb_said) {
		/* USB Accessories turned to Automatic: its message, as
		 * usb_auto_message lays it out */
		char lines[4][LB_LINE];
		menu_row rows[4];
		int n;

		ui_scrim(a->r, 0);
		n = usb_auto_lines(a, lines, rows);
		menu_draw(a, T(S_SYSMENU_USB_ACCESSORIES), rows, n, -1, menu_std_width(a), MENU_ZYPEROS);
	}
	if (shot_usb_offer) {
		/* Zyper's offer of Automatic, as usb_offer_show lays it out, the
		 * cursor on Keep Asking */
		char lines[USB_OFFER_LINES][LB_LINE];
		menu_row rows[USB_OFFER_LINES + 3];
		int n;

		ui_scrim(a->r, 0);
		n = usb_offer_rows(a, lines, rows);
		rows[n]     = (menu_row){ T(S_SYSMENU_USB_AUTO), NULL, true };
		rows[n + 1] = (menu_row){ T(S_XFER_USB_OFFER_KEEP), NULL, true };
		menu_draw(a, T(S_XFER_USB_OFFER), rows, n + 2, n + 1, menu_std_width(a), MENU_ZYPEROS);
	}
	if (shot_datetime >= 0) {
		/* Date & Time's page as clock_build builds it, at a time of its own,
		 * in the language that is on */
		static const char *const ids[4] = { "America/New_York", "America/Sao_Paulo",
		                                     "America/Bahia_Banderas", "America/New_York" };
		struct tm tm = { .tm_year = 2026 - 1900, .tm_mon = 9, .tm_mday = 3,
		                 .tm_hour = 18, .tm_min = 5, .tm_isdst = -1 };
		menu_row rows[CLOCK_ZONE_LIST_MAX];
		clock_ui c;
		int k = shot_datetime % 9, n;

		memset(&c, 0, sizeof c);
		ui_scrim(a->r, 0);
		if (k >= 5) {
			static const char *const list_ids[4] = { "Pacific/Honolulu", "Africa/Johannesburg",
			                                         "America/Sao_Paulo", "America/Bahia_Banderas" };
			static zone_list_ui z;
			const char *zid = list_ids[k - 5];
			bool autoz = k >= 7;

			n = zone_list_rows(&z, zid, autoz, autoz ? zid : "", rows, CLOCK_ZONE_LIST_MAX);
			menu_draw(a, T(S_CLOCK_TIME_ZONE), rows, n, clock_zone_list_row(zid, autoz),
			          menu_std_width(a), MENU_ZYPEROS);
		} else if (k == 4) {
			char lines[2][LB_LINE];

			n = zone_auto_lines(a, lines, rows);
			menu_draw(a, T(S_CLOCK_TIME_ZONE), rows, n, -1, menu_std_width(a), MENU_ZYPEROS);
		} else {
			n = clock_page_rows(&c, mktime(&tm), ids[k], k < 3,
			                    k == 1 || k == 2 ? ids[k] : "", true, rows, CLOCK_ROWS + 4);
			menu_draw(a, T(S_DATETIME_TITLE), rows, n, CLOCK_ROW_ZONE, menu_std_width(a),
			          MENU_ZYPEROS);
		}
	}
	if (shot_dev >= 0) {
		/* Developer Mode's page, as dev_screen builds it, and the message
		 * the unlock leaves, in the language that is on */
		char cmd[96], keys[48];
		menu_row rows[6];
		int n;

		ui_scrim(a->r, 0);
		if (shot_dev < 3) {
			n = dev_rows(rows, cmd, sizeof cmd, keys, sizeof keys, true, "10.42.0.1", "x7k4mq2d",
			             shot_dev == 0 ? 1 : shot_dev == 1 ? 12 : 0, T(S_SSH_WAITING));
			menu_draw(a, T(S_XFER_DEV_HEADING), rows, n, -1, menu_std_width(a), MENU_ZYPEROS);
		} else {
			rows[0] = (menu_row){ T(S_DEVMODE_IS_ON), NULL, false };
			rows[1] = (menu_row){ T(S_DEVMODE_WHERE), NULL, false };
			menu_draw(a, T(S_SYSMENU_DEVELOPER), rows, 2, -1, 0, MENU_ZYPEROS);
		}
	}
	if (shot_xfer_ask >= 0) {
		/* The question while files move, from the same words xs_ask shows */
		const char *const *l = shot_xfer_game ? XS_GAME_L : XS_STOP_L;
		const char *const *w = shot_xfer_game ? XS_GAME_W : XS_STOP_W;
		int k = shot_xfer_ask % 3;
		char said[128];
		menu_row rows[6] = {
			{ l[0], NULL, true }, { l[1], NULL, true }, { l[2], NULL, true },
			MENU_NOTE(said), MENU_NOTE(w[k]),
			MENU_NOTE(shot_xfer_game ? XS_GAME_LEGEND : XS_STOP_LEGEND),
		};

		snprintf(said, sizeof said, T(S_XFER_RECEIVING), "Tobu Tobu Girl.gb");
		ui_scrim(a->r, 0);
		tiles_panel(a, PANEL_TILES, T(S_XFER_STILL_MOVING), rows, shot_xfer_game ? XS_GAME_M : XS_STOP_M, 6, k,
		            PANEL_REST.panel);
	}
	if (shot_steps_way) {
		static const struct { const char *name; int step, sub, hint; } ST[] = {
			{ "unplugged",  LS_LINK,    0, LH_NONE },
			/* a computer that has not taken the handheld yet, and one
			 * that took it and has not opened its network: the loading bar */
			{ "taking",     LS_LINK,    1, LH_NONE },
			{ "opening",    LS_LINK,    2, LH_NONE },
			{ "nocomputer", LS_LINK,    0, LH_NO_COMPUTER },
			{ "noanswer",   LS_LINK,    1, LH_NO_ANSWER },
			{ "nodriver",   LS_LINK,    2, LH_NO_DRIVER },
			{ "address",    LS_ADDRESS, 0, LH_NONE },
			{ "noaddress",  LS_ADDRESS, 0, LH_NO_ADDRESS },
			{ "browser",    LS_BROWSER, 0, LH_NONE },
			{ "nobrowser",  LS_BROWSER, 0, LH_NO_BROWSER },
			{ "nonetwork",  LS_LINK,    3, LH_NO_NETWORK },
			{ "waits",      LS_LINK,    4, LH_NONE },
			{ "replug",     LS_LINK,    4, LH_REPLUG },
			/* the page open in a browser, its PIN not in yet */
			{ "pin",        LS_DONE,    0, LH_NONE },
		};
		const bool phone = !strcmp(shot_steps_way, "phone");
		const bool cable = strcmp(shot_steps_way, "wifi") != 0 && !phone;
		const char *addr = cable ? "10.42.0.1" : phone ? "192.168.42.37" : "192.168.1.42";
		link_steps st = { 0 };
		menu_row rows[STEP_ROWS];
		char head[192], hint[320], lines[3][LB_LINE];
		int k, n, h = LH_NONE;

		st.started = true;
		for (k = 0; k < (int)(sizeof ST / sizeof ST[0]); k++)
			if (!strcmp(ST[k].name, shot_steps_state)) {
				st.step = ST[k].step;
				st.sub = ST[k].sub;
				h = ST[k].hint;
			}
		/* Off Wi-Fi the heading says so, with no name, as the screen does */
		const char *name = shot_steps_noname || phone ? "" : "zyperos.local";

		if (!cable && st.step == LS_LINK) hare_head(head, sizeof head, false, NULL, T(S_XFER_WIFI_GONE), "4071");
		else hare_head(head, sizeof head, cable, name, addr, "4071");
		draw_shelf(a);
		ui_scrim(a->r, 0);
		/* As the screen chooses (xfer_screen_way): the way in, or the steps
		 * while one before the browser stalls */
		if (xs_way_hint(h)) {
			n = xs_step_rows(a, rows, lines, hint, sizeof hint, &st, h, cable,
			                 b_legend(S_KEYHINT_CLOSE));
			menu_draw(a, head, rows, n, -1, menu_std_width(a), MENU_ZYPEROS);
		} else {
			xs_way_set(cable, phone, name, shot_steps_addr ? shot_steps_addr : addr, "4071");
			n = xs_way_rows(rows, &st, cable || phone, b_legend(S_KEYHINT_CLOSE));
			/* over the cable, before the address: the loading bar, its
			 * step waited on for a second and a half */
			if (cable && st.step < LS_BROWSER)
				xs_load_draw(a, rows, n, xs_load_target(&st, 1500));
			else
				xs_way_draw(a, rows, n);
		}
	}
	if (shot_hare || (shot_hot && shot_hot_in)) {
		/* The tiles as the screen sets them (xs_move_set), from a
		 * session's numbers over Wi-Fi (--hare-move): nothing moving after
		 * a file came in, a file coming in (its bar at 62%), one going
		 * out, both, closing after the one coming in, Play after this
		 * file's wait, and Wi-Fi gone with the totals kept. `now` is the
		 * Now row's words for the file (shot_hare_head). */
		static const struct {
			const char *state;
			bool on[2];
			unsigned long long rate[2];
			int bar;                /* per mille of the upload; -1 none */
			str_id now;
		} MV[] = {
			{ "idle",    { false, false }, { 0, 0 },                   -1,  S_HARE_RECEIVED },
			{ "in",      { true, false },  { 2400000ull, 0 },          620, S_HARE_RECEIVING },
			{ "out",     { false, true },  { 0, 3100000ull },          -1,  S_HARE_SENDING },
			{ "both",    { true, true },   { 1800000ull, 1200000ull }, 350, S_HARE_RECEIVING },
			{ "closing", { true, false },  { 2400000ull, 0 },          840, S_XFER_CLOSING_AFTER },
			{ "finish",  { true, false },  { 2400000ull, 0 },          840, S_XFER_FINISHING },
			{ "gone",    { false, false }, { 0, 0 },                   -1,  S_HARE_RECEIVED },
		};
		int m = 1, k;
		xs_flow flow[2] = { { 186000000ull, 0, 0, 0, false }, { 41000000ull, 0, 0, 0, false } };
		char now[192];

		for (k = 0; k < (int)(sizeof MV / sizeof MV[0]); k++)
			if (shot_hare_move && !strcmp(shot_hare_move, MV[k].state)) m = k;
		for (k = 0; k < 2; k++) {
			flow[k].active = MV[m].on[k];
			flow[k].rate = MV[m].rate[k];
		}
		xs_move_set(flow, MV[m].bar, MV[m].bar >= 0 ? 1000 : 0);
		snprintf(now, sizeof now, T(MV[m].now), shot_hare_head);
		if (shot_hot && shot_hot_in) {
			/* the hotspot once someone came in: the news, its network,
			 * password and PIN in the heading (hot_head) */
			menu_row rows[5];
			char head[192];
			int n = xs_page_rows(rows, shot_hot_phones, NULL, shot_hot_now, S_KEYHINT_CLOSE);

			hot_head(head, sizeof head, menu_std_width(a), shot_hot_ssid, shot_hot_pass, "4071");
			draw_shelf(a);
			ui_scrim(a->r, 0);
			xs_page_show(a, head, rows, n);
		} else if (m == 5) {
			/* Play after this file's wait, as xs_before_game draws it */
			menu_row rows[3];

			draw_shelf(a);
			ui_scrim(a->r, 0);
			xs_page_show(a, XFER_NAME_WIFI, rows, xs_finish_rows(rows, now, sizeof now, shot_hare_head));
		} else {
			const bool gone = m == 6;

			hare_preview(a, gone ? "" : "zyperos.local", gone ? T(S_XFER_WIFI_GONE) : shot_hare_addr,
			             "4071", shot_hare_who,
			             /* the cable's address is USB File Transfer, with SSH */
			             !strcmp(shot_hare_addr, "10.42.0.1") ? "1 signed in" : NULL, now,
			             m == 4 ? S_KEYHINT_HIDE
			             : MV[m].on[0] || MV[m].on[1] ? S_KEYHINT_KEEP_GOING_OR_STOP : S_KEYHINT_CLOSE);
		}
	} else if (shot_hot)
		hot_preview(a, shot_hot_ssid, shot_hot_pass, shot_hot_phones, shot_hot_now);
	/* The synopsis card, from the card's own games table - so a shot of it is a
	 * picture of real scraped text rather than a fixture. */
	if (shot_syn) {
		sysview *v = &a->view[a->sys_cursor];
		const system_cfg *s = &a->sys.systems[shelf_owner(a, a->sys_cursor, v->cursor)];
		game_meta m;

		if (v->list.count > 0 &&
		    db_game_get(db_lib(), s->folder, v->list.items[v->cursor].file, &m) &&
		    m.synopsis[0]) {
			char lines[SYN_WRAP_LINES][CHV_WRAP_COLS];
			menu_row rows[SYN_ROWS];
			int fixed = menu_std_width(a);
			int loop_at = 0;
			int nl = syn_layout(m.synopsis, fixed, lines, rows, &loop_at);

			ui_scrim(a->r, 0);
			menu_draw_ex(a, v->list.items[v->cursor].title, rows, nl, -1, fixed,
			             s->accent, NULL, false, loop_at);
		}
	}
	if (shot_cheevos) {
		/* Real titles, and the long ones are the point: a set's names run past
		 * the panel often, and this shot drew eight short ones - so the screen
		 * could be looked at without a device and still not show the thing
		 * worth looking at. Two of the three below are real RetroAchievements
		 * titles from sets on this card. */
		static const struct { const char *t; int p; bool got; } sample[] = {
			{ "The Path to Disaster", 5, true },
			{ "Hold On To Your Potatoes, We're In For A Bumpy Ride", 10, true },
			{ "Violated Heavens", 10, true },
			{ "I Am Not Left Handed Either, But I Can Still Beat You", 25, false },
			{ "Koma Faction's Fall", 25, false },
			{ "Not So Disaster", 10, false },
			{ "Storming The Fortress", 10, false },
			{ "Blazing through the Skies", 10, false },
		};
		int cn = (int)(sizeof sample / sizeof sample[0]), ci;
		menu_row rows[8];
		unsigned vcols[8] = { 0 };
		char vals[8][16], head[192];

		for (ci = 0; ci < cn; ci++) {
			snprintf(vals[ci], sizeof vals[ci], "%d", sample[ci].p);
			rows[ci] = (menu_row){ sample[ci].t, vals[ci], sample[ci].got };
			vcols[ci] = sample[ci].got ? VCOL_EARNED : 0u;
		}
		if (shot_cheevos_waiting > 0)
			snprintf(head, sizeof head, TN(S_CHEEVOLIST_HEADING_WAITING, shot_cheevos_waiting),
			         shot_cheevos_game, 3, 36, shot_cheevos_waiting);
		else
			snprintf(head, sizeof head, T(S_CHEEVOLIST_HEADING),
			         shot_cheevos_game, 3, 36, 25, 415);
		/* Drawn the way cheevos_screen draws it - menu_draw_ex, the earned
		 * colors, the shared width - because a shot that composes the panel
		 * differently from the screen is a picture of something that does not
		 * exist. It used to call menu_draw with a width of 0. */
		for (ci = 0; ci < cn; ci++) shot_badge_got[ci] = sample[ci].got;
		g_badge_r = a->r;
		g_row_icon = shot_row_badge;
		menu_draw_ex(a, head, rows, cn, shot_cheevos_sel, menu_std_width(a),
		             a->tint, vcols, true, 0);
		g_row_icon = NULL;
	}
	if (shot_popup) {
		static char pbody[96];
		char slot[32];

		snprintf(slot, sizeof slot, T(S_SLOTS_SLOT), 2);
		shot_notice = pbody;
		snprintf(pbody, sizeof pbody, "%s", slot);
		if (!strcmp(shot_popup, "saved"))             shot_notice_head = T(S_POPUP_SAVED);
		else if (!strcmp(shot_popup, "loaded"))       shot_notice_head = T(S_POPUP_LOADED);
		else if (!strcmp(shot_popup, "save_failed"))  shot_notice_head = T(S_POPUP_SAVE_FAILED);
		else if (!strcmp(shot_popup, "load_failed"))  shot_notice_head = T(S_POPUP_LOAD_FAILED);
		else if (!strcmp(shot_popup, "slot_empty"))   shot_notice_head = T(S_POPUP_SLOT_EMPTY);
		else if (!strcmp(shot_popup, "no_states")) {
			shot_notice_head = T(S_POPUP_NO_STATES);
			snprintf(pbody, sizeof pbody, "%s", "Super Metroid");
		} else if (!strcmp(shot_popup, "dslang")) {
			/* A DS game started fresh in a new Game Language (ds_resume_fresh) */
			shot_notice_head = T(S_POPUP_DSLANG_HEAD);
			snprintf(pbody, sizeof pbody, "%s", T(S_POPUP_DSLANG_BODY));
		} else if (!strcmp(shot_popup, "corestate")) {
			/* A game started fresh, its resume left by the other core
			 * (core_resume_fresh) */
			shot_notice_head = T(S_POPUP_CORESTATE_HEAD);
			snprintf(pbody, sizeof pbody, "%s", T(S_POPUP_CORESTATE_BODY));
		} else if (!strcmp(shot_popup, "player2_joined") || !strcmp(shot_popup, "player2_left")) {
			/* A friend's browser joining NES Player 2 or leaving it
			 * (p2op_notice) */
			shot_notice_head = T(S_PLAYER2_NOTICE);
			snprintf(pbody, sizeof pbody,
			         T(!strcmp(shot_popup, "player2_joined") ? S_PLAYER2_JOINED : S_PLAYER2_LEFT), 2);
		} else if (!strcmp(shot_popup, "player2_stopped")) {
			/* ...and Player 2 stopping by itself during the game */
			shot_notice_head = T(S_PLAYER2_NOTICE);
			snprintf(pbody, sizeof pbody, "%s", T(S_PLAYER2_STOPPED));
		} else if (!strcmp(shot_popup, "stick_on") || !strcmp(shot_popup, "stick_off")) {
			/* R2 in a PSP game (on_stick) */
			shot_notice_head = T(S_POPUP_STICK);
			snprintf(pbody, sizeof pbody, "%s",
			         T(!strcmp(shot_popup, "stick_on") ? S_POPUP_STICK_ON : S_POPUP_STICK_OFF));
		} else {
			shot_notice_head = T(S_POPUP_SPEED);
			snprintf(pbody, sizeof pbody,
			         shot_popup_half % 2 ? T(S_POPUP_SPEED_HALF) : T(S_POPUP_SPEED_WHOLE),
			         shot_popup_half / 2);
		}
	}
	if (shot_notice) {
		char dt[512];
		FILE *nf;

		snprintf(dt, sizeof dt, "%s.dtov", shot_path);
		if ((shot_popup ? notice_render(shot_notice_head, shot_notice, dt, theme_index())
		     : shot_notice_mark
		     ? notice_render_mark(shot_notice_head, shot_notice, (notice_mark)shot_notice_mark,
		                          dt, theme_index())
		     : notice_render_ex(shot_notice_head, shot_notice, shot_notice_badge,
		                        notice_look(shot_notice_kind), shot_notice_value,
		                        shot_notice_target, dt, theme_index())) &&
		    (nf = fopen(dt, "rb"))) {
			unsigned char hd[8];
			if (fread(hd, 1, 8, nf) == 8 && !memcmp(hd, "DTOV", 4)) {
				int nw = hd[4] | (hd[5] << 8), nh = hd[6] | (hd[7] << 8);
				SDL_Surface *ns = SDL_CreateRGBSurfaceWithFormat(
					0, nw, nh, 32, SDL_PIXELFORMAT_ARGB8888);
				if (ns && fread(ns->pixels, 4, (size_t)nw * nh, nf)
				          == (size_t)nw * nh) {
					SDL_Rect at = { (ZYPEROS_SCREEN_W - nw) / 2,
					                ZYPEROS_SCREEN_H - nh - ZYPEROS_SCREEN_H / 24,
					                nw, nh };
					SDL_Texture *nt = SDL_CreateTextureFromSurface(a->r, ns);
					if (nt) {
						SDL_SetTextureBlendMode(nt, SDL_BLENDMODE_BLEND);
						SDL_RenderCopy(a->r, nt, NULL, &at);
						SDL_DestroyTexture(nt);
					}
				}
				if (ns) SDL_FreeSurface(ns);
			}
			fclose(nf);
			remove(dt);
		}
	}
	/* The song pop-up, on a clock of its own so the frame is the same each
	 * time: shown at T0, drawn --songpop's MS into it */
	if (shot_songpop) {
		const unsigned T0 = 100000;
		const char *bar = strchr(shot_songpop, '|');
		char head[BGM_TAG_MAX], body[BGM_TAG_MAX];

		snprintf(head, sizeof head, "%.*s", bar ? (int)(bar - shot_songpop) : 0, shot_songpop);
		snprintf(body, sizeof body, "%s", bar ? bar + 1 : shot_songpop);
		zr_init(&g_songpop_zyper, T0, 1);
		if (!head[0]) snprintf(head, sizeof head, "%s", BGM_SONG_HEAD_NONE);
		songpop_show(&g_songpop, head, body, song_hold(head, body), T0);
		zr_works(&g_songpop_zyper, T0);
		song_popup_draw(a, T0 + (unsigned)shot_songpop_ms);
	}
	if (shot_heatpop >= 0) {
		const unsigned T0 = 100000;

		heatpop_show(T0);
		if (shot_ranews > 0) {
			snprintf(g_heatpop.line1, sizeof g_heatpop.line1, TN(S_CHEEVOS_SENT, shot_ranews), shot_ranews);
			snprintf(g_heatpop.line2, sizeof g_heatpop.line2, "%s", T(S_CHEEVOS_SENT_WHERE));
		}
		if (shot_upnews) {
			snprintf(g_heatpop.line1, sizeof g_heatpop.line1,
			         T(strcmp(shot_upnews, "ready") ? S_UPDATE_NEWS_AVAILABLE : S_UPDATE_NEWS_READY), "1.0.4");
			snprintf(g_heatpop.line2, sizeof g_heatpop.line2, "%s", T(S_UPDATE_NEWS_WHERE));
		}
		zr_init(&g_heatpop.rig, T0, 1);
		zr_works(&g_heatpop.rig, T0 + DS_MS_MOVE);
		heatpop_draw(a, T0 + (unsigned)shot_heatpop);
	}
	/* The replug message (--replug) where the overlay goes: over everything
	 * else, under the bar */
	if (g_replug.on) replug_draw_at(a->r);
	/* The volume or brightness bar over whatever else was asked for, held
	 * until it has slid all the way in. */
	if (shot_osd_kind) {
		plat_osd_show(shot_osd_kind, shot_osd_val,
		              shot_osd_kind == 1 ? PLAT_BRIGHT_MAX : PLAT_VOL_MAX);
		SDL_Delay(OSDBAR_IN_MS + 10);
		plat_draw_osd(a->r);
	}
}

/* The row --shot-at's plate leaves for --menu's: the nearest row above it
 * the cursor can rest on, or below it when there is none above. */
static int shot_menu_from(app *a, int sel)
{
	menu_row rows[MENU_MAX_ROWS];
	menu_bufs bufs;
	const char *heading;
	int n = menu_build(a, a->screen, a->sys_cursor, rows, &bufs, &heading), i;

	for (i = (sel < n ? sel : n) - 1; i >= 0; i--)
		if (rows[i].label && rows[i].live) return i;
	for (i = sel + 1; i < n; i++)
		if (rows[i].label && rows[i].live) return i;
	return sel;
}

/* The row of the top menu called `label` in English, or -1: the same row in
 * every language, found in English as --menu's is */
static int shot_menu_row(app *a, const char *label)
{
	menu_row rows[MENU_MAX_ROWS];
	menu_bufs bufs;
	const char *heading;
	lang was = i18n_lang();
	int n, i;

	i18n_set(LANG_EN);
	n = menu_build(a, a->screen, a->sys_cursor, rows, &bufs, &heading);
	for (i = 0; i < n; i++)
		if (rows[i].label && !strcmp(rows[i].label, label)) break;
	/* In MENU, on whichever tab has it, and that tab is then on */
	if (i == n && a->screen != SCREEN_GAMES) {
		int t, at = g_tabs.at;

		for (t = 0; t < PT_TABS && i == n; t++) {
			g_tabs.at = t;
			n = menu_build(a, a->screen, a->sys_cursor, rows, &bufs, &heading);
			for (i = 0; i < n; i++)
				if (rows[i].label && !strcmp(rows[i].label, label)) break;
		}
		if (i == n) g_tabs.at = at;
	}
	i18n_set(was);
	return i < n ? i : -1;
}

/* --gamemenu's motion: at rest as it was, then the press at `t0` - L1 or
 * R1 from --gm-tab-from's tab, or the Controls tab's keys - for
 * shot_motion's frames to go on from */
static void shot_gm_motion(app *a, unsigned t0)
{
	int to = shot_gamemenu_tab;
	const char *k;

	if (shot_gm_from >= 0) {
		shot_gamemenu_tab = shot_gm_from;
		shot_compose(a);                         /* at rest on the tab before */
		shot_gamemenu_tab = to;
		shot_gm_t0 = t0 ? t0 : 1;
		if (to % GT_TABS == GT_CONTROLS) ctlscr_enter();
		g_menu_epoch++;
		return;
	}
	shot_compose(a);                             /* at rest on its row */
	for (k = shot_gm_press; k && *k; ) {
		size_t len = strcspn(k, ",");
		ctlscr_game g;
		menu_row rows[CT_MAX_ROWS];
		unsigned vcol[CT_MAX_ROWS];
		int ids[CT_MAX_ROWS], n;

		gm_ctl_game(a, NULL, &g);
		n = ctlscr_rows(a, &g, rows, vcol, NULL, ids);
		if (shot_gamemenu_sel >= n) shot_gamemenu_sel = n - 1;
		if (len == 2 && !strncmp(k, "up", 2))
			shot_gamemenu_sel = menu_step_round(rows, n, shot_gamemenu_sel, -1);
		else if (len == 4 && !strncmp(k, "down", 4))
			shot_gamemenu_sel = menu_step_round(rows, n, shot_gamemenu_sel, +1);
		else if (len == 4 && !strncmp(k, "left", 4))
			ctlscr_key(a, &g, IN_LEFT, ids[shot_gamemenu_sel]);
		else if (len == 5 && !strncmp(k, "right", 5))
			ctlscr_key(a, &g, IN_RIGHT, ids[shot_gamemenu_sel]);
		else
			fprintf(stderr, "--gm-press: %.*s is no key here\n", (int)len, k);
		/* ...and the cursor where the key put it, as the runner takes it */
		if (g_menu_sel_to >= 0) {
			shot_gamemenu_sel = g_menu_sel_to;
			g_menu_sel_to = -1;
		}
		k += len + (k[len] == ',');
	}
	/* Where the cursor ended, off a quiet row as the runner steps off one,
	 * for the shot checks to read */
	if (shot_gm_press) {
		ctlscr_game g;
		menu_row rows[CT_MAX_ROWS];
		unsigned vcol[CT_MAX_ROWS];
		int ids[CT_MAX_ROWS], n, sel;

		gm_ctl_game(a, NULL, &g);
		n = ctlscr_rows(a, &g, rows, vcol, NULL, ids);
		sel = shot_gamemenu_sel < n ? shot_gamemenu_sel : n - 1;
		if (!rows[sel].live) {
			sel = menu_step_sel(rows, n, sel, +1);
			if (!rows[sel].live) sel = menu_step_sel(rows, n, sel, -1);
		}
		fprintf(stderr, "gm-press: the cursor on %s, %s\n", rows[sel].label,
		        rows[sel].value && !ROW_IS_NOTE(rows[sel]) ? rows[sel].value : "");
	}
}

/* --shot-at: the press, and the frames after it up to the shot's time.
 *
 * The clock is held at the press and moved on a frame at a time, 60 a second
 * as on the device, each frame drawn and thrown away, and then to the shot's
 * time exactly, which take_shot draws. The tweens would land in the same
 * place from the press alone; the frames are for what follows from one frame
 * to the next - the tint, which steps by the time since the last frame, and
 * the static state the menu keeps between draws. */
static void shot_motion(app *a)
{
	unsigned t0 = plat_now_ms(), t;

	g_clock_held = true;
	g_clock_at = t0;
	shot_t0 = t0;
	cf_clock(anim_ticks);
	/* The tint's last step is now, or the first frame after the press would
	 * step it by however long the shot took to get here, and the shot would
	 * be a picture of the setup's speed. */
	last_tint_ms = t0;
	if (shot_kb && shot_press != SHOT_STEP) {
		/* The same bridge as the real loop: --open from the shelf,
		 * --swap from its menu, --close back to that menu. */
		shot_keyboard = (keyboard_transition){ .a = a };
		if (shot_press != SHOT_OPEN) {
			shot_lv = panel_open();
			mm_press(shot_lv);
			draw_shelf(a);
			zyperos_menu_draw(a, shot_menu_sel);
		}
		keyboard_panel(&shot_keyboard, KB_PANEL_OPEN, false);
		if (shot_press == SHOT_CLOSE) {
			keyboard_panel(&shot_keyboard, KB_PANEL_FRAME, true);
			keyboard_panel(&shot_keyboard, KB_PANEL_LEAVE, false);
		}
		shot_keyboard_panel = true;
	} else if (shot_storage && !strcmp(shot_storage, "delete-open")) {
		/* As on the handheld: MENU's panel at rest and drawn, so it is the
		 * panel on screen; A on Storage, whose page is a base
		 * (store_base_open); a second on the page, for MENU's panel to have
		 * left over it; then A on a game, and Delete Game's question opens.
		 * Its first frame is the page with nothing on it (2026-11-08: it was
		 * the panel before it, leaving, and then the question coming) */
		int row = shot_menu_row(a, "Storage");

		shot_lv = panel_open();
		mm_press(shot_lv);
		draw_shelf(a);
		zyperos_menu_draw(a, row >= 0 ? row : 0);
		store_base_open();
		t0 += 1000;
		g_clock_at = t0;
		shot_t0 = t0;
		shot_lv = panel_open();
		shot_pressed = true;
	} else if (shot_menu && shot_press != SHOT_STEP) {
		/* Opened on the shelf, as MENU opens it, by panel_open as menu_run
		 * does: --open's press. The others' menu is at rest when theirs
		 * comes - B, whose panel the shelf then draws leaving, or A on its
		 * Controls row, whose page opens over it. */
		shot_lv = panel_open();
		if (shot_press != SHOT_OPEN) {
			int c = shot_press == SHOT_SWAP ? shot_menu_row(a, "Controls") : -1;

			if (c >= 0) shot_menu_sel = c;
			mm_press(shot_lv);
			shot_compose(a);
			if (shot_press == SHOT_CLOSE) panel_close(a, shot_lv, false);
			else shot_lv = panel_open();
		}
		shot_pressed = true;
	} else if (shot_menu) {
		int to = shot_menu_sel;

		shot_menu_sel = shot_menu_from(a, to);
		shot_compose(a);                    /* at rest on the row above */
		shot_menu_sel = to;                 /* the press, at the next draw */
	} else if (shot_gamemenu && (shot_gm_from >= 0 || shot_gm_press)) {
		shot_gm_motion(a, t0);
	} else if (a->screen == SCREEN_GAMES) {
		sysview *v = &a->view[a->sys_cursor];

		shot_compose(a);
		if (v->list.count > 1) {
			v->cursor = (v->cursor + 1) % v->list.count;
			cf_set_cursor_dir(&v->cf, v->cursor, v->list.count, +1);
		}
	} else {
		shot_compose(a);
		if (a->sys.count > 1) {
			a->sys_cursor = (a->sys_cursor + 1) % a->sys.count;
			home_select(&a->home, a->sys_cursor, a->sys.count, +1, anim_now());
		}
	}
	for (t = 0; t < (unsigned)shot_at; t += 1000 / 60) {
		g_clock_at = t0 + t;
		tick_tint(a);
		shot_compose(a);
	}
	g_clock_at = t0 + (unsigned)shot_at;
	tick_tint(a);
}

/* --launch: the zoom's frame at --shot-at's time, as anim_launch draws it.
 * It asks for the card itself, after draining whatever has landed, as
 * anim_launch does, rather than counting on the prime_window the shot runs
 * for the games screen alone: a shot has no worker, so the ask reads the
 * card on the frame, from either screen. */
static void shot_launch_frame(app *a)
{
	sysview *v = &a->view[a->sys_cursor];
	SDL_Rect from;
	int tw = 0, th = 0;
	SDL_Texture *card;
	float k = shot_at > 0 ? (float)shot_at / DS_MS_LAUNCH : 0.0f;

	texload_drain(a);
	card = v->list.count ? game_get_tex(a, v->cursor, &tw, &th, NULL) : NULL;
	cf_focus_rect(&CF_LAYOUT_GAMES, ZYPEROS_SCREEN_W, ZYPEROS_SCREEN_H, &from);
	launch_frame(a, card, &from, k > 1.0f ? 1.0f : k);
}

/* --flip's turn: the whole shot drawn once in the theme it started in, then
 * Auto set a minute before the edge that ends that theme - 7:00 for Salt &
 * Pepper, 19:00 for Beard, today, where the same theme is still on - and its
 * clock read at the edge itself. theme_tick says the theme changed and
 * theme_changed is told, which is tick_theme's body with the clock's time
 * handed in: the path 7:00 and 19:00 take on a handheld, and not one written
 * for the shot.
 *
 * Then the frame right after the turn, as the device draws it: there every
 * card is asked of a worker and nothing is read on the frame, so a card the
 * turn took away is a hole in the row until a worker hands its replacement
 * over. A shot has no worker and reads every card on the frame, so this one
 * frame is drawn with the reads held (g_loads_held), and a slot in view with
 * no card in it fails the shot (BLANK): on the systems row, and on a shelf of
 * games, where the row is what was drawn - not under Now Playing, which
 * covers it, and not on Muse's shelf, whose cards are made on the frame. The
 * shot itself is the frame after, every card read in the theme that is on. */
static void shot_flip_theme(app *a)
{
	time_t now = time(NULL), edge;
	struct tm tm;
	sysview *v = &a->view[a->sys_cursor];
	int n, i, k;

	shot_compose(a);
	localtime_r(&now, &tm);
	tm.tm_hour = theme_index() == DS_DARK ? 7 : 19;
	tm.tm_min = tm.tm_sec = 0;
	tm.tm_isdst = -1;
	edge = mktime(&tm);
	theme_init_at(APPEAR_AUTO, edge - 60);
	if (theme_tick_at(edge)) theme_changed(a);
	else fprintf(stderr, "--flip: the theme did not turn at %02d:00\n", tm.tm_hour);

	g_loads_held = true;
	shot_compose(a);
	g_loads_held = false;
	/* The games in view: the cursor's and CF_HALF_WINDOW either side,
	 * wrapping as prime_around does. Every one was read before the turn, by
	 * prime_window at the start and the frame drawn then, so an empty one is
	 * one the turn emptied. */
	if (a->screen != SCREEN_GAMES || shot_np >= 0 || v->list.count <= 0 ||
	    is_muse(&a->sys.systems[a->sys_cursor]))
		return;
	for (k = -CF_HALF_WINDOW; k <= CF_HALF_WINDOW; k++) {
		n = v->list.count;
		i = n >= 2 ? ((v->cursor + k) % n + n) % n : 0;
		if (!v->tex[i]) {
			fprintf(stderr, "  BLANK: %s had no card on the frame after the "
			        "turn, so the shelf had a hole until its card came back\n",
			        v->list.items[i].title);
			shot_failed = 1;
		}
	}
}

/* Every card in a slot has a foot: the fraction of its height its pixels
 * reach (content_bottom), which is where cf_draw ends the lift under it and
 * starts its reflection. A card in a slot with a foot of 0 is drawn with a
 * shadow, a glow and a reflection 0 units high at its top - a thin line above
 * a card on Beard and nothing at all on Salt & Pepper - which is what every
 * system with no card file was drawn with until its generated card was given
 * one. The name of the first card without one, or NULL. */
static const char *shot_cards_have_feet(const app *a)
{
	for (int i = 0; i < a->sys.count; i++) {
		const sysview *v = &a->view[i];

		for (int k = 0; v->tex && v->cb && k < v->list.count; k++)
			if (v->tex[k] && !(v->cb[k] > 0.0f)) return v->list.items[k].title;
	}
	return NULL;
}

/* --portload: the game by the title the shelf shows or its file's name, on
 * a port's or a "run:" system's shelf before any other, drawn as
 * launch_port draws it and written to the shot. Nothing is started and
 * nothing is written for splash. The shot fails (exit 3) when the screen
 * says it fell short - a line or the band under its contrast, or the words
 * or the cover in Zyper's frame - and when there is no such game. */
static void shot_portload_take(app *a)
{
	SDL_Surface *out;
	int pass, si, gi = -1;

	for (pass = 0; pass < 2 && gi < 0; pass++)
		for (si = 0; si < a->sys.count && gi < 0; si++) {
			const sysview *v = &a->view[si];
			int k;

			if (pass == 0 && !is_port(&a->sys.systems[si])) continue;
			for (k = 0; k < v->list.count; k++)
				if (!strcmp(v->list.items[k].title, shot_portload) ||
				    !strcmp(v->list.items[k].name, shot_portload)) {
					a->sys_cursor = si;
					gi = k;
					break;
				}
		}
	if (gi < 0) {
		fprintf(stderr, "--portload: no game called %s\n", shot_portload);
		shot_failed = 1;
		return;
	}
	a->screen = SCREEN_GAMES;
	a->view[a->sys_cursor].cursor = gi;
	/* The screen's own checks: everything at its contrast, and nothing in
	 * Zyper's frame */
	if (!port_loading_draw(a, &a->sys.systems[shelf_owner(a, a->sys_cursor, gi)], gi))
		shot_failed = 1;
	out = SDL_CreateRGBSurfaceWithFormat(0, ZYPEROS_SCREEN_W, ZYPEROS_SCREEN_H, 32,
	                                     SDL_PIXELFORMAT_RGBA32);
	if (!out) return;
	SDL_RenderReadPixels(a->r, NULL, SDL_PIXELFORMAT_RGBA32, out->pixels, out->pitch);
	plat_present();
	IMG_SavePNG(out, shot_path);
	SDL_FreeSurface(out);
	fprintf(stderr, "wrote %s  [portload] %s\n", shot_path,
	        a->view[a->sys_cursor].list.items[gi].title);
}

/* --tour's frame, at a clock fixed for the shot, so the same arguments draw
 * the same picture: the loops, Zyper's breath and the confetti are all read
 * from it. */
static void shot_tour_draw(app *a)
{
	const uint32_t now = 1000000u;
	const tour_hooks hk = { tour_zyper_draw, tour_poweroff_draw, a, P_ROOT };
	tour t;
	int at[TOUR_CHOICES];

	if (shot_tour < 0) {
		const float ms = (float)shot_tour_ms;

		draw_shelf(a);
		tour_message(a->r, T(S_TOURRUN_SKIP_MESSAGE),
		             tour_ease(TC_OUT, (uint32_t)shot_tour_ms, 0, DS_MS_MOVE)
		             * (1.0f - fminf(1.0f, fmaxf(0.0f, (ms - DS_MS_HOLD_SHORT) / DS_MS_FADE_OUT))));
	} else {
		tour_pose(&t, (tour_step)shot_tour, now, shot_tour_ms, shot_tour_done,
		          shot_fn_volume || plat_brightness_on_volume());
		tour_lang_from(&t, (int)i18n_lang());
		tour_set_music(&t, shot_tour_music);
		tour_settings(a, at);
		tour_choices_from(&t, at);
		if (shot_tour_pick >= 0)
			tour_pose_pick(&t, shot_tour_pick, now, shot_tour_pick_ms);
		/* The language step is drawn in the language under its cursor */
		if (t.step == TOUR_LANG) {
			const lang f = (lang)tour_lang_focus(&t, now);

			if (f != i18n_lang()) language_apply(a, f, false);
		}
		if (shot_tour_done >= 0 && tour_choice_of(t.step) >= 0)
			t.kept[tour_choice_of(t.step)] = t.pick[tour_choice_of(t.step)];
		if (shot_tour_wrong >= 0)
			tour_pose_wrong(&t, (tour_key)shot_tour_wrong, now, shot_tour_wrong_ms);
		tour_draw(a->r, &t, now, &hk);
	}
	if (shot_osd_kind) {
		plat_osd_show(shot_osd_kind, shot_osd_val,
		              shot_osd_kind == 1 ? PLAT_BRIGHT_MAX : PLAT_VOL_MAX);
		SDL_Delay(OSDBAR_IN_MS + 10);
		plat_draw_osd(a->r);
	}
	tour_draw_free();
}

/* --charging's frame, on a clock of its own, the estimate settled */
static void shot_charging_draw(app *a)
{
	const unsigned T0 = 100000;
	unsigned now = T0 + (unsigned)shot_charging;

	memset(&g_chg, 0, sizeof g_chg);
	g_chg.shown_at = T0;
	ch_screen_start(&g_chg.s, T0);
	/* kept: held at an 85% limit; limit: charging toward it; keptfull: held,
	 * but charged full before the limit was set */
	ch_fake_parse(shot_charging_kind == 5 ? "85 -5 4100 wall 1 1"
	              : shot_charging_kind == 7 ? "100 -5 4290 wall 1 1"
	              : shot_charging_kind == 8 ? "92 1480 4150 wall 1" : "62 1480 4120 wall 1", &g_chg.r);
	if (shot_charging_kind >= 5 && shot_charging_kind != 8) g_chg.r.limit = 85;
	/* The limit saved in MENU, for Y's hint: 85 but for soon's, which has
	 * none; tofull, Y pressed: the limit off this time, to 100% */
	g_charge_limit = shot_charging_kind == 8 ? 0 : 85;
	if (shot_charging_kind == 9) { g_full_shot = true; g_chg.r.limit = 0; }
	/* pending: the PHY has not named the charger yet */
	if (shot_charging_kind == 4) g_chg.r.kind = CH_PENDING;
	g_chg.valid = true;
	g_chg.eta = (ch_eta){ true, T0 - 60000, T0, 1480.0 };
	ch_words(&g_chg.r, ch_eta_minutes(&g_chg.eta, T0, &g_chg.r), false, g_chg.speed,
	         sizeof g_chg.speed, g_chg.when, sizeof g_chg.when);
	if (shot_charging_kind == 1) {
		ch_screen_tick(&g_chg.s, now - DS_MS_HOLD_UNLOCK / 2, true, true, true);
		ch_screen_tick(&g_chg.s, now, false, true, true);
	} else if (shot_charging_kind == 2)
		ch_screen_tick(&g_chg.s, now, false, false, false);
	if (shot_charging_kind == 3) {
		/* the cable out, its hold, then the send-off */
		unsigned t = (unsigned)shot_charging > POWEROFF_DIM ? POWEROFF_DIM : (unsigned)shot_charging;

		ch_screen_tick(&g_chg.s, T0 + 500, false, false, false);
		ch_screen_tick(&g_chg.s, T0 + 500 + DS_MS_HOLD_SHORT, false, false, false);
		chg_draw(a, T0 + 1000, ds_out((float)t / POWEROFF_DIM), 1.0f - (float)t / POWEROFF_DIM);
		return;
	}
	chg_draw(a, now, 0.0f, 1.0f);
}

/* --lowbatt's frame, on a clock of its own so it is the same each time */
static void shot_lowbatt_draw(app *a)
{
	const unsigned T0 = 100000, PLUG = 2400;
	unsigned now = T0 + (unsigned)shot_lowbatt;
	float bar_k = 1.0f;

	g_lb.pct = 2;
	lowbatt_start(&g_lb.l, T0);
	zr_init(&g_lb.rig, T0, 1);
	zr_fails(&g_lb.rig, T0);
	if (shot_lowbatt_kind == 2) {
		/* the send-off, from the end of the hold */
		now = T0 + DS_MS_HOLD_LOW_BATTERY;
		zr_fails(&g_lb.rig, now - DS_MS_HOLD_ZYPER_FAIL / 2);
		zr_tick(&g_lb.rig, now);
		g_lb.pose = zr_pose_at(&g_lb.rig, now);
		lowbatt_off_frame(a, &g_lb.pose, (unsigned)shot_lowbatt, now);
		return;
	}
	if (shot_lowbatt_kind == 1 && shot_lowbatt >= (int)PLUG) {
		lowbatt_tick(&g_lb.l, T0 + PLUG, false, true, NULL);
		zr_fails(&g_lb.rig, T0 + PLUG - DS_MS_HOLD_ZYPER_FAIL / 2);
		zr_press(&g_lb.rig, T0 + PLUG);
		zr_works(&g_lb.rig, T0 + PLUG);
		bar_k = now - (T0 + PLUG) >= DS_MS_FADE_OUT ? 0.0f
		      : 1.0f - (float)(now - (T0 + PLUG)) / DS_MS_FADE_OUT;
	} else if (shot_lowbatt > (int)DS_MS_HOLD_ZYPER_FAIL / 2) {
		zr_fails(&g_lb.rig, now - DS_MS_HOLD_ZYPER_FAIL / 2);
	}
	zr_tick(&g_lb.rig, now);
	g_lb.pose = zr_pose_at(&g_lb.rig, now);
	lowbatt_draw(a, now, &g_lb.pose, 0.0f, 1.0f, bar_k);
}

static void take_shot(app *a)
{
	SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, ZYPEROS_SCREEN_W,
	                                                  ZYPEROS_SCREEN_H, 32,
	                                                  SDL_PIXELFORMAT_RGBA32);

	if (shot_launch) {
		shot_launch_frame(a);
	} else if (shot_lowbatt >= 0) {
		shot_lowbatt_draw(a);
	} else if (shot_charging >= 0) {
		shot_charging_draw(a);
	} else if (shot_tour > -2) {
		shot_tour_draw(a);
	} else if (shot_focus_from >= 0) {
		int at = g_home_phase;

		g_home_phase = shot_focus_from;
		shot_compose(a);
		g_home_phase = at;
		/* focus_frame's own rule: a whole frame when anything the focus
		 * frame does not draw again is on screen */
		if (focus_ok(a) && focus_draw(a)) {
			/* What plat_present adds to a frame that skipped
			 * plat_draw_osd (plat_overlay_present) */
			if (g_replug.on) replug_draw_at(a->r);
		} else {
			fprintf(stderr, "focus: not for this sprite, a whole frame\n");
			shot_compose(a);
		}
	} else {
		if (shot_flip) shot_flip_theme(a);
		shot_compose(a);
	}
	if (out) {
		/* Read BEFORE presenting: the backbuffer is invalid afterwards. */
		SDL_RenderReadPixels(a->r, NULL, SDL_PIXELFORMAT_RGBA32,
		                     out->pixels, out->pitch);
		plat_present();
		IMG_SavePNG(out, shot_path);
		SDL_FreeSurface(out);
		/* Say what was drawn, not just that something was: a tool whose whole
		 * job is rendering one state is far more useful when the state it
		 * chose is in the output beside the filename. */
		{
			sysview *v = &a->view[a->sys_cursor];
			const char *what = a->screen == SCREEN_GAMES && v->list.count > 0
			                 ? v->list.items[v->cursor].title
			                 : a->sys.systems[a->sys_cursor].name;
			/* Checked rather than assumed. This line once named a focused
			 * game on a shot with no cards in it. A card drawn on the frame
			 * always ends up with a texture - box art, a preview, or the
			 * generated card - so one without means the picture is not what
			 * the name beside it claims, and that is worth saying loudly. */
			const char *note =
				a->screen == SCREEN_GAMES && v->list.count > 0 &&
				v->tex && !v->tex[v->cursor]
				? "  (NO ART: the focused card had no texture on this frame)" : "";
			const char *footless = shot_cards_have_feet(a);

			if (shot_tour > -2)
				fprintf(stderr, "wrote %s  [tour] %s\n", shot_path,
				        shot_tour < 0 ? "skip" : TOUR_STEP_NAMES[shot_tour]);
			else
				fprintf(stderr, "wrote %s  [%s] %s%s\n", shot_path,
				        a->screen == SCREEN_GAMES ? "games" : "systems", what, note);
			if (footless) {
				fprintf(stderr, "  NO FOOT: %s's card has a content bottom of 0, so "
				        "nothing is lifted or reflected under it\n", footless);
				shot_failed = 1;
			}
			{
				/* The screensaver draws no corner: it is the whole screen */
				const char *crowded = g_shot_saver >= 0 ? NULL : shot_corner_clear(a);

				if (crowded) {
					fprintf(stderr, "  CORNER: %s\n", crowded);
					shot_failed = 1;
				}
			}
		}
	}
}

/* The start's scan off the main thread (main says why): from the saved
 * library when there is one (src/savedlib.h), except for a shot, which shows
 * the card it is given */
static int scan_thread(void *ctx)
{
	scan_all_start(ctx, !shot_path);
	t_mark("library scan");
	return 0;
}

/* --port-menu TAG GAME (src/portmenu.h): the in-game menu over a program of
 * its own (OpenBOR, the standalone Flycast, a port from the Ports
 * shelf and PortMaster itself, whose GAME is the port's script), and only
 * that. What it needs of the launcher's start, in the same order: the
 * settings, the port's system from systems.cfg (its name and accent, and its
 * buttons for the Controls page; a port's, what its files say), the
 * language and theme, the interface
 * sounds, the display (taken as from the splash: portkeys has let go of it),
 * the buttons and the faces. No scan of the card, no Diatom, no MP4, no menu
 * music, no File Transfer: the port has the memory, and the menu wants none
 * of them. Auto Off is off: a port has no state to come back to after it,
 * and the battery run flat is still POWER (idle_due). */
static int port_menu_main(int argc, char **argv)
{
	static app a;
	const char *tag = argc > 2 ? argv[2] : "", *game = argc > 3 ? argv[3] : "";
	const char *dot = strrchr(game, '.');
	const system_cfg *s = NULL;
	char path[CFG_STR * 2];
	int how, i;

	signal(SIGTERM, on_sigterm);
	signal(SIGINT, on_sigterm);
	t_boot0 = plat_now_ms();
	paths_init();
	{
		char dev[CFG_STR * 2], lib[CFG_STR * 2], env[CFG_STR * 2], tz[64];

		db_paths_ready(dev, sizeof dev, lib, sizeof lib, env, sizeof env);
		if (!db_init(dev, lib, env))
			fprintf(stderr, "port menu: no settings database; the shipped defaults\n");
		db_get_str(db_lib(), "timezone", tz, sizeof tz, "America/New_York");
		plat_clock_zone(tz);
	}
	snprintf(path, sizeof path, "%s/systems.cfg", P_ROOT);
	if (!cfg_load_systems(path, &a.sys)) {
		fprintf(stderr, "port menu: no usable %s\n", path);
		return PORT_MENU_CONTINUE;
	}
	for (i = 0; i < a.sys.count; i++)
		if (!strcmp(a.sys.systems[i].tag, tag)) {
			a.sys_cursor = i;
			s = &a.sys.systems[i];
			break;
		}
	if (!s) {
		fprintf(stderr, "port menu: no system tagged %s\n", tag);
		return PORT_MENU_CONTINUE;
	}
	g_port_menu = true;
	/* Its buttons when ZyperOS knows them (OpenBOR's); over the standalone
	 * Flycast, In a game alone (port_menu_pads) */
	g_port_ctl = (ctl_game){ port_menu_pads(s->tag) ? s->tag : "", s->name,
	                         home_label(s->tag, s->name), s->map, plat_turbo_map(s->tag),
	                         dot ? dot + 1 : "", ctl_head_fits, &a, true, NULL, NULL, false,
	                         CTL_STICK_TABLE };
	/* A port's pages: what its README, its keymap and its port.json say,
	 * or PortMaster's own buttons over PortMaster (src/portdoc.h) */
	if (!strcmp(s->core, "port")) {
		static port_doc doc;

		port_doc_read(game, &doc);
		g_port_ctl.doc = &doc;
		fprintf(stderr, "port menu: %s%s: %d of its README's controls, %d keys, %s, %s\n",
		        doc.title, doc.portmaster ? " (PortMaster)" : "", doc.nsaid, doc.nkeys,
		        doc.gptokeyb ? "gptokeyb" : "no gptokeyb",
		        doc.inst[0] || doc.desc[0] ? "an About" : "no About");
	}
	{
		char ap[16], lc[16], v[16], dir[CFG_STR * 2];

		db_get_str(db_dev(), "language", lc, sizeof lc, "en");
		i18n_set(lang_parse(lc));
		db_get_str(db_dev(), "appearance", ap, sizeof ap, appearance_id(APPEAR_LIGHT));
		look_load();
		theme_init(appearance_parse(ap));
		IMG_Init(IMG_INIT_PNG);
		db_get_str(db_dev(), "uisounds", v, sizeof v, uisound_setting_id(DS_VOICE_DEFAULT));
		uisound_set(uisound_setting_parse(v));
		snprintf(dir, sizeof dir, "%s/res/sounds", P_ROOT);
		uisound_init(dir);
	}
	if (!plat_video_init()) {
		fprintf(stderr, "port menu: video init failed\n");
		return PORT_MENU_CONTINUE;
	}
	a.r = plat_renderer();
	tour_hint_draw_set(keyhints_width, tour_hint);
	plat_input_init();
	plat_settings_init();
	if (!ui_init(a.r, P_FONT)) fprintf(stderr, "port menu: font init failed\n");
	ui_fonts_for(i18n_lang());
	page_fonts_init(P_FONT);
	clock24_load();
	batt_icon_load();
	a.tint = s->accent;
	a.auto_off = 0;
	a.running = true;
	fprintf(stderr, "port menu: %s, ready in %u ms\n", s->name, plat_now_ms() - t_boot0);

	how = port_menu(&a);
	fprintf(stderr, "port menu: %s\n", how == PORT_MENU_QUIT ? "Quit" :
	        how == PORT_MENU_POWER ? "POWER" : "Continue");
	/* The window closed, which puts back on the panel the frame SDL found
	 * there - the port's - before the port has the display again */
	uisound_release();
	plat_input_quit();
	ui_quit();
	plat_video_quit();
	db_shutdown();
	return how;
}

int main(int argc, char *argv[])
{
	app a = { 0 };
	char path[CFG_STR * 2];
	char startup[CFG_STR];

	if (argc > 1 && !strcmp(argv[1], PORT_MENU_ARG)) return port_menu_main(argc, argv);

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--shot") && i + 1 < argc) {
			shot_path = argv[++i];
			/* A shot puts no window on the desktop of whoever is using the
			 * computer, whether or not the caller asked (src/device/brick.c,
			 * plat_video_init): a hidden window the software renderer
			 * draws, on SDL's dummy video driver, which needs no OpenGL.
			 * Before any SDL call, since the driver is chosen at the first.
			 * A variable the caller set stays as it set it: the check
			 * scripts set both, as they did before. */
			setenv("ZYPEROS_HEADLESS", "1", 0);
			setenv("SDL_VIDEODRIVER", "dummy", 0);
		} else if (!strcmp(argv[i], "--screen") && i + 1 < argc)
			shot_screen = strcmp(argv[++i], "games") == 0 ? SCREEN_GAMES
			                                              : SCREEN_SYSTEMS;
		/* --menu [row] draws the ZyperOS menu over whichever screen --screen
		 * asked for, so the panel can be looked at without a device. The row
		 * is its number or its label ("Clock"), which stays the same row when
		 * one is added above it. */
		else if (!strcmp(argv[i], "--menu")) {
			shot_menu = 1;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_menu_sel = atoi(argv[++i]);
			else if (i + 1 < argc && argv[i + 1][0] != '-')
				shot_menu_label = argv[++i];
		}
		else if (!strcmp(argv[i], "--menu-steps") && i + 1 < argc)
			shot_menu_steps = atoi(argv[++i]);
		/* --menu-tab N: MENU on tab N (pm_tab, 0 Games to 5 About), and
		 * --menu-tab-from M MS on its way there from tab M, MS into the
		 * move (DESIGN.md 11.3) */
		else if (!strcmp(argv[i], "--look") && i + 1 < argc)
			shot_look = argv[++i];
		else if (!strcmp(argv[i], "--look-page") && i + 1 < argc) {
			shot_look_page = argv[++i];
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_look_row = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--menu-tab") && i + 1 < argc)
			g_tabs.at = g_tabs.from = atoi(argv[++i]) % PT_TABS;
		else if (!strcmp(argv[i], "--menu-tab-from") && i + 2 < argc) {
			shot_tab_from = atoi(argv[++i]) % PT_TABS;
			shot_tab_ms = atoi(argv[++i]);
		}
		/* --slots <sel> [aspect] draws one frame of the save/load carousel
		 * over synthetic frames, which is the only way to look at it without
		 * a game running on a device. */
		/* --jump N applies N letter-jumps before the shot (negative for up),
		 * so the d-pad's behavior on a real library can be checked without a
		 * device or a hand on it. */
		else if (!strcmp(argv[i], "--jump") && i + 1 < argc) {
			shot_jump = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--folder") && i + 1 < argc) shot_folder = argv[++i];
		else if (!strcmp(argv[i], "--shelf-press") && i + 1 < argc) shot_shelf_press = argv[++i];
		else if (!strcmp(argv[i], "--game") && i + 1 < argc) shot_game = argv[++i];
		else if (!strcmp(argv[i], "--view") && i + 1 < argc) {
			shot_view = argv[++i];
			if (strcmp(shot_view, "list") && strcmp(shot_view, "covers")) {
				fprintf(stderr, "--view needs list or covers\n");
				return 2;
			}
		}
		/* --keyboard [layer] [text] draws one frame of the text-entry panel,
		 * for the same reason --menu and --slots exist: it is dense, and
		 * laying it out against a screenshot beats a round trip to a device. */
		else if (!strcmp(argv[i], "--cheevos-screen")) {
			shot_cheevos = 1;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_cheevos_sel = atoi(argv[++i]);
			/* A game name, so the heading can be looked at with one of the
			 * long ones rather than only the short one that fit. */
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_cheevos_game = argv[++i];
		}
		else if (!strcmp(argv[i], "--cheevos-badge") && i + 2 < argc) {
			shot_badge_set = argv[++i];
			shot_badge_name = argv[++i];
		}
		else if (!strcmp(argv[i], "--synopsis")) shot_syn = 1;
		/* --nowplaying ALBUM [TRACK [SECONDS]] draws Now Playing for an album
		 * of the card's Music folder, by its number in the scan, without a
		 * daemon: the cover has to be on the card already, or the album is
		 * drawn as one with none. --paused draws it paused. */
		else if (!strcmp(argv[i], "--nowplaying") && i + 1 < argc) {
			shot_np = atoi(argv[++i]);
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_np_track = atoi(argv[++i]);
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_np_at = (float)atof(argv[++i]);
		}
		else if (!strcmp(argv[i], "--paused")) shot_np_paused = 1;
		else if (!strcmp(argv[i], "--locked")) shot_locked = 1;
		else if (!strcmp(argv[i], "--video")) {
			shot_video = NPV_FULL;
			if (i + 1 < argc && !strcmp(argv[i + 1], "controls")) { shot_video = NPV_CONTROLS; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "full")) i++;
		}
		else if (!strcmp(argv[i], "--mode") && i + 1 < argc) shot_mode = argv[++i];
		else if (!strcmp(argv[i], "--sysmove") && i + 1 < argc)
			shot_sysmove = (float)atof(argv[++i]);
		else if (!strcmp(argv[i], "--gamemove") && i + 1 < argc)
			shot_gamemove = (float)atof(argv[++i]);
		else if (!strcmp(argv[i], "--shot-at") && i + 1 < argc) {
			shot_at = atoi(argv[++i]);
			if (shot_at < 0) shot_at = 0;
		}
		else if (!strcmp(argv[i], "--launch")) shot_launch = 1;
		else if (!strcmp(argv[i], "--info")) shot_info = 1;
		/* The details page with the cursor on Delete Game, and its question
		 * over the shelf (DESIGN.md 11.31) */
		else if (!strcmp(argv[i], "--info-delete")) shot_info = 2;
		else if (!strcmp(argv[i], "--info-rename")) shot_info = 3;
		else if (!strcmp(argv[i], "--info-art")) shot_info_art = true;
		/* The Buttons page a game with its own buttons opens from its details */
		else if (!strcmp(argv[i], "--info-buttons")) shot_info = 4;
		else if (!strcmp(argv[i], "--delete-ask")) shot_delete_ask = 1;
		/* The game under the cursor deleted as Delete Game's yes deletes it,
		 * then the shot: the shelf after, and what went on stderr */
		else if (!strcmp(argv[i], "--delete-now")) shot_delete_now = 1;
		/* --osd volume|brightness LEVEL: the settings bar at that level, on
		 * top of the rest of the shot. */
		else if (!strcmp(argv[i], "--osd") && i + 2 < argc) {
			shot_osd_kind = !strcmp(argv[i + 1], "brightness") ? 1 : 2;
			shot_osd_val = atoi(argv[i + 2]);
			i += 2;
		}
		/* --theme light|dark: the shot in that theme, whatever the saved
		 * Appearance is, so a screen can be looked at in both without a
		 * device and without changing the setting. Auto is not offered: a
		 * shot of a screen is a shot of one theme, and Auto's would be the
		 * theme of the minute it was taken in. An unknown word stops the
		 * run, since a picture in the other theme than the one asked for is
		 * what a typo would otherwise give and nobody would see why. */
		else if (!strcmp(argv[i], "--song") && i + 1 < argc) shot_song = argv[++i];
		else if (!strcmp(argv[i], "--song-paused")) shot_song_paused = true;
		else if (!strcmp(argv[i], "--home")) {
			if (i + 1 >= argc) {
				fprintf(stderr, "--home needs spotlight or gallery\n"); return 2;
			}
			const char *value = argv[++i];
			if (strcmp(value, "spotlight") && strcmp(value, "gallery")) {
				fprintf(stderr, "--home needs spotlight or gallery\n"); return 2;
			}
			shot_home = home_parse(value);
		}
		/* --ways usb|wifi|both|none[,brick]: which of the empty home's ways
		 * can start, and a Brick-like device (no shared port, Wi-Fi built in)
		 * with ",brick"; the shot draws them as the device would */
		else if (!strcmp(argv[i], "--cheer") && i + 1 < argc) shot_cheer = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--songpop") && i + 1 < argc) {
			shot_songpop = argv[++i];
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_songpop_ms = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--ways") && i + 1 < argc) {
			const char *w = argv[++i];

			g_eh_shot = !strncmp(w, "usb", 3) ? 1 << EH_USB : !strncmp(w, "wifi", 4) ? 1 << EH_WIFI
			          : !strncmp(w, "both", 4) ? (1 << EH_USB) | (1 << EH_WIFI) : 0;
			if (strstr(w, ",brick")) g_eh_shot |= 4;
		}
		else if (!strcmp(argv[i], "--lang") && i + 1 < argc) shot_lang = argv[++i];
		else if (!strcmp(argv[i], "--theme") && i + 1 < argc) {
			i++;
			if (!strcmp(argv[i], "light")) shot_theme = APPEAR_LIGHT;
			else if (!strcmp(argv[i], "dark")) shot_theme = APPEAR_DARK;
			else {
				fprintf(stderr, "--theme: %s is not light or dark\n", argv[i]);
				return 2;
			}
		}
		/* --status "WORDS": what the shelf's status corner shows, for a
		 * host with no radio and no battery. Words, in any order:
		 * H:MM (the time of day; none is a clock never set), 24h or 12h,
		 * wifi=off|idle|1|2|3, bt, batt=PCT, chg, and blinkoff for a
		 * critical battery caught in its second off, eta=MIN for what
		 * MENU > Battery Icon > Time Left writes, icon=bars|percentage|time
		 * for the setting itself. */
		else if (!strcmp(argv[i], "--status") && i + 1 < argc) {
			if (!shot_status_parse(argv[++i])) {
				fprintf(stderr, "--status: cannot read \"%s\"\n", argv[i]);
				return 2;
			}
		}
		/* --gamemenu [row] [dual]: the menu over a paused game, that row
		 * selected, with a Nintendo DS game's three screen rows when dual */
		else if (!strcmp(argv[i], "--ingame")) shot_ingame = 1;
		else if (!strcmp(argv[i], "--handheld-box")) {
			shot_handheld_box = "all";
			if (i + 1 < argc && (!strcmp(argv[i + 1], "tab") || !strcmp(argv[i + 1], "tour")))
				shot_handheld_box = argv[++i];
		}
		else if (!strcmp(argv[i], "--controls-fit")) {
			shot_controls_fit = "all";
			if (i + 1 < argc && (!strcmp(argv[i + 1], "game") || !strcmp(argv[i + 1], "home")))
				shot_controls_fit = argv[++i];
		}
		else if (!strcmp(argv[i], "--gamecontrols")) {
			shot_gamecontrols = 0;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_gamecontrols = atoi(argv[++i]);
			if (i + 1 < argc && argv[i + 1][0] >= 'A' && argv[i + 1][0] <= 'Z')
				shot_gamecontrols_tag = argv[++i];
		}
		else if (!strcmp(argv[i], "--gamemenu")) {
			shot_gamemenu = 1;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_gamemenu_sel = atoi(argv[++i]);
			if (i + 1 < argc && !strcmp(argv[i + 1], "dual")) {
				g_game_dual = true;
				i++;
			} else if (i + 1 < argc && !strcmp(argv[i + 1], "port")) {
				g_port_menu = true;
				i++;
			}
		}
		/* --gm-tab N: the in-game menu's tab, 0 Game, 1 Display, 2 Controls */
		else if (!strcmp(argv[i], "--gm-tab") && i + 1 < argc) shot_gamemenu_tab = atoi(argv[++i]);
		/* The Controls tab's (shot_draw_ctltab) */
		else if (!strcmp(argv[i], "--gm-all")) shot_gm_all = true;
		else if (!strcmp(argv[i], "--gm-pick")) shot_gm_pick = true;
		else if (!strcmp(argv[i], "--gm-ask")) shot_gm_ask = true;
		else if (!strcmp(argv[i], "--gm-keys") && i + 1 < argc) shot_gm_keys = atoi(argv[++i]);
		/* --gm-discs N [LABEL]: a game on N discs, disc 2 in the tray, the
		 * core calling it LABEL (none: the row says Disc 2 of N) */
		else if (!strcmp(argv[i], "--gm-discs") && i + 1 < argc) {
			g_shot_discs = atoi(argv[++i]);
			g_gm_disc.index = g_shot_discs > 1 ? 1 : 0;
			if (i + 1 < argc && strncmp(argv[i + 1], "--", 2) != 0) g_shot_disc_label = argv[++i];
		}
		else if (!strcmp(argv[i], "--gm-tab-from") && i + 1 < argc) shot_gm_from = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--gm-press") && i + 1 < argc) shot_gm_press = argv[++i];
		else if (!strcmp(argv[i], "--backdrop") && i + 1 < argc) shot_backdrop = argv[++i];
		else if (!strcmp(argv[i], "--press") && i + 1 < argc) {
			i++;
			shot_key = !strcmp(argv[i], "left")  ? IN_LEFT
			           : !strcmp(argv[i], "right") ? IN_RIGHT
			           : !strcmp(argv[i], "l1")    ? IN_L1
			           : !strcmp(argv[i], "r1")    ? IN_R1
			           : !strcmp(argv[i], "a")     ? IN_ACCEPT : -1;
			if (shot_key < 0) {
				fprintf(stderr, "--press: %s is not left, right, l1, r1 or a\n", argv[i]);
				return 2;
			}
		}
		else if (!strcmp(argv[i], "--flip")) shot_flip = 1;
		else if (!strcmp(argv[i], "--tour") && i + 1 < argc) {
			int k;

			i++;
			for (k = 0; k < TOUR_STEPS && strcmp(TOUR_STEP_NAMES[k], argv[i]); k++) ;
			if (k == TOUR_STEPS && strcmp(argv[i], "skip")) {
				fprintf(stderr, "--tour: %s is no step\n", argv[i]);
				return 2;
			}
			shot_tour = k == TOUR_STEPS ? -1 : k;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_tour_ms = atoi(argv[++i]);
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_tour_done = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--tour-wrong") && i + 1 < argc) {
			int k;

			i++;
			/* By tour_key_name, any case: up ... volume, brightness, power */
			for (k = 0; k < TK_KEYS && strcasecmp(tour_key_name((tour_key)k), argv[i]); k++) ;
			if (k == TK_KEYS) {
				fprintf(stderr, "--tour-wrong: %s is no button\n", argv[i]);
				return 2;
			}
			shot_tour_wrong = k;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_tour_wrong_ms = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--fn-volume")) shot_fn_volume = 1;
		else if (!strcmp(argv[i], "--tour-pick") && i + 1 < argc) {
			shot_tour_pick = atoi(argv[++i]);
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_tour_pick_ms = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--tour-music")) shot_tour_music = true;
		else if (!strcmp(argv[i], "--portload") && i + 1 < argc)
			shot_portload = argv[++i];
		else if (!strcmp(argv[i], "--portmaster")) {
			/* The Install PortMaster page, as ZYPEROS_PM_STATUS's file
			 * says, with any of running, nonet, nopython, installed; and
			 * Reinstall PortMaster's with repair, or failed (the error
			 * ZYPEROS_PM_WHY says, a missing module when it says none) */
			g_pm_shot = 0;
			if (i + 1 < argc && argv[i + 1][0] != '-') {
				const char *w = argv[++i];
				if (strstr(w, "running"))   g_pm_shot |= PM_SHOT_RUNNING;
				if (strstr(w, "nonet"))     g_pm_shot |= PM_SHOT_NONET;
				if (strstr(w, "nopython"))  g_pm_shot |= PM_SHOT_NOPYTHON;
				if (strstr(w, "installed")) g_pm_shot |= PM_SHOT_INSTALLED;
				if (strstr(w, "repair"))    g_pm_shot |= PM_SHOT_REPAIR;
				if (strstr(w, "failed"))    g_pm_shot |= PM_SHOT_FAILED;
			}
		}
		else if (!strcmp(argv[i], "--storage-sort") && i + 1 < argc) {
			const char *w = argv[++i];

			g_store_sort_shot = !strcmp(w, "az") ? ST_SORT_AZ : !strcmp(w, "last") ? ST_SORT_LAST : ST_SORT_SIZE;
		}
		else if (!strcmp(argv[i], "--media")) {
			shot_media = "page";
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_media = argv[++i];
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') shot_media_tile = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--media-art") && i + 1 < argc) shot_media_art = argv[++i];
		else if (!strcmp(argv[i], "--storage")) {
			shot_storage = "main";
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_storage = argv[++i];
			if (i + 1 < argc && argv[i + 1][0] != '-') {
				i++;
				if (argv[i][0] >= '0' && argv[i][0] <= '9') shot_storage_sel = atoi(argv[i]);
				else shot_storage_row = argv[i];
			}
		}
		else if (!strcmp(argv[i], "--ranews") && i + 1 < argc) {
			shot_ranews = atoi(argv[++i]);
			if (shot_heatpop < 0) shot_heatpop = 3000;
		}
		/* --achtab STATE: the in-game menu's Achievements tab (--gm-tab 3) in
		 * one state, a made-up set for list and all: list, all (every one
		 * earned), not-system, not-file, signed-out, waiting, unchecked,
		 * none */
		else if (!strcmp(argv[i], "--achtab") && i + 1 < argc) {
			static const char *const names[] = {
				"list", "not-system", "not-file", "signed-out", "waiting",
				"unchecked", "none", "all"
			};
			const char *v = argv[++i];
			int k;

			for (k = 0; k < (int)(sizeof names / sizeof names[0]); k++)
				if (!strcmp(v, names[k])) shot_achtab = k;
			if (shot_achtab < 0) {
				fprintf(stderr, "--achtab: no state %s\n", v);
				return 2;
			}
		}
		else if (!strcmp(argv[i], "--cheevos-waiting") && i + 1 < argc)
			shot_cheevos_waiting = atoi(argv[++i]);
		/* The Achievements rows of a game never played with a network, in
		 * the in-game menu (--gamemenu) and the game's details (--info) */
		else if (!strcmp(argv[i], "--cheevos-unchecked")) g_shot_unchecked = true;
		/* ...and of one with no hash here: "file" or "system" */
		else if (!strcmp(argv[i], "--cheevos-no-hash") && i + 1 < argc) {
			i++;
			g_shot_no_hash = !strcmp(argv[i], "system") ? RA_HASH_NO_SYSTEM
			                                            : RA_HASH_NO_FILE;
		}
		else if (!strcmp(argv[i], "--upnews") && i + 1 < argc) {
			shot_upnews = argv[++i];
			if (shot_heatpop < 0) shot_heatpop = 3000;
		}
		/* What's New scrolled N lines (System Update's page) */
		else if (!strcmp(argv[i], "--update-scroll") && i + 1 < argc) {
			g_up_scroll = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--update")) {
			shot_update = 1;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_update = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--music-page")) {
			shot_music_page = 0;
			if (i + 1 < argc && !strcmp(argv[i + 1], "foot")) {
				shot_music_page = -1;
				i++;
			} else if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_music_page = atoi(argv[++i]);
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_music_steps = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--language")) {
			shot_language = -1;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_language = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--about-scroll") && i + 1 < argc) {
			shot_about_scroll = (float)atof(argv[++i]);
			if (shot_about_scroll < 0) shot_about_scroll = 0;
			if (shot_about_scroll > 1) shot_about_scroll = 1;
		}
		else if (!strcmp(argv[i], "--about")) {
			shot_about = 0;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] < '0' + ABOUT_PAGES &&
			    !argv[i + 1][1])
				shot_about = atoi(argv[++i]);
		}
		/* --snap K: the games shelf with its focused cover K of the way
		 * into a video snap (a test picture), 0 to 1 */
		else if (!strcmp(argv[i], "--snap") && i + 1 < argc) {
			g_shot_snap_k = (float)atof(argv[++i]);
			if (g_shot_snap_k < 0) g_shot_snap_k = 0;
			if (g_shot_snap_k > 1) g_shot_snap_k = 1;
		}
		/* --saver [cover]: the screensaver, over a test picture; --saver-alpha F
		 * how far the clip has faded up, 0 to 1 */
		else if (!strcmp(argv[i], "--saver")) {
			g_shot_saver = i + 1 < argc && !strcmp(argv[i + 1], "cover") ? (i++, 1) : 0;
		}
		else if (!strcmp(argv[i], "--saver-pic") && i + 1 < argc) {
			g_shot_saver_pic = argv[++i];
		}
		else if (!strcmp(argv[i], "--saver-alpha") && i + 1 < argc) {
			g_shot_saver_alpha = (float)atof(argv[++i]);
		}
		/* --snap-wait F: the games shelf with the bar under its focused
		 * cover F of the way through the wait for its snap, 0 to 1 */
		else if (!strcmp(argv[i], "--snap-wait") && i + 1 < argc) {
			g_shot_snap_wait = (float)atof(argv[++i]);
			if (g_shot_snap_wait < 0) g_shot_snap_wait = 0;
			if (g_shot_snap_wait > 1) g_shot_snap_wait = 1;
		}
		else if (!strcmp(argv[i], "--scraper")) {
			shot_scraper = 3;
			if (i + 1 < argc && argv[i + 1][0] >= '3' && argv[i + 1][0] <= '4')
				shot_scraper = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--scraper-page") && i + 1 < argc) {
			shot_scraper_page = atoi(argv[++i]);
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_scraper_page_t = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--sgdb")) {
			shot_sgdb = 0;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '2')
				shot_sgdb = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--controls")) {
			shot_controls = 0;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_controls = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--phase") && i + 1 < argc)
			shot_phase = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--focus-from") && i + 1 < argc)
			shot_focus_from = atoi(argv[++i]);
		/* --zyper [works|fails]: Zyper in the top menu's --menu shot,
		 * --shot-at's time after MENU opened it (--open), and a success or a
		 * failure at 500 ms. A still of him needs a moment, as a marquee's
		 * does, and the moment is the one clock every shot keeps: a time
		 * here, as --zyper once took, is not his. */
		else if (!strcmp(argv[i], "--zyper")) {
			shot_press = SHOT_OPEN;
			if (i + 1 < argc && !strcmp(argv[i + 1], "works")) { shot_zyper_did = 1; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "fails")) { shot_zyper_did = -1; i++; }
			else if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
				fprintf(stderr, "--zyper takes its time from --shot-at: "
				        "--zyper --shot-at %s\n", argv[i + 1]);
				return 2;
			}
		}
		/* --open, --close, --swap: what --shot-at presses on --menu's top
		 * menu - MENU opening it on the shelf, B closing it onto the shelf,
		 * or A on its Controls row, the menu leaving and MENU > Controls
		 * (--controls' page, 0 by default) arriving - for frame strips of
		 * the menus' motion (DESIGN.md 8.2). Without --shot-at, the menu at
		 * rest. */
		else if (!strcmp(argv[i], "--open"))  shot_press = SHOT_OPEN;
		else if (!strcmp(argv[i], "--close")) shot_press = SHOT_CLOSE;
		else if (!strcmp(argv[i], "--swap")) {
			shot_press = SHOT_SWAP;
			if (shot_controls < 0) shot_controls = 0;
		}
		/* --poweroff: one frame of the power-off animation, --shot-at's time
		 * in, which the device otherwise shows only as it switches itself
		 * off */
		else if (!strcmp(argv[i], "--poweroff")) {
			shot_poweroff = 1;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
				fprintf(stderr, "--poweroff takes its time from --shot-at: "
				        "--poweroff --shot-at %s\n", argv[i + 1]);
				return 2;
			}
		}
		else if (!strcmp(argv[i], "--usb-ask")) {
			shot_usb = 0;
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_usb = atoi(argv[++i]);
			if (i + 1 < argc && !strcmp(argv[i + 1], "plain")) { shot_usb_dev = false; i++; }
			/* "drive": with USB Drive, as an image with usbdrive asks it */
			if (i + 1 < argc && !strcmp(argv[i + 1], "drive")) { shot_usb_drive = true; i++; }
		}
		/* --usb-drive N: USB Drive's picture N (connected, finishing, failed) */
		else if (!strcmp(argv[i], "--usb-drive") && i + 1 < argc) {
			shot_usbdrive = atoi(argv[++i]);
			/* The tiles' numbers: a copy to the card, one from it, both
			 * ways, or the widest */
			if (i + 1 < argc) {
				static const char *const V[] = { "idle", "in", "out", "both", "wide", "bright" };
				int v;

				for (v = 0; v < 6; v++)
					if (!strcmp(argv[i + 1], V[v])) { shot_usbdrive_vals = v; i++; break; }
			}
		}
		/* MENU's USB rows on the host, which has no usbrole: Ask, or
		 * Automatic with "auto"; "said" for Automatic's message */
		/* The replug message up, as on the device: drawn over the frame
		 * where the overlay goes (plat_draw_osd), for the home's focus
		 * check (tools/home-shot-check.py) */
		else if (!strcmp(argv[i], "--replug")) {
			g_replug.on = true;
			g_replug.plug = 1;
			g_replug.say = USBPOWER_SAY_COMPUTER;
			if (i + 1 < argc && !strcmp(argv[i + 1], "phone")) { g_replug.say = USBPOWER_SAY_PHONE; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "iphone")) { g_replug.say = USBPOWER_SAY_IPHONE; i++; }
		}
		else if (!strcmp(argv[i], "--usb-menu")) {
			shot_usb_menu = true;
			if (i + 1 < argc && !strcmp(argv[i + 1], "auto")) { g_usb_auto = true; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "said")) { g_usb_auto = true; shot_usb_said = true; i++; }
		}
		/* Zyper's offer of Automatic after a second USB Accessory */
		else if (!strcmp(argv[i], "--usb-offer")) shot_usb_offer = true;
		else if (!strcmp(argv[i], "--usb-power")) {
			shot_power = UP_COMPUTER;
			if (i + 1 < argc && !strcmp(argv[i + 1], "said")) { shot_power = UP_TILES; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "phone")) { shot_power = UP_TILES + 1; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "iphone")) { shot_power = UP_TILES + 2; i++; }
			else if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '2')
				shot_power = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--datetime")) {
			shot_datetime = 0;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_datetime = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--dev-page")) {
			shot_dev = 0;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_dev = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--xfer-ask")) {
			shot_xfer_ask = 0;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_xfer_ask = atoi(argv[++i]);
			if (i + 1 < argc && !strcmp(argv[i + 1], "game")) { shot_xfer_game = true; i++; }
		}
		else if (!strcmp(argv[i], "--player2") && i + 1 < argc) {
			static char qr[160];
			const char *kind = argv[++i];
			p2_view *v = &g_shot_p2_view;

			g_shot_p2 = true;
			memset(v, 0, sizeof *v);
			v->st = P2_WAITING;
			v->link = P2_HOTSPOT;
			v->players = 2;
			v->ssid = "ZyperOS-7A2B";
			v->pass = "q7mx-3kpa";
			captive_wifi_qr(qr, sizeof qr, v->ssid, v->pass);
			v->qr = qr;
			v->url = P2_HOTSPOT_ADDR "/play";
			if (!strcmp(kind, "connected"))      { v->st = P2_CONNECTED; v->guests = 1; }
			else if (!strcmp(kind, "cable"))     { v->link = P2_CABLE; v->url = "zyperos.local/play"; }
			else if (!strcmp(kind, "off"))       v->st = P2_OFF;
			else if (!strcmp(kind, "off-cable")) { v->st = P2_OFF; v->link = P2_CABLE; }
			else if (!strcmp(kind, "none"))      { v->st = P2_OFF; v->link = P2_NO_LINK; }
			else if (!strcmp(kind, "failed"))    v->st = P2_FAILED;
			else if (!strcmp(kind, "starting"))  v->starting = true;
			else if (!strcmp(kind, "closing"))   v->closing = true;
			else if (!strcmp(kind, "closing-cable")) {
				v->closing = true;
				v->link = P2_CABLE;
				v->url = "zyperos.local/play";
			}
		}
		else if (!strcmp(argv[i], "--hotspot")) {
			shot_hot = 1;
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_hot_ssid   = argv[++i];
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_hot_pass   = argv[++i];
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_hot_phones = argv[++i];
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_hot_now    = argv[++i];
		}
		/* --hotspot-in: the hotspot once someone came in (the news) */
		else if (!strcmp(argv[i], "--hotspot-in")) shot_hot_in = 1;
		/* --hare-addr ADDR: the way in's address (a long one, for the fit) */
		else if (!strcmp(argv[i], "--hare-addr") && i + 1 < argc) shot_steps_addr = argv[++i];
		else if (!strcmp(argv[i], "--hare-steps") && i + 2 < argc) {
			shot_steps_way = argv[++i];
			shot_steps_state = argv[++i];
			if (i + 1 < argc && !strcmp(argv[i + 1], "noname")) { shot_steps_noname = true; i++; }
		}
		/* --hare-move idle|in|out|both|closing|finish|gone: File Transfer's
		 * tiles, with --hare or --hotspot-in (the shot's MV) */
		else if (!strcmp(argv[i], "--hare-move") && i + 1 < argc) shot_hare_move = argv[++i];
		else if (!strcmp(argv[i], "--hare")) {
			shot_hare = 1;
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_hare_addr = argv[++i];
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_hare_who  = argv[++i];
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_hare_head = argv[++i];
		}
		else if (!strcmp(argv[i], "--wait") && i + 1 < argc) {
			shot_wait = argv[++i];
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_wait_msg = argv[++i];
		}
		else if (!strcmp(argv[i], "--notice") && i + 1 < argc) {
			shot_notice = argv[++i];
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_notice_head = argv[++i];
		}
		else if (!strcmp(argv[i], "--notice-badge") && i + 1 < argc) {
			shot_notice_badge = argv[++i];
		}
		else if (!strcmp(argv[i], "--notice-progress") && i + 2 < argc) {
			shot_notice_value = atoi(argv[++i]);
			shot_notice_target = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--notice-kind") && i + 1 < argc) {
			const char *k = argv[++i];

			shot_notice_kind = !strcmp(k, "unlock") ? NQ_UNLOCK
			                 : !strcmp(k, "progress") ? NQ_PROGRESS : NQ_SET;
		}
		else if (!strcmp(argv[i], "--notice-heat")) shot_notice_mark = NOTICE_MARK_HEAT;
		else if (!strcmp(argv[i], "--popup") && i + 1 < argc) {
			shot_popup = argv[++i];
			if (!strcmp(shot_popup, "speed") && i + 1 < argc && argv[i + 1][0] != '-')
				shot_popup_half = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--charging") && i + 1 < argc) {
			shot_charging = atoi(argv[++i]);
			if (shot_charging < 0) shot_charging = 0;
			if (i + 1 < argc && !strcmp(argv[i + 1], "hold")) { shot_charging_kind = 1; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "unplugged")) { shot_charging_kind = 2; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "off")) { shot_charging_kind = 3; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "pending")) { shot_charging_kind = 4; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "kept")) { shot_charging_kind = 5; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "limit")) { shot_charging_kind = 6; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "keptfull")) { shot_charging_kind = 7; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "soon")) { shot_charging_kind = 8; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "tofull")) { shot_charging_kind = 9; i++; }
		}
		else if (!strcmp(argv[i], "--heatpop") && i + 1 < argc) {
			shot_heatpop = atoi(argv[++i]);
			if (shot_heatpop < 0) shot_heatpop = 0;
		}
		else if (!strcmp(argv[i], "--lowbatt") && i + 1 < argc) {
			shot_lowbatt = atoi(argv[++i]);
			if (shot_lowbatt < 0) shot_lowbatt = 0;
			if (i + 1 < argc && !strcmp(argv[i + 1], "charging")) { shot_lowbatt_kind = 1; i++; }
			else if (i + 1 < argc && !strcmp(argv[i + 1], "off")) { shot_lowbatt_kind = 2; i++; }
		}
		else if (!strcmp(argv[i], "--keyboard")) {
			shot_kb = 1;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '2')
				shot_kb_layer = atoi(argv[++i]);
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_kb_text = argv[++i];
			if (i + 1 < argc && argv[i + 1][0] != '-') shot_kb_title = argv[++i];
		}
		/* --kbsel ROW COL puts the keyboard's selection on a key, so the
		 * action row can be looked at selected key by key. */
		else if (!strcmp(argv[i], "--kbsel") && i + 2 < argc) {
			shot_kb_row = atoi(argv[i + 1]);
			shot_kb_col = atoi(argv[i + 2]);
			i += 2;
		}
		else if (!strcmp(argv[i], "--slots")) {
			shot_slots = 1;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_slot_sel = atoi(argv[++i]);
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				shot_slot_aspect = (float)atof(argv[++i]);
		}
	}
	/* --status is for a shot, as --theme is: a launcher started
	 * with it and no --shot draws the device's own corner. And a shot draws
	 * a clock only when --status gives one (status_clock). */
	shot_st.shot = shot_path != NULL;
	if (!shot_path) shot_st.set = false;

	/* --wifi prints what the radio can see and exits. The wifi module talks
	 * to firmware that only exists on the device, so it cannot be exercised
	 * from the host at all, and a flag that reports its view is the cheapest
	 * way to tell "the machinery is wrong" from "the network is". */
	if (argc > 1 && !strcmp(argv[1], "--wifi")) {
		wifi_net nets[WIFI_MAX_NETS];
		char ssid[WIFI_SSID_MAX], ip[64];
		int n, i;
		wifi_state st;

		printf("bringing the supplicant up ...\n");
		printf("  wifi_up: %s\n", wifi_up() ? "ok" : "FAILED");
		st = wifi_status(ssid, sizeof ssid, ip, sizeof ip);
		printf("  status: %s  ssid=[%s] ip=[%s]\n",
		       st == WIFI_CONNECTED  ? "connected"  :
		       st == WIFI_CONNECTING ? "connecting" :
		       st == WIFI_IDLE       ? "idle"       : "off", ssid, ip);
		n = wifi_scan(nets, WIFI_MAX_NETS);
		printf("  scan: %d network(s)\n", n);
		for (i = 0; i < n; i++)
			printf("    %-32s %4d dBm  %-9s %s\n", nets[i].ssid,
			       nets[i].signal, nets[i].secured ? "secured" : "open",
			       nets[i].known ? "known" : "");
		return n < 0 ? 1 : 0;
	}

	signal(SIGTERM, on_sigterm);
	signal(SIGINT, on_sigterm);
	/* SSH sessions a launcher before this one left open, if it died with USB
	 * File Transfer's SSH running: nothing must outlive that screen. */
	if (!shot_path) sshlink_stop();

	/* Nothing on the device can read a database - there is no sqlite3 binary -
	 * so losing the ability to SEE a setting would be a real loss where losing
	 * the ability to edit one is the point. Runs before video: it is a
	 * question asked over adb, not a screen. */
	/* Repair, not configuration. A wrong shipped default is permanent on a
	 * device that already seeded it - seeding fills absent keys only, and it
	 * cannot tell "the player chose this" from "we seeded it by mistake" - so
	 * there has to be a way to correct one in place. There is no sqlite3 on
	 * the device to do it with.
	 *
	 * It refuses a key that does not already exist, in either scope. That is
	 * what keeps it a repair tool: a typo cannot invent a setting that looks
	 * real, is written faithfully, and is read by nothing. */
	if (argc > 2 && !strcmp(argv[1], "--set")) {
		char dev[CFG_STR * 2], lib[CFG_STR * 2], env[CFG_STR * 2];
		char key[CFG_STR];
		const char *eq = strchr(argv[2], '=');
		db *target;

		if (!eq || eq == argv[2]) {
			fprintf(stderr, "usage: zyperos.elf --set key=value\n");
			return 2;
		}
		snprintf(key, sizeof key, "%.*s", (int)(eq - argv[2]), argv[2]);
		paths_init();
		db_paths_ready(dev, sizeof dev, lib, sizeof lib, env, sizeof env);
		if (!db_init(dev, lib, env)) {
			fprintf(stderr, "cannot open the settings database\n");
			return 1;
		}
		target = db_has(db_dev(), key) ? db_dev()
		       : db_has(db_lib(), key) ? db_lib() : NULL;
		if (!target) {
			fprintf(stderr, "no setting named '%s' - --dump lists them\n", key);
			db_shutdown();
			return 1;
		}
		if (!db_set_str(target, key, eq + 1)) {
			fprintf(stderr, "could not write '%s'\n", key);
			db_shutdown();
			return 1;
		}
		/* Two of these keys are exported for launch.sh, and a value the
		 * database holds but boot.env does not is a setting that takes effect
		 * everywhere except at boot. Setting wifi=1 and rebooting into a
		 * radio that stays down is how that was found. */
		db_write_boot_env();
		printf("%s = %s\n", key, eq + 1);
		db_shutdown();
		return 0;
	}

	/* --meta <file>: fill the card's games table from scraped rows.
	 *
	 * The scrape runs on a computer for now, and from there the card's
	 * database cannot simply be written: the launcher holds it open
	 * in WAL mode for as long as the device is on, and swapping the file under
	 * a live SQLite handle is how a card loses its favorites and its play time.
	 * Measured on this device: a 76 KB library.db beside a 161 KB -wal.
	 *
	 * So the rows travel as a file and the writing happens HERE, in a second
	 * process, where SQLite arbitrates between the two the way it is built to.
	 * Runs before video, like --set and --dump: it is a question asked over
	 * adb, not a screen, and nothing here presents.
	 *
	 * A record is a tab-separated header - folder, file, year, publisher,
	 * developer, players, genres, esrb, note, and the synopsis length in bytes
	 * - then that many bytes of synopsis and a newline. COUNTED RATHER THAN
	 * ESCAPED, because the synopsis is the one field with newlines in it and 42
	 * of the 78 replies this card's games got have them; an escape is a second
	 * thing to get right at both ends. */
	if (argc > 2 && !strcmp(argv[1], "--meta")) {
		char dev[CFG_STR * 2], lib[CFG_STR * 2], head[1024];
		FILE *f = fopen(argv[2], "rb");
		int wrote = 0, bad = 0;

		if (!f) {
			fprintf(stderr, "cannot read %s\n", argv[2]);
			return 1;
		}
		paths_init();
		db_paths_ready(dev, sizeof dev, lib, sizeof lib, NULL, 0);
		if (!db_init(dev, lib, NULL)) {
			fprintf(stderr, "cannot open the library database\n");
			fclose(f);
			return 1;
		}
		while (fgets(head, sizeof head, f)) {
			game_meta m = { 0 };
			char *fld[10], *p = head;
			size_t want, take;
			long len;
			int i;

			for (i = 0; i < 9; i++) {
				char *t = strchr(p, '\t');

				if (!t) break;
				*t = '\0';
				fld[i] = p;
				p = t + 1;
			}
			if (i < 9) { bad++; continue; }
			fld[9] = p;
			/* A length that is not a length means the file is not what it says
			 * and every byte after it is at an unknown offset. Stop, rather
			 * than write whatever the rest happens to parse as.
			 *
			 * Checked digit by digit rather than left to strtol, which reads
			 * "nine" as 0 - a perfectly valid length, and the row lands with
			 * its synopsis quietly gone and the reader none the wiser. */
			for (p = fld[9]; *p >= '0' && *p <= '9'; p++)
				;
			if (p == fld[9] || (*p != '\n' && *p != '\r' && *p != '\0')) {
				bad++;
				break;
			}
			len = strtol(fld[9], NULL, 10);
			if (len < 0 || len > 1 << 20) { bad++; break; }
			want = (size_t)len;
			take = want < sizeof m.synopsis ? want : sizeof m.synopsis - 1;
			if (fread(m.synopsis, 1, take, f) != take) { bad++; break; }
			m.synopsis[take] = '\0';
			for (want -= take; want > 0; want--)
				if (fgetc(f) == EOF) break;
			fgetc(f);                            /* the record's newline */
			snprintf(m.year,      sizeof m.year,      "%s", fld[2]);
			snprintf(m.publisher, sizeof m.publisher, "%s", fld[3]);
			snprintf(m.developer, sizeof m.developer, "%s", fld[4]);
			snprintf(m.players,   sizeof m.players,   "%s", fld[5]);
			snprintf(m.genres,    sizeof m.genres,    "%s", fld[6]);
			snprintf(m.esrb,      sizeof m.esrb,      "%s", fld[7]);
			snprintf(m.note,      sizeof m.note,      "%s", fld[8]);
			if (db_game_set(db_lib(), card_folder(fld[0]), fld[1], &m)) wrote++;
			else bad++;
		}
		fclose(f);
		printf("%d written, %d rejected\n", wrote, bad);
		db_shutdown();
		return bad && !wrote ? 1 : 0;
	}

	/* --titles <file>: display titles for the card's games, from a computer.
	 *
	 * --meta's companion, written for the same reason and in the same second
	 * process: the launcher holds the library database open in WAL mode, so
	 * a title made on a computer (a LaunchBox or No-Intro name for a ROM
	 * whose file is called "060 Advance Wars 2.gba") is written here, where
	 * SQLite arbitrates, rather than into a copy of the file.
	 *
	 * One line a game: folder, file and title, separated by tabs, the file
	 * as the shelf keys it (a disc folder's "Folder/Game.m3u"). Only the
	 * title changes: the row's year, genres and synopsis are read first and
	 * written back as they were, and a game with no row gets one holding the
	 * title alone. An empty title is skipped rather than clearing one. */
	if (argc > 2 && !strcmp(argv[1], "--titles")) {
		char dev[CFG_STR * 2], lib[CFG_STR * 2], line[2048];
		FILE *f = fopen(argv[2], "rb");
		int wrote = 0, bad = 0;

		if (!f) {
			fprintf(stderr, "cannot read %s\n", argv[2]);
			return 1;
		}
		paths_init();
		db_paths_ready(dev, sizeof dev, lib, sizeof lib, NULL, 0);
		if (!db_init(dev, lib, NULL)) {
			fprintf(stderr, "cannot open the library database\n");
			fclose(f);
			return 1;
		}
		while (fgets(line, sizeof line, f)) {
			char *folder = line, *file, *title, *end;
			game_meta m;

			line[strcspn(line, "\r\n")] = '\0';
			if (!line[0]) continue;
			if (!(file = strchr(folder, '\t')) || !(title = strchr(file + 1, '\t'))) {
				bad++;
				continue;
			}
			*file++ = '\0';
			*title++ = '\0';
			if ((end = strchr(title, '\t'))) *end = '\0';
			if (!folder[0] || !file[0] || !title[0]) { bad++; continue; }
			folder = (char *)card_folder(folder);
			db_game_get(db_lib(), folder, file, &m);
			snprintf(m.title, sizeof m.title, "%s", title);
			if (db_game_set(db_lib(), folder, file, &m)) wrote++;
			else bad++;
		}
		fclose(f);
		printf("%d titles written, %d rejected\n", wrote, bad);
		db_shutdown();
		return bad && !wrote ? 1 : 0;
	}

	/* --scrape [folder] runs the box art scraper with no video at all.
	 *
	 * The same art_begin/art_step the screen drives, stepped from here instead
	 * of from a frame loop, so a long run does not depend on somebody holding
	 * the device - and so that re-scraping just a few games is a five-minute
	 * job with a command rather than a menu to navigate.
	 *
	 * Headless, so it must NOT touch video: the live launcher is presenting
	 * while this runs, and two presenters wedge the display engine. It also
	 * takes the one async network slot, which is per process, so the launcher
	 * is unaffected either way.
	 *
	 * The shrinker runs inline here rather than on a worker - art_shrink falls
	 * back to doing it on the spot when no thread was started - which is what
	 * a run with nothing to draw wants anyway. */
	if (argc > 1 && !strcmp(argv[1], "--scrape")) {
		char dev[CFG_STR * 2], lib[CFG_STR * 2], cp[CFG_STR * 2], path[CFG_STR * 2];
		const char *only_sys = argc > 2 ? argv[2] : NULL;
		systems_cfg all, one;
		art_progress st;
		char seen[128] = "";
		int r;

		paths_init();
		snprintf(path, sizeof path, "%s/systems.cfg", P_ROOT);
		if (!cfg_load_systems(path, &all)) {
			fprintf(stderr, "no usable %s\n", path);
			return 1;
		}
		/* The folders the shelves read, EmulationStation's where they read
		 * one, and its covers and videos counted as covers and videos */
		shelf_folders_from_card(&all);
		art_es_media(es_art_cover, es_art_video);
		db_paths_ready(dev, sizeof dev, lib, sizeof lib, NULL, 0);
		if (!db_init(dev, lib, NULL)) {
			fprintf(stderr, "cannot open the databases\n");
			return 1;
		}
		snprintf(cp, sizeof cp, "%s/cacert.pem", P_ROOT);
		net_set_ca_path(cp);
		ss_creds_load();
		sgdb_key_load();
		ss_run_videos(snap_wanted);
		IMG_Init(IMG_INIT_PNG);

		if (only_sys) {
			int i;

			memset(&one, 0, sizeof one);
			for (i = 0; i < all.count; i++)
				if (!strcmp(all.systems[i].folder, only_sys) || !strcmp(all.systems[i].home, only_sys)) {
					one.systems[0] = all.systems[i];
					one.count = 1;
					break;
				}
			if (!one.count) {
				fprintf(stderr, "no shelf called %s\n", only_sys);
				return 1;
			}
		}
		fprintf(stderr, "scrape: %s, ScreenScraper %s, SteamGridDB %s\n",
		        only_sys ? only_sys : "every shelf",
		        ss_signed_in() ? "signed in" : "not signed in",
		        sgdb_have_key() ? "with a key" : "no key");
		art_begin(only_sys ? &one : &all, P_ROMS, NULL);
		while ((r = art_step()) == 1) {
			art_status(&st);
			/* One line per thing it turns to, not per step: a step is mostly
			 * a poll of a request already in flight. Keyed on what it says it
			 * is doing rather than on the counters, because the interesting
			 * case - a game asked about and not found - moves no counter. */
			if (strcmp(st.now, seen)) {
				snprintf(seen, sizeof seen, "%s", st.now);
				fprintf(stderr, "scrape: %-44s  %d found, %d missing, %d had one, %d names saved\n",
				        st.now, st.found, st.missing, st.skipped, st.named);
			}
			usleep(20000);
		}
		art_status(&st);
		fprintf(stderr, "scrape: done - %d found, %d missing, %d already had one, %d names saved%s%s\n",
		        st.found, st.missing, st.skipped, st.named,
		        st.problem[0] ? " - " : "", st.problem);
		return 0;
	}

	if (argc > 1 && !strcmp(argv[1], "--dump")) {
		char dev[CFG_STR * 2], lib[CFG_STR * 2];
		paths_init();
		db_paths_ready(dev, sizeof dev, lib, sizeof lib, NULL, 0);
		if (!db_init(dev, lib, NULL)) {
			fprintf(stderr, "cannot open the settings database\n");
			return 1;
		}
		printf("device  %s\n", dev);
		db_dump(db_dev(), stdout);
		printf("library %s\n", lib);
		db_dump(db_lib(), stdout);
		db_shutdown();
		return 0;
	}

	/* --game-rows TAG FILE: one game's rows in the library database (its
	 * play sessions, its favorite, its Pak; src/gamerows.h) on stdout, and
	 * --game-rows-put TAG FILE: that game's rows made what stdin holds. For
	 * px2-snap (tools/px2-snap.sh), which keeps a game's saves and states and
	 * these beside them. Any number of arguments is answered here, never
	 * by a second launcher. */
	if (argc > 1 && (!strcmp(argv[1], "--game-rows") || !strcmp(argv[1], "--game-rows-put"))) {
		char dev[CFG_STR * 2], lib[CFG_STR * 2];
		bool put = !strcmp(argv[1], "--game-rows-put");
		int n;

		if (argc != 4 || !argv[2][0] || !argv[3][0]) {
			fprintf(stderr, "usage: zyperos.elf %s TAG FILE\n", argv[1]);
			return 2;
		}
		paths_init();
		db_paths_ready(dev, sizeof dev, lib, sizeof lib, NULL, 0);
		if (!db_init(dev, lib, NULL)) {
			fprintf(stderr, "cannot open the settings database\n");
			return 1;
		}
		n = put ? gamerows_put(db_lib(), argv[2], argv[3], stdin)
		        : gamerows_dump(db_lib(), argv[2], argv[3], stdout);
		db_shutdown();
		if (n < 0) {
			fprintf(stderr, put ? "game-rows: not one game's rows for %s %s; nothing changed\n"
			                    : "game-rows: could not read the rows of %s %s\n",
			        argv[2], argv[3]);
			return 1;
		}
		if (put) fprintf(stderr, "game-rows: %d row(s) for %s %s\n", n, argv[2], argv[3]);
		return 0;
	}

	/* --coreopts TAG: the core options the launcher sends a game of the system
	 * TAG (systems.cfg's tag field, "NDS"), one key=value a line in the order
	 * they are sent, with the language the card has saved in force: the
	 * Nintendo DS's language is made from it (src/dslang.h), and
	 * tools/dslang-check.sh holds that it is. */
	if (argc > 2 && !strcmp(argv[1], "--coreopts")) {
		char dev[CFG_STR * 2], lib[CFG_STR * 2], lc[16];
		int i;

		paths_init();
		db_paths_ready(dev, sizeof dev, lib, sizeof lib, NULL, 0);
		if (!db_init(dev, lib, NULL)) {
			fprintf(stderr, "cannot open the settings database\n");
			return 1;
		}
		db_get_str(db_dev(), "language", lc, sizeof lc, "en");
		i18n_set(lang_parse(lc));
		for (i = 0; i < plat_coreopt_count(argv[2]); i++)
			puts(plat_coreopt(argv[2], i));
		db_shutdown();
		return 0;
	}

	t_boot0 = plat_now_ms();
	paths_init();

	/* Before anything reads a setting. Two databases, because the files this
	 * replaced kept per-device and per-card data apart on purpose - see db.h.
	 * A failure here is fatal rather than silently defaulted: every setting
	 * the launcher has would be a fallback, and the player would find their
	 * volume, brightness and account quietly reset with nothing said. */
	{
		char dev[CFG_STR * 2], lib[CFG_STR * 2], env[CFG_STR * 2];
		db_paths_ready(dev, sizeof dev, lib, sizeof lib, env, sizeof env);
		/* NOT fatal, and the reason is what "fatal" costs here. launch.sh
		 * restarts the launcher when it exits, and five exits inside five
		 * seconds each makes it give up and call poweroff - so returning 1
		 * from this point turns a settings problem into a device that will
		 * not boot. It is also BEFORE plat_resident_ready() below, which is
		 * what stops a running game so that only one process is presenting;
		 * exiting here skips that entirely.
		 *
		 * Every getter takes what to say instead, and db_dev()/db_lib()
		 * return NULL harmlessly, so the launcher comes up on the shipped
		 * defaults with no favorites, no turbo and no core options. Worse
		 * than working, far better than off - and it says so where anyone
		 * looking at the log will find it. */
		if (!db_init(dev, lib, env)) {
			fprintf(stderr, "settings: cannot open the database at %s\n", dev);
			fprintf(stderr, "settings: continuing on the shipped defaults - "
			                "favorites, turbo and core options will be absent, "
			                "and nothing set here will be remembered\n");
		}
		/* Rewritten at every boot, not only on change: it is derived, so a
		 * card that lost it or never had one gets a correct one for free. */
		db_write_boot_env();
		/* Developer Mode's file for plastron's S40usbgadget, the same way */
		if (!shot_path) usblink_sync(db_dev(), P_USERDATA);
		if (!shot_path) usblink_release_dev(devmode_unlocked(db_dev()));
	}
	/* Local time is the card's chosen zone, in this process, from here on,
	 * and Diatom's with each game (resident.c's RUN). The system itself stays
	 * on UTC. Before it, 1.0.5's default: Auto on, once for a card from
	 * before (src/autozone.h), never in a shot, whose cards are scratch. */
	{
		char tz[64];

		if (!shot_path && autozone_migrate(db_lib()))
			fprintf(stderr, "clock: auto zone %s, 1.0.5's default, once\n",
			        db_get_int(db_lib(), AUTOZONE_KEY, 0) == 1 ? "on" : "not on");
		db_get_str(db_lib(), "timezone", tz, sizeof tz, "America/New_York");
		plat_clock_zone(tz);
	}
	t_mark("databases");

	snprintf(path, sizeof path, "%s/systems.cfg", P_ROOT);
	if (!cfg_load_systems(path, &a.sys)) {
		fprintf(stderr, "no usable %s\n", path);
		return 1;
	}
	system_order_load();
	system_order_apply(&a.sys);

	/* Before the scan, because the scan builds the Favorites shelf out of
	 * them and a shelf cannot be built from a list that has not been read. */
	fav_load();

	/* Close any session the device never saw end - a power cut, a flat
	 * battery - at its last checkpoint, before anything reads a total. See
	 * stats.h: short by up to the checkpoint interval beats lost entirely. */
	stats_recover();

	{	char cp[CFG_STR * 2];
		chv_earned_load();
		plat_resident_on_unlock(on_cheevo_unlocked);
		plat_resident_on_progress(on_cheevo_progress);
		plat_resident_on_speed(on_speed);
		plat_resident_on_stick(on_stick);
		plat_resident_on_quick(on_quick);
		plat_resident_on_tick(on_game_tick);
		plat_resident_on_battery_empty(battery_empty);
		p2_setup();
		heat_init(&g_heat);

		/* Without this every HTTPS request fails verification, because the
		 * device has no trust store of its own - res/ssl/README.md. */
		a.auto_off = auto_off_load();
		g_saver_s = saver_load();
		g_charge_limit = charge_limit_load();
		/* A full charge asked for ends with the cable out, here too */
		if (!on_charger()) remove(CHG_FULL_ONCE);
		charge_limit_apply();
		g_aout_policy = aout_load();
		g_channels = channels_load();

		snprintf(cp, sizeof cp, "%s/cacert.pem", P_ROOT);
		net_set_ca_path(cp);
		ra_creds_load();
		ss_creds_load();
		sgdb_key_load();
		ss_run_videos(snap_wanted);
		{
			char bin[CFG_STR * 2], music[CFG_STR * 2];

			snprintf(bin, sizeof bin, "%s/muse", P_ROOT);
			snprintf(music, sizeof music, "%s", P_CARD);
			/* A shot starts no daemon: it draws one frame and exits, and a
			 * cover it would ask for is drawn as missing instead - see
			 * muse_covers_settle. */
			musec_init(shot_path ? "" : bin, music);
			musec_on_before_heard(aout_before_muse);
			musec_names_by(muse_track_names);
		}
		/* The play mode the player left it in. */
		{
			char m[32];
			int k;

			db_get_str(db_dev(), "muse.mode", m, sizeof m, "in order");
			if (shot_mode) snprintf(m, sizeof m, "%s", shot_mode);
			for (k = 0; k < MUQ_MODES; k++)
				if (!strcmp(m, MUSE_MODES[k].key)) musec_set_mode((muq_mode)k);
		}
	}
	t_mark("muse+accounts");

	/* Scan every system now, not when one is opened: it is three directory
	 * reads, it happens behind the boot animation, and it means walking into
	 * a system is a frame rather than a wait.
	 *
	 * On a thread of its own, while this one starts the display, the input,
	 * the sounds and the faces, which need nothing from it: the shelves are
	 * first wanted after the join below ("scan joined"). On my card
	 * (8,844 games, exFAT) the scan was 514 ms of a 900 ms start and the
	 * display's start 190 ms, one after the other (2026-11-07). The
	 * databases are shared with this thread, and db_open makes every
	 * connection serialized for that; nothing here reads a.sys, a.view or
	 * the MP4 library until the join.
	 *
	 * The folders' listings come from the saved library when there is one
	 * (src/savedlib.h), and the card is read again once the shelf is up
	 * (after_start_step): after a cold boot, 742 ms to the first frame on
	 * My card became 506-521 ms (2026-11-07). */
	SDL_Thread *scanner = SDL_CreateThread(scan_thread, "zyperos-scan", &a);

	if (!scanner) scan_thread(&a);

	/* The theme, from the saved Appearance - or from --theme, for a shot,
	 * which is asked to be one theme or the other whatever the device has
	 * saved. theme_init also writes THEME_FILE (src/themefile.h) for the two
	 * programs that draw the volume and brightness bar without this one,
	 * Diatom over a game and portkeys over a port, so the file says the
	 * theme that is on from the first frame and not one that was.
	 *
	 * After the databases are open, because that is where the choice is
	 * kept, and before ui_init and the first thing drawn, so whatever asks
	 * theme() for its colors has an answer. A /tmp that cannot be written to
	 * is a line in the log and nothing more, as theme.h says. */
	{
		char ap[16];

		/* A shot writes no theme file. It is run beside a launcher on a
		 * development host as often as not, and THEME_FILE is that
		 * launcher's word: a shot in the other theme must not turn its
		 * bar over. */
		if (shot_path) theme_set_file(NULL);
		db_get_str(db_dev(), "appearance", ap, sizeof ap,
		           appearance_id(APPEAR_LIGHT));
		/* The language, MENU > Language's, or --lang's for a shot: every
		 * string from here on is in it (src/i18n.h). English until a first
		 * start's step or the menu says otherwise. Its faces once ui_init
		 * has opened them, below. */
		{
			char lc[16];

			db_get_str(db_dev(), "language", lc, sizeof lc, "en");
			i18n_set(lang_parse(shot_path && shot_lang ? shot_lang : lc));
		}
		look_load();
		/* --look's, for a shot: ACCENT,LIGHT,DARK,CORNERS,ZYPER as the
		 * database keeps them ("blue,cool,deepblue,square,off") */
		if (shot_path && shot_look) {
			char w[5][16] = { "", "", "", "", "" };
			look l;

			sscanf(shot_look, "%15[^,],%15[^,],%15[^,],%15[^,],%15s",
			       w[0], w[1], w[2], w[3], w[4]);
			l = look_parse(w[0], w[1], w[2], w[3], w[4]);
			look_set(&l);
			look_bar_colors();
		}
		theme_init(shot_path && shot_theme >= 0 ? (appearance)shot_theme
		                                        : appearance_parse(ap));
	}
	{
		char value[24];
		db_get_str(db_dev(), "home", value, sizeof value, "spotlight");
		a.home_layout = shot_path && shot_home >= 0 ? (home_layout)shot_home : home_parse(value);
	}
	t_mark("theme");

	/* After IMG_Init: the worker calls IMG_Load. Before the display, because
	 * neither needs it - decoding is CPU work, and making textures of what it
	 * decodes waits for the renderer, on this thread, in texload_drain - and
	 * because the threads would otherwise sit idle through video, input and
	 * settings. Home textures and game covers use the same worker.
	 *
	 * Not for --shot. A shot draws the shelf exactly once, and with the workers
	 * running every card on that one frame is only queued - so the PNG came out
	 * with the title, the count and the rail and not a single card, while the
	 * log line named the focused game as though it had been drawn. With no
	 * worker, game_get_tex decodes on the frame the way it always did, which is
	 * what a tool for checking one frame wants: the right picture, not a fast
	 * one. */
	IMG_Init(IMG_INIT_PNG);
	texload_set_late(es_cover_late);
	art_es_media(es_art_cover, es_art_video);
	if (!shot_path) texload_start();

	/* The interface sounds, which read their files on a thread of their own,
	 * so the first frame waits for none of them. Not for --shot, which
	 * presses nothing, nor a headless run (ZYPEROS_HEADLESS, a check's): no
	 * business opening the host's audio. */
	{
		char v[16], dir[CFG_STR * 2];

		db_get_str(db_dev(), "uisounds", v, sizeof v,
		           uisound_setting_id(DS_VOICE_DEFAULT));
		uisound_set(uisound_setting_parse(v));
		snprintf(dir, sizeof dir, "%s/res/sounds", P_ROOT);
		if (!shot_path && !getenv("ZYPEROS_HEADLESS")) uisound_init(dir);
	}
	/* Menu music, the same way: settings now, the folder read on its own
	 * thread, and nothing heard until the shelf's first pass (src/bgm.h) */
	{
		char v[16], track[512], dir[CFG_STR * 2], builtin[CFG_STR * 2];

		db_get_str(db_dev(), "bgm", v, sizeof v, "on");
		bgm_set_on(bgm_on_parse(v));
		db_get_str(db_dev(), "bgm.volume", v, sizeof v, "");
		bgm_set_volume(bgm_volume_parse(v));
		db_get_str(db_dev(), "bgm.track", track, sizeof track, "");
		bgm_set_track(track);
		db_get_str(db_dev(), "bgm.popup", v, sizeof v, "on");
		g_song_popup = bgm_on_parse(v);
		db_get_str(db_dev(), "bgm.builtin", v, sizeof v, "on");
		bgm_set_builtin(bgm_on_parse(v));
		bgm_dirs(dir, builtin, sizeof dir);
		/* The card's own folder, there to be filled from a computer or File
		 * Transfer's page, on a card older than it too (mk/payload.sh makes
		 * it on a new one) */
		if (!shot_path) mkdir(dir, 0755);
		if (!shot_path && !getenv("ZYPEROS_HEADLESS")) bgm_init(dir, builtin);
	}

	/* BEFORE the first frame this process ever draws: if a previous launcher
	 * died while a game was running, the resident is still presenting through
	 * fbdev right now, and drawing the shelf over it is the two-presenter
	 * case that wedges the display engine in-kernel (Diatom's handoff spike;
	 * it cost a power cycle to prove, twice). plat_resident_ready() connects,
	 * and on READY state=running it stops the game and drains to EXIT - so by
	 * the time video comes up, only one presenter exists. The check at launch
	 * time was too late by definition: this process draws long before the
	 * player launches anything. */
	plat_resident_ready();
	t_mark("diatom connect");

	if (!plat_video_init()) {
		fprintf(stderr, "video init failed\n");
		if (scanner) SDL_WaitThread(scanner, NULL);
		return 1;
	}
	t_mark("video");
	a.r = plat_renderer();
	tour_hint_draw_set(keyhints_width, tour_hint);
	t_mark("renderer");
	/* The replug message, over every screen (DESIGN.md 11.36) */
	plat_set_overlay(replug_overlay);
	plat_input_init();
	t_mark("input");
	/* Nothing is handed in any more. The two-tier lookup this replaces - a
	 * shipped default and the player's saved level - is one key each in the
	 * database, seeded once and overwritten by a nudge. That is also the end
	 * of a bug it kept reintroducing: reapplying the config afterwards put
	 * the shipped default ahead of the level the player last chose. */
	plat_settings_init();
	t_mark("settings");

	/* Before ui_init, which is where the sizes are decided; it persists across
	 * the ui_quit/ui_init pair the standalone-emulator fallback goes through. */
	/* The size the player chose beats the shipped default, the same way a
	 * saved brightness does. Read before ui_init, which is when the scale is
	 * applied. */
	if (!ui_init(a.r, P_FONT)) fprintf(stderr, "font init failed\n");
	/* The language's faces: Lexend's, opened above, or 简体中文's, opened now */
	ui_fonts_for(i18n_lang());
	page_fonts_init(P_FONT);
	/* ui_init on its own, between "settings" and here: the fonts, the glow
	 * and Beard's three shadow textures, every one made before the first
	 * frame (src/ui.c says why), and made again after every fallback game. */
	t_mark("ui");

	/* The library, read meanwhile (scan_thread): what follows needs it */
	if (scanner) SDL_WaitThread(scanner, NULL);
	t_mark("scan joined");
	/* After the scan, because it indexes by system, and before anything can
	 * launch, because the mode has to reach Diatom with the first RUN. */
	display_load(&a);
	core_load(&a);
	sort_load(&a);
	view_load(&a);
	sort_all(&a);
	t_mark("scan");

	/* MENU > Clock, which the corner's first frame is drawn in */
	clock24_load();
	batt_icon_load();
	if (!shot_path) usb_auto_load();
	if (shot_path && shot_st.h24 >= 0) g_clock24 = shot_st.h24;
	if (shot_path && shot_st.icon >= 0) g_batt_mode = shot_st.icon;
	a.sys_cursor = 0;
	db_get_str(db_lib(), "startup_system", startup, sizeof startup, "");
	if (startup[0])
		for (int i = 0; i < a.sys.count; i++)
			if (strcasecmp(a.sys.systems[i].name, startup) == 0) {
				a.sys_cursor = i;
				break;
			}
	restore_place(&a);
	/* After restore_place, so it wins: `.last` says where the shelf was and
	 * this says what was actually being played. Before the card priming
	 * below, because that loads textures for whatever the cursor is on, and
	 * a fallback to the shelf should find the right ones there. */
	a.resume_menu = playing_restore(&a);
	/* Back from a port: on its game list, as a Diatom game comes back. */
	if (!access(PORT_RETURN, F_OK)) {
		unlink(PORT_RETURN);
		if (!a.resume_menu && a.view[a.sys_cursor].list.count > 0) {
			a.screen = SCREEN_GAMES;
			cf_reset(&a.view[a.sys_cursor].cf, a.view[a.sys_cursor].cursor);
		games_reset(&a.view[a.sys_cursor].row, a.view[a.sys_cursor].cursor, a.view[a.sys_cursor].list.count, plat_now_ms());
		}
	}
	/* MENU+START (tools/zyhome.c): the home, whatever was running and
	 * whatever the records say */
	if (!access(CTL_HOME_FLAG, F_OK)) {
		unlink(CTL_HOME_FLAG);
		a.resume_menu = false;
		playing_clear();
		a.screen = SCREEN_SYSTEMS;
		fprintf(stderr, "zyhome: started on the home\n");
	}
	home_reset(&a.home, a.sys_cursor, a.sys.count, plat_now_ms());
	a.tint = a.sys.systems[a.sys_cursor].accent;
	t_mark("restore place");

	/* Home art decodes on texload's workers. Finished art wakes the loop. */
	prime_window(&a, a.sys_cursor);
	prime_home(&a);
	t_mark("card assets");

	a.running = true;
	last_tint_ms = plat_now_ms();

	if (shot_path) {
		if (shot_screen >= 0) a.screen = (screen_id)shot_screen;
		if (shot_view) a.view[a.sys_cursor].listed = !strcmp(shot_view, "list");
		if (shot_folder) shelf_open_dir(&a, a.sys_cursor, shot_folder, NULL);
		if (shot_game) shelf_reveal(&a, a.sys_cursor, shot_game);
		for (const char *k = shot_shelf_press; k && *k; ) {
			static const struct { const char *w; in_button b; } KEYS[] = {
				{ "a", IN_ACCEPT }, { "b", IN_BACK }, { "x", IN_X }, { "y", IN_Y },
				{ "up", IN_UP }, { "down", IN_DOWN }, { "left", IN_LEFT },
				{ "right", IN_RIGHT }, { "l1", IN_L1 }, { "r1", IN_R1 }, { "start", IN_START },
			};
			size_t len = strcspn(k, ",");
			unsigned j;

			for (j = 0; j < sizeof KEYS / sizeof KEYS[0]; j++)
				if (strlen(KEYS[j].w) == len && !strncmp(KEYS[j].w, k, len)) break;
			if (j == sizeof KEYS / sizeof KEYS[0]) {
				fprintf(stderr, "--shelf-press: %.*s is no key\n", (int)len, k);
				return 2;
			}
			memset(&a.in, 0, sizeof a.in);
			a.in.pressed[KEYS[j].b] = true;
			a.screen = SCREEN_GAMES;
			update_games(&a);
			memset(&a.in, 0, sizeof a.in);
			fprintf(stderr, "shelf-press: %s -> %s [%s] %d/%d\n", KEYS[j].w,
			        a.view[a.sys_cursor].dir[0] ? a.view[a.sys_cursor].dir : "top",
			        a.view[a.sys_cursor].list.count ? a.view[a.sys_cursor].list.items[a.view[a.sys_cursor].cursor].file : "",
			        a.view[a.sys_cursor].cursor + 1, a.view[a.sys_cursor].list.count);
			k += len + (k[len] == ',');
		}
		if (shot_jump) {
			sysview *v = &a.view[a.sys_cursor];
			int k, dir = shot_jump > 0 ? 1 : -1;
			for (k = 0; k < (shot_jump < 0 ? -shot_jump : shot_jump); k++)
				if (v->list.count > 0) v->cursor = shelf_letter_jump(v, dir);
		}
		/* A shelf nobody has moved sits at position 0, wherever its cursor
		 * was restored to. On the device that never reaches the screen:
		 * update_games runs cf_set_cursor_dir every pass, which snaps an
		 * unmoved shelf to its cursor. A shot draws one frame and never runs
		 * that pass, so without this it drew the list's first card under the
		 * selected game's title. */
		cf_reset(&a.view[a.sys_cursor].cf, a.view[a.sys_cursor].cursor);
		games_reset(&a.view[a.sys_cursor].row, a.view[a.sys_cursor].cursor, a.view[a.sys_cursor].list.count, plat_now_ms());

		if (shot_song) home_song_update(&g_home_song, true, shot_song_paused, shot_song);
		g_home_phase = shot_phase >= 0 ? shot_phase : 0;
		if (shot_sysmove != 0.0f && a.sys.count > 1) {
			unsigned now = plat_now_ms();
			home_reset(&a.home, a.sys_cursor, a.sys.count, now);
			a.sys_cursor = (a.sys_cursor + 1) % a.sys.count;
			home_select(&a.home, a.sys_cursor, a.sys.count, 1, now);
			g_home_phase = (int)(shot_sysmove * DS_MS_MOVE);
		}

		/* The games shelf, with the cursor ALREADY at the destination - which
		 * is the state the device is in for the whole of a move, and the reason
		 * anything drawn from the cursor arrives early. */
		if (shot_gamemove != 0.0f) {
			sysview *v = &a.view[a.sys_cursor];

			if (v->list.count > 1) {
				int was = v->cursor;

				v->cursor = (v->cursor + 1) % v->list.count;
				cf_stage(&v->cf, (float)was, (float)was + 1.0f, shot_gamemove);
				games_reset(&v->row, was, v->list.count, plat_now_ms());
				games_select(&v->row, v->cursor, v->list.count, 1, v->row.moved);
				g_games_phase = (int)(shot_gamemove * DS_MS_MOVE);
			}
		}
		/* --delete-now: Delete Game's yes on the game under the cursor, and
		 * the shot is the shelf after it (tools/gamedel-shot-check.py) */
		if (shot_delete_now && a.sys.count > 0 && a.view[a.sys_cursor].list.count > 0) {
			sysview *v = &a.view[a.sys_cursor];
			char said[192];
			bool gone = game_delete_now(&a, shelf_owner(&a, a.sys_cursor, v->cursor),
			                            &v->list.items[v->cursor], false, said, sizeof said);

			fprintf(stderr, "delete: %s. %s\n", gone ? "deleted" : "kept", said);
		}
		if (a.screen == SCREEN_GAMES) prime_window(&a, a.sys_cursor);
		if (shot_at >= 0 && !shot_launch && !shot_portload && !shot_poweroff &&
		    (!shot_kb || shot_press != SHOT_STEP))
			shot_motion(&a);
		if (shot_portload) shot_portload_take(&a);
		else take_shot(&a);
		goto done;
	}

	/* Dev instrumentation, same standing as --shot: launch one game with no
	 * buttons pressed, so the resident path can be exercised over adb with
	 * nobody holding the device. ZYPEROS_AUTOLAUNCH="TAG<tab>rom-filename";
	 * pair with ZYPEROS_AUTOSTOP_S to end the game on a clock. */
	{
		const char *auto_spec = getenv("ZYPEROS_AUTOLAUNCH");
		if (auto_spec && strchr(auto_spec, '\t')) {
			char tag[64], file[LIB_PATH];
			const char *bar = strchr(auto_spec, '\t');
			int si, gi, found = 0;
			snprintf(tag, sizeof tag, "%.*s", (int)(bar - auto_spec), auto_spec);
			snprintf(file, sizeof file, "%s", bar + 1);
			for (si = 0; si < a.sys.count && !found; si++) {
				if (strcmp(a.sys.systems[si].tag, tag) != 0) continue;
				for (gi = 0; gi < a.view[si].list.count; gi++) {
					const char *b = strrchr(a.view[si].list.items[gi].file, '/');
					b = b ? b + 1 : a.view[si].list.items[gi].file;
					if (strcmp(b, file) == 0) {
						a.sys_cursor = si;
						a.view[si].cursor = gi;
						found = 1;
						break;
					}
				}
			}
			if (found) {
				fprintf(stderr, "autolaunch: %s / %s\n", tag, file);
				launch(&a);
			} else {
				fprintf(stderr, "autolaunch: no %s / %s in the library\n", tag, file);
			}
			goto done;
		}
	}

	/* px2-open's commands, from here on: a game resumed at boot, the
	 * tutorial and the shelf (src/ctl.h) */
	ctl_start(&a);

	/* Nothing pressed during the boot is honored. How long that takes is the
	 * device's to say - see plat_input_settle. */
	plat_input_settle(&a.in);

	/* A start a charger made shows the charging screen first (DESIGN.md
	 * 11.25): held POWER goes on with this start, unplugged turns it off */
	if (a.running && !charge_boot_gate(&a)) goto done;

	/* A battery too low to play says so first, before a resume, the
	 * tutorial or the shelf, and powers off (DESIGN.md 11.24); with a
	 * charger plugged in meanwhile the start goes on below. After the
	 * settle, so the press that switched the handheld on is no POWER. */
	if (a.running && !lowbatt_gate(&a)) goto done;

	/* Straight back into the game, before the shelf is ever drawn. After the
	 * input grace above, so a wake press replayed by the boot does not land in
	 * the menu that is about to open. */
	if (a.resume_menu && a.running) {
		a.screen = SCREEN_GAMES;
		launch(&a);
	}
	/* Or, on a handheld that has never shown it, the tutorial, before the
	 * shelf is ever drawn (DESIGN.md 11.10). Not on a start straight back
	 * into a game: that handheld has been played, and it runs at the next
	 * start that comes to the shelf. */
	else if (a.running && tour_wanted(db_dev())) {
		fprintf(stderr, "tour: %s is not set, so the tutorial comes first\n", TOUR_DONE_KEY);
		tour_run(&a, true);
	}
	/* Or, back from a PortMaster that closed with an error, its page with
	 * what happened and Reinstall PortMaster */
	else if (a.running && !shot_path) {
		pm_after_run(&a);
	}
	/* Its record read or not, it is this start's alone */
	if (!shot_path) unlink(PM_RAN);

	/* Drawing only when something changed. See g_redraw_at.
	 *
	 * The loop still runs every IDLE_POLL_MS when nothing is drawn, so the power
	 * button, Auto Off, the headphone jack and the Bluetooth sink are all still
	 * checked at the same rate as before - only the draw and the present are
	 * skipped. 16ms keeps the worst case from a press to its first frame where
	 * it was, one refresh.
	 *
	 * Why a poll and not a wait on the input devices: input arrives through two
	 * roads, SDL's own events and three raw evdev descriptors, and a held
	 * button produces no event at all while it repeats - in_repeat works from
	 * the clock. A wait that slept until the next event would have stopped key
	 * repeat dead. Polling costs a handful of syscalls a frame, against a full
	 * draw and a present. */
	const Uint32 IDLE_POLL_MS = 16;
	/* A gap this long between two passes means something else had the screen -
	 * a menu, a game, the art scraper - and whatever it drew is not the shelf.
	 * Catching it here covers every one of them without a list to keep. */
	const Uint32 AWAY_MS = 100;
	/* Drawn at least this often regardless. A backstop, not a mechanism: if some
	 * animation is ever added without telling g_redraw_at, it shows as a screen
	 * updating once a second, which is visibly wrong, rather than one that has
	 * silently frozen. */
	const Uint32 HEARTBEAT_MS = 1000;
	/* A still shelf rests longer between passes: until the next thing due -
	 * a frame, a focus frame, the heartbeat - and at most this, where a pass
	 * every IDLE_POLL_MS was 58 wake-ups a second on the home with nothing
	 * happening (2026-11-07). Only while nothing is under way that this loop
	 * follows by looking rather than by being woken: art decoding
	 * (texload_ready), MP4 playing (muse_poll answers a track's end with the
	 * next), File Transfer, a pop-up or the volume bar; a held button draws
	 * every pass anyway. A press ends the rest at once all the same:
	 * plat_input_wait polls the buttons' own devices. Under AWAY_MS, which a
	 * gap between two passes would read as another screen's. */
	const Uint32 IDLE_REST_MS = 80;
	/* How long the shelf sits still, nothing pressed and no art decoding,
	 * before the memory goes down to its slower clock. See the end of the
	 * loop. */
	const Uint32 RAM_IDLE_MS = 2000;
	Uint32 last_pass = 0, last_render = 0, last_busy = plat_now_ms();
	charge_dark dark = { 0 };    /* the screen off on the charger: src/chargedark.h */

	/* At full, whatever a launcher before this one left it at. */
	g_ram_idle = true;
	ram_idle_set(false);
	/* An update the page brought over the cable that is installed now. The
	 * card's empty .media folders go once the shelf is up (after_start_step). */
	up_cable_tidy();

	/* A test of MENU > Scraper's page (scp_test): the page at once */
	if (getenv("ZYPEROS_SCRAPER_TEST")) {
		scraper_page(&a, NULL);
		a.running = false;
	}
	while (a.running) {
		plat_input_poll(&a.in);
		if (a.in.quit_requested || want_quit) break;
		after_start_step(&a, last_render != 0);

		/* The memory back at full before a press is acted on, whatever it
		 * opens: a menu, a game, the next shelf's art. Games set their own
		 * clocks from there (Diatom's power profiles), and a port runs at
		 * full. */
		if (g_ram_idle) {
			for (int b = 0; b < IN_COUNT; b++)
				if (a.in.pressed[b] || a.in.down[b]) {
					ram_idle_set(false);
					last_busy = plat_now_ms();
					break;
				}
		}

		/* Followed here as well as during a game. Diatom is resident and takes
		 * SETAUDIO while idle, so a cable plugged in at the shelf moves the
		 * sound before the next launch rather than at it. */
		aout_apply(false);

		/* An account answer that outlived its game, merged when it lands, and
		 * then whatever was waiting on it sent. */
		sync_poll();

		/* Unlocks sent in the background: apply what the account answered.
		 * And a network that came back with unlocks waiting sends them, from
		 * here only - never during a game (ra_flush_watch). */
		ra_flush_poll();
		ra_flush_watch(plat_now_ms());
		ra_sent_news(ra_flush_take_sent());

		/* What Muse said since the last pass - including END, which is how
		 * the next track of an album starts while nobody is on its screen -
		 * and the covers it was asked for by Muse's shelf, which that shelf
		 * has to be drawn again to ask the worker for. */
		muse_poll();
		if (cover_answers()) redraw_now();

		/* Albums straight in Music/ filed under their artists, from what the
		 * player says their tags are; the shelf read again once anything
		 * moved. See file_album. */
		muse_file_albums();
		if (g_muse_moved && !flat_albums_waiting()) {
			g_muse_moved = false;
			rescan_all(&a);
			redraw_now();
		}
		/* Files that arrived over a File Transfer behind the shelf, once
		 * they have been still a moment, and the shelf after one ended */
		if (xs_shelf_due()) {
			rescan_all(&a);
			redraw_now();
		}

		/* Auto Off is the same line as the power button, on every screen
		 * that draws. Diatom watches it during a game, because it owns the
		 * pad then; everywhere else the launcher can see input itself.
		 *
		 * The charger HOLDS the clock, inside idle_due. It used to gate only
		 * the shot, which let the countdown run to zero while plugged in and
		 * sit there expired - so unplugging powered the device off in the
		 * same instant, however long it had been on the cable. Found on the
		 * device 2026-08-30 at a 30s timeout. Counting the charger as
		 * activity is also what the setting says: unplugging starts a whole
		 * fresh countdown, because until then the timeout was not running. */
		/* On the cable, Auto Off's time turns the screen off instead, and
		 * while it is off every button - power too - only lights it again
		 * (src/chargedark.h). Ahead of the power check for that reason. */
		{
			bool any = false;

			for (int b = 0; b < IN_COUNT && !any; b++)
				any = a.in.pressed[b] || a.in.down[b];
			switch (charge_dark_poll(&dark, a.auto_off, plat_now_ms(), any,
			                         on_charger(), musec_playing())) {
			case CHARGE_GO_DARK:
				saver_stop(&a, "the screen went dark on the cable");
				snap_off(); plat_backlight(false); continue;
			case CHARGE_DARK:
				/* Dark on the cable, and still moving files behind it */
				/* Developer Mode on a release image holds its link and its
				 * SSH with the screen dark: the cable is the very reason it
				 * is dark (Auto Off on the charger), and a computer plugged
				 * in then, or a launcher restarted in the dark, must find
				 * both. The question never comes from here (Developer Mode
				 * asks none). */
				if (!usblink_dev_image() && usblink_developer(db_dev()))
					usblink_follow(db_dev(), plat_now_ms());
				dev_ssh_tick();
				if (g_xs.on) { xs_tick(&a); SDL_Delay(xs_moving() ? 4 : 50); }
				else SDL_Delay(100);
				continue;
			case CHARGE_WAKE:
				plat_backlight(true);
				plat_input_flush();
				memset(&a.in, 0, sizeof a.in);
				redraw_now();
				continue;
			case CHARGE_LIT:     break;
			}
		}

		/* USB charges only until asked: a computer plugged in gets its
		 * question here, once per plug (usblink.h). After the dark screen's
		 * turn, so it waits for the screen to be lit rather than opening
		 * in the dark. */
		if (usblink_follow(db_dev(), plat_now_ms())) {
			saver_stop(&a, "a computer was plugged in");
			usb_ask(&a);
		}
		/* Something on the port wants power: asked too, once per plug;
		 * the replug message is the overlay's, over any screen */
		power_follow();
		if (g_power_ask) {
			unsigned plug = g_power_ask;

			g_power_ask = 0;
			saver_stop(&a, "something was plugged in");
			usb_power_ask(&a, plug);
		}
		/* Zyper's offer of Automatic, due from a USB Accessory answer
		 * here or in MENU > USB Device: here, so never over a game */
		if (g_usb_offer_due && a.running) usb_offer_show(&a);
		/* A phone's USB tethering with an address: File Transfer opens on
		 * it, once a link (phonelink.h) */
		if (phone_link_due(g_xs.on)) {
			saver_stop(&a, "a phone shared its network");
			xfer_screen_way(&a, 2);
		}

		if (a.in.pressed[IN_POWER] || idle_due(&a)) { power_off(&a); break; }

		if (in_repeat(&a.in, IN_VOLUP))    plat_volume_nudge(+1);
		if (in_repeat(&a.in, IN_VOLDN))    plat_volume_nudge(-1);
		if (in_repeat(&a.in, IN_BRIGHTUP)) plat_brightness_nudge(+1);
		if (in_repeat(&a.in, IN_BRIGHTDN)) plat_brightness_nudge(-1);

		/* Auto's clock: 7:00 and 19:00 by the handheld, looked at once a
		 * minute (theme_tick says why not every pass). Here for the shelf;
		 * menu_run_body asks for every menu, every screen that runs a loop
		 * of its own asks through tick_theme - the keyboard's through its
		 * panel, keyboard_panel - a message asks before it is drawn, and
		 * on_game_tick asks for the file while a game runs and leaves
		 * g_theme_late for tick_theme to take. What does not ask is over in
		 * a moment or draws nothing: the launch zoom, the power-off, the
		 * frame a port loads behind, and Muse's lock, whose panel is dark
		 * until the screen it returns to asks for itself. */
		/* px2-open's commands on the shelf, where an open is A on its game
		 * (src/ctl.h), and tick_theme's turn then not a screen's */
		if (ctl_shelf_turn(&a)) {
			if (!a.running) break;
			continue;
		}
		/* The screensaver (src/saver.h): after the delay with nothing
		 * pressed it takes the shelf's place, and goes on taking it until a
		 * button. After idle_due above, which is why it can never keep the
		 * device on past Auto Off; after px2-open's commands, which a
		 * screensaver must not hold up. */
		if (saver_pass(&a)) continue;
		g_ctl_shelf_pass = true;
		tick_theme(&a);
		g_ctl_shelf_pass = false;

		/* Back on the shelf, so any menu MENU closed is closed: put the flag
		 * down before anything here can open another. See menu_leaving. And
		 * no screen is open over the shelf, whatever a way out that ended
		 * the launcher left counted (src/menumotion.h). */
		g_menu_closing = false;
		mm_reset();
		/* On the shelf: menu music may play, back from a game or for the
		 * first time since the launcher started */
		bgm_hold(BGM_HOLD_START | BGM_HOLD_GAME, false);
		/* ...and Zyper says each new song while it is up here */
		song_watch();
		/* ...and "Getting hot", when the chip has passed the trip */
		heat_watch();
		up_news_watch();
		ra_news_watch();

		/* SELECT is Muse, on the shelf and on every screen but a running
		 * game - see muse_open. Before the shelf's own keys, which do not use
		 * it, so every layout gets it from this one line. */
		if (a.in.pressed[IN_SELECT]) { muse_open(&a, NULL, NULL); continue; }

		/* MENU on the shelf is ZyperOS's own menu, the counterpart to the one
		 * MENU opens in a game. It draws over the shelf and returns here.
		 * Both handles its own, because there MENU and B mean two different
		 * menus and the screen cannot say which. */
		if (a.in.pressed[IN_MENU]) { zyperos_menu(&a); continue; }
		/* L2/R2, the menu music's tracks, on either screen */
		song_keys(&a);
		if (a.screen == SCREEN_SYSTEMS) update_systems(&a);
		else update_games(&a);
		if (!a.running) break;
		empty_watch(&a, a.screen == SCREEN_SYSTEMS && no_games(&a));

		{
			Uint32 now = plat_now_ms();
			/* Held counts as touched, not only pressed: a held direction
			 * repeats from the clock and moves the shelf every 90ms. */
			bool touched = false, away = now - last_pass > AWAY_MS;
			int b;

			for (b = 0; b < IN_COUNT && !touched; b++)
				touched = a.in.pressed[b] || a.in.down[b];
			last_pass = now;

			/* texload_ready because finished art is installed while drawing:
			 * a loop that never drew would never find it. */
			bool focus_due = now >= g_focus_at;

			if (touched || away || texload_ready() || now >= g_redraw_at ||
			    now - last_render >= HEARTBEAT_MS || snap_new_picture() ||
			    (focus_due && !focus_ok(&a))) {
				tick_tint(&a);
				render(&a);
				/* With the first frame up, Diatom maps its cores now (its
				 * PREMAP). Mapped the moment it listened, they were read off
				 * the card beside this process's GPU libraries and firmware,
				 * and the first frame came about 0.4 s later on most Brick
				 * boots (measured 2026-10-18). A Diatom that predates it maps
				 * them at once anyway; none at all, and this does nothing. */
				if (!last_render) {
					t_mark("first frame");
					plat_resident_line("PREMAP");
				}
				last_render = plat_now_ms();
			} else if (focus_due) {
				/* Only the home's sprite has moved on: its square alone.
				 * last_render stays, so the heartbeat's whole frame comes. */
				focus_frame(&a);
			} else {
				/* Until a button moves, or IDLE_POLL_MS: a press is drawn
				 * the moment it comes, not up to 16 ms later */
				Uint32 rest = IDLE_POLL_MS;

				if (texload_idle() && !musec_playing() && !g_xs.on && !g_songpop.on &&
				    !g_heatpop.on && plat_osd_until() == UINT32_MAX) {
					Uint32 due = g_redraw_at < g_focus_at ? g_redraw_at : g_focus_at;

					if (last_render + HEARTBEAT_MS < due) due = last_render + HEARTBEAT_MS;
					rest = due > now ? due - now : 1;
					if (rest > IDLE_REST_MS) rest = IDLE_REST_MS;
				}
				plat_input_wait(xs_moving() ? 4 : rest);
			}

			/* The memory down to its slower clock once nothing has been
			 * pressed and no art decoded for RAM_IDLE_MS, and back at full as
			 * soon as art is wanted again; a press raised it at the top. A
			 * still shelf, its heartbeat and a scrolling title need nothing
			 * like the fast clock, and art loads as fast as before, because
			 * it never loads at the slow one. On the Pixel 2 an idle shelf
			 * drew 69 mW more with the DDR at 1056 MHz than at 328
			 * (2026-11-04). */
			/* The song pop-up too: a song changes by itself on a shelf
			 * nobody has touched for minutes, and Zyper's hop is drawn
			 * every frame for its two seconds */
			if (touched || !texload_idle() || g_songpop.on || xs_moving()) last_busy = now;
			ram_idle_set(now - last_busy >= RAM_IDLE_MS);
		}
	}
	/* A launcher stopped while the shelf sat still leaves nothing slow
	 * behind for whatever runs next, and nothing dark. */
	ram_idle_set(false);
	if (dark.dark) plat_backlight(true);
	/* A File Transfer behind the shelf ends with the launcher */
	xs_end();

done:
	ctl_stop();
	free_all_textures(&a);
	/* First, and before IMG_Quit: the worker is inside IMG_Load, and the
	 * views it decodes into are freed just below. The resizer too, for the
	 * same IMG_Quit - it is in IMG_Load and IMG_SavePNG. */
	texload_stop();
	art_shrink_stop();
	/* Told, not waited for - see ra_flush_stop. */
	ra_flush_stop();
	for (int i = 0; i < a.sys.count; i++) {
		free(a.view[i].tex); free(a.view[i].tw); free(a.view[i].th);
		lib_free(&a.view[i].list);
	}
	page_fonts_quit();
	ui_quit();
	IMG_Quit();
	plat_input_quit();
	plat_video_quit();
	uisound_quit();
	bgm_quit();
	SDL_Quit();

	/* Close the databases, which is what checkpoints their WAL and removes
	 * it. Without this the launcher never called sqlite3_close on a normal
	 * exit, so a checkpoint only ever happened when SQLite's own 1000-page
	 * autocheckpoint fired mid-write. zyperos.db had crossed that and held 20
	 * keys; library.db had not, and its .db file was one empty page with the
	 * schema and every row living in library.db-wal. Measured 2026-09-07.
	 *
	 * That is not a growth problem - the WAL sits at its high-water mark
	 * either way. It is a portability one: a .db that means nothing without
	 * its sidecar is lost by any backup, card swap or cleanup that treats a
	 * -wal as scratch, and it reseeds silently rather than failing. Closing
	 * here leaves a complete file behind on every clean exit.
	 *
	 * A battery pull still leaves a WAL, which is what WAL recovery is for. */
	db_shutdown();
	return shot_failed ? 3 : 0;
}
