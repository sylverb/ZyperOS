/* SPDX-License-Identifier: GPL-3.0-only */
#include "devmode_screen.h"

#include "aout_route.h"
#include "app.h"
#include "devmode.h"
#include "keyhints.h"
#include "mdns.h"
#include "panel.h"
#include "sshlink.h"
#include "uisound.h"
#include "usblink.h"
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

/* ---- Developer Mode's SSH, on a release image (devmode.h) -----------------
 *
 * While Developer Mode is on and a computer is on the cable, SSH runs on the
 * cable's address (sshlink.h), with no question and no page open: the link is
 * kept by usblink (usblink_release_dev), and this starts SSH once per plug,
 * from every screen's loop (tick_theme) and a game's (on_game_tick), so a
 * game does not end it. Two ways in, both the player's own: the keys on the
 * card (the Developer folder, devmode_import_dir) and a password made for
 * this plug, which only the Developer page shows and nothing keeps. It ends
 * when the cable comes out, when Developer Mode is turned off, when File
 * Transfer opens (which runs its own SSH with its PIN, and this one starts
 * again after it), and with the launcher: a launcher that restarts (a
 * deploy) ends the sessions and starts SSH again at once, so a key logs in
 * again a few seconds later. Five wrong passwords lock it until the next
 * plug. */
dv_state g_dv;

static void dev_ssh_forget(void)
{
	memset(g_dv.pw, 0, sizeof g_dv.pw);
	memset(&g_dv, 0, sizeof g_dv);
}

/* The card's Developer folder and the keys the player put in it, joined to
 * the ones ssh-copy-id saves: how many were read */
static int dev_keys_read(void)
{
	char dir[CFG_STR * 2], keydir[CFG_STR * 2];

	snprintf(dir, sizeof dir, "%s/%s", P_CARD, DEVMODE_KEYS_DIR);
	mkdir(dir, 0755);                        /* so the card shows where they go */
	snprintf(keydir, sizeof keydir, "%s/ssh", P_USERDATA);
	return devmode_import_dir(dir, keydir);
}

void dev_ssh_tick(void)
{
	unsigned now = plat_now_ms();
	char keydir[CFG_STR * 2];

	if (usblink_dev_image() || !devmode_unlocked(db_dev()) || g_xs.on) {
		/* Off, or File Transfer is open. Over the cable it started SSH of its
		 * own with its PIN, which took ours over (sshlink_start ends the one
		 * running) and which it polls and locks: left to it. Over Wi-Fi (or
		 * the hotspot) it runs no SSH, and ours must not go on unpolled - a
		 * lockout needs the poll, and a radio ends SSH - so it ends here. It
		 * starts again once File Transfer closes. */
		if (g_dv.run) {
			if (!(g_xs.on && g_xs.cable)) sshlink_stop();
			/* File Transfer runs the name now, wherever it is */
			if (!g_xs.on) mdns_stop();
			dev_ssh_forget();
		}
		return;
	}
	if (g_dv.up) {
		sshlink_poll();
		mdns_poll(now);
	}
	/* The port is read five times a second at most */
	if (g_dv.at && now - g_dv.at < 200u) return;
	g_dv.at = now ? now : 1;
	if (plat_usb_peer() != USB_PEER_COMPUTER) {
		if (g_dv.run) {
			sshlink_stop();
			mdns_stop();
			fprintf(stderr, "devmode: the cable is out, SSH stopped\n");
			dev_ssh_forget();
		}
		return;
	}
	if (g_dv.run || (g_dv.retry && now - g_dv.retry < 15000u)) return;
	if (!plat_cable_link(g_dv.ip, sizeof g_dv.ip) || !devmode_password(g_dv.pw, sizeof g_dv.pw)) {
		g_dv.retry = now ? now : 1;
		memset(g_dv.pw, 0, sizeof g_dv.pw);
		return;
	}
	g_dv.nkeys = dev_keys_read();
	snprintf(keydir, sizeof keydir, "%s/ssh", P_USERDATA);
	g_dv.up = sshlink_start(g_dv.ip, g_dv.pw, keydir);
	fprintf(stderr, "devmode: SSH %s, %d key%s from the card\n",
	        g_dv.up ? "on" : "did not start", g_dv.nkeys > 0 ? g_dv.nkeys : 0,
	        g_dv.nkeys == 1 ? "" : "s");
	if (g_dv.up) {
		g_dv.run = true;
		/* ssh root@zyperos.local, as File Transfer's cable has it */
		mdns_start("usb0", g_dv.ip, false);
	} else {
		/* Wi-Fi up, say: tried again in a while */
		g_dv.retry = now ? now : 1;
		memset(g_dv.pw, 0, sizeof g_dv.pw);
	}
}

/* The page's rows, from what it is told, so the page and its shot (the
 * overflow check draws it in every language) are one list */
int dev_rows(menu_row rows[6], char *cmd, size_t cn, char *keys, size_t kn, bool up,
             const char *ip, const char *pw, int nkeys, const char *ssh)
{
	int n = 0;

	if (nkeys > 0) snprintf(keys, kn, TN(S_DEVMODE_KEYS_COUNT, nkeys), nkeys);
	else snprintf(keys, kn, "%s", T(S_DEVMODE_KEYS_NONE));
	snprintf(cmd, cn, "ssh root@%s", ip);
	if (up) {
		rows[n++] = (menu_row){ T(S_DEVMODE_COMMAND), cmd, false };
		rows[n++] = (menu_row){ T(S_PANEL_PASSWORD), pw, false };
	}
	rows[n++] = (menu_row){ T(S_DEVMODE_KEY_FOLDER), DEVMODE_KEYS_DIR, false };
	rows[n++] = (menu_row){ T(S_DEVMODE_KEYS), keys, false };
	rows[n++] = (menu_row){ "SSH", ssh, false };
	rows[n++] = MENU_NOTE(keyhints_legend((key_hint[]){ { "A", T(S_KEYHINT_TURN_OFF_MODE) }, { "B", T(S_KEYHINT_CLOSE) } }, 2));
	return n;
}

/* MENU's Developer Mode row opens this page: what to type, the password
 * (made when SSH started for this plug), where the keys go and how many were
 * read, and SSH's own status. A turns Developer Mode off (and ends SSH), B
 * closes the page and leaves everything running. */
void dev_screen(app *a)
{
	char cmd[96], keys[48];
	menu_row rows[6];
	bool done = false;
	int n, lv, width = 0;
	float k;

	if (!g_dv.up) g_dv.nkeys = dev_keys_read();
	lv = panel_open();
	while (!done && a->running && !want_quit) {
		plat_input_poll(&a->in);
		muse_poll();
		aout_apply(false);
		tick_theme(a);
		if (a->in.quit_requested) { a->running = false; break; }
		if (a->in.pressed[IN_POWER] || idle_due(a)) {
			panel_close(a, lv, true);
			power_off(a);
			return;
		}
		if (menu_leaving(a)) done = true;
		if (a->in.pressed[IN_ACCEPT]) {
			devmode_set(db_dev(), false);
			usblink_release_dev(false);
			fprintf(stderr, "devmode: off\n");
			dev_ssh_tick();                      /* SSH ends now */
			uisound_play(DS_SOUND_ACCEPT);
			done = true;
		}
		n = dev_rows(rows, cmd, sizeof cmd, keys, sizeof keys, g_dv.up, g_dv.ip, g_dv.pw,
		             g_dv.nkeys, sshlink_status());
		{
			int m = menu_measure(rows, n, T(S_XFER_DEV_HEADING));

			if (!width) width = menu_std_width(a);
			if (m > width) width = m;
		}
		if (!panel_frame(a, lv, &k)) continue;
		tick_tint(a);
		draw_shelf(a);
		ui_scrim_k(a->r, 0, k);
		menu_draw_caption(a, T(S_XFER_DEV_HEADING), rows, n, -1, width, MENU_ZYPEROS, NULL,
		                  false, 0, NULL);
		plat_draw_osd(a->r);
		plat_present();
		ui_frame_rest(8);
	}
	panel_close(a, lv, !a->running);
	plat_input_flush();
	memset(&a->in, 0, sizeof a->in);
}
