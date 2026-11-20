/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_MENU_RUNNER_H
#define ZYPEROS_MENU_RUNNER_H

/* The menu runner (DESIGN.md 8): one screen, one loop - polling, power,
 * idle, quit, back, volume, brightness, the cursor and the whole frame -
 * with MENU's side keys and Zyper's answer, the timed and result messages,
 * the on-screen keyboard's prompt, and the Wi-Fi screen. The code is
 * src/menu_runner.c; panel.h declares the runner's types. */
#include "app.h"
#include "cheats.h"
#include "panel.h"
#include "zyperrig.h"

/* Wi-Fi is the largest menu: the switch, WIFI_MAX_NETS networks, a rule and a
 * note. Distinct from MENU_MAX_ROWS, which is the system menu's own count and
 * is much smaller. */
/* And the Video Snaps page, a row a console (sys_menu_snaps_rows). */
#define MENU_RUN_ROWS_ (WIFI_MAX_NETS + 8 > SN_ROWS ? WIFI_MAX_NETS + 8 : SN_ROWS)

/* And the Achievements tab, a row an achievement: a set's whole list */
#define MENU_RUN_ROWS_A (MENU_RUN_ROWS_ > CHV_MAX ? MENU_RUN_ROWS_ : CHV_MAX)

/* And a game's cheats (src/cheats_screen.c): a row a cheat, a seam and a
 * name a group, Turn All Off and the foot */
#define MENU_RUN_ROWS_CHEATS (CHEATS_MAX + 2 * CHEATS_GROUPS + 8)
#define MENU_RUN_ROWS (MENU_RUN_ROWS_A > MENU_RUN_ROWS_CHEATS ? MENU_RUN_ROWS_A : MENU_RUN_ROWS_CHEATS)

/* The shelf under the keyboard and the sign-in messages. kb_prompt's loop is
 * src/keyboard.c's, which knows no app and so no theme_changed; it calls this
 * every pass, and Auto's clock is asked here for it. */
typedef struct {
	app *a;
	int level;
	bool closed, shown;
} keyboard_transition;

/* What main.c uses of it; each is described where it is defined */
extern bool g_ram_idle;
void menu_zyper_answer(zr_rig *z, unsigned now, menu_result r);
kb_pose keyboard_panel(void *ctx, kb_stage stage, bool pressed);

#endif
