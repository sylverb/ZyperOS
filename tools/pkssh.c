/* SPDX-License-Identifier: GPL-3.0-only */
/* See pkssh.h. */
#include "pkssh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mdns.h"
#include "sshlink.h"

#define CABLE_IF   "usb0"
#define LOOK_MS    200        /* the link and the radio, as the launcher's tick */
#define READ_MS    2000       /* the two settings, which only the launcher's menus change */
#define RETRY_MS   15000      /* a start that failed, tried again */

static sqlite3     *g_db;
static char         g_keydir[512];
static sshlink_mode g_mode;   /* what this started and believes runs */
static bool         g_dev, g_wifi, g_gave_up;
static long         g_read_at = -1, g_look_at = -1, g_retry_at = -1;
static char         g_ip[64], g_ifn[16];

/* A setting as the launcher's db_get_int has it: 1 is on, anything else off */
static bool setting_on(const char *key)
{
	sqlite3_stmt *st = NULL;
	bool on = false;

	if (!g_db || sqlite3_prepare_v2(g_db, "SELECT value FROM settings WHERE key=?;", -1, &st, NULL) != SQLITE_OK)
		return false;
	sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
	if (sqlite3_step(st) == SQLITE_ROW) {
		const unsigned char *v = sqlite3_column_text(st, 0);

		on = v && !strcmp((const char *)v, "1");
	}
	sqlite3_finalize(st);
	return on;
}

void pkssh_init(sqlite3 *db, const char *db_path)
{
	const char *slash = db_path ? strrchr(db_path, '/') : NULL;

	g_db = db;
	if (slash) snprintf(g_keydir, sizeof g_keydir, "%.*s/ssh", (int)(slash - db_path), db_path);
	/* What the launcher's SSH left: its namespace, by RUN_NS */
	sshlink_stop();
	g_mode = SSHLINK_OFF;
}

static void end(const char *why)
{
	if (g_mode != SSHLINK_OFF && sshlink_running() != SSHLINK_OFF) sshlink_stop();
	mdns_stop();
	if (why) fprintf(stderr, "portkeys: %s\n", why);
	g_mode = SSHLINK_OFF;
	g_ip[0] = g_ifn[0] = '\0';
}

int pkssh_tick(long now)
{
	sshlink_facts f = { 0 };
	sshlink_mode want;
	char ip[64] = "", ifn[16] = "", wip[64] = "";

	if (!g_keydir[0]) return -1;
	if (g_mode != SSHLINK_OFF) {
		sshlink_poll();
		mdns_poll((unsigned)now);
	}
	if (g_look_at >= 0 && now - g_look_at < LOOK_MS) return (int)(LOOK_MS - (now - g_look_at));
	g_look_at = now;
	if (g_read_at < 0 || now - g_read_at >= READ_MS) {
		g_read_at = now;
		g_dev = setting_on("devmode");
		g_wifi = g_dev && setting_on("devmode.wifi");
	}
	f.cable = sshlink_if_addr(CABLE_IF, ip, sizeof ip);
	f.over_wifi = g_wifi;
	f.radio = sshlink_radio(ifn, sizeof ifn, wip, g_wifi ? sizeof wip : 0);
	want = sshlink_want_port(&f, g_dev);
	if (g_mode != SSHLINK_OFF && (want != g_mode || sshlink_running() != g_mode)) {
		bool by_itself = sshlink_running() != g_mode;

		end(by_itself ? "SSH ended by itself" : want == SSHLINK_OFF ? "SSH stopped (Developer Mode, the link or the radio)"
		                                                            : "SSH changes its way in");
		if (by_itself) g_retry_at = now;
	}
	/* The name follows the address, as under the launcher */
	if (g_mode == SSHLINK_NET && strcmp(wip, g_ip)) {
		snprintf(g_ip, sizeof g_ip, "%s", wip);
		if (wip[0]) mdns_start(ifn, wip, true);
		else mdns_stop();
	}
	if (g_mode != SSHLINK_OFF || want == SSHLINK_OFF || g_gave_up) return LOOK_MS;
	if (g_retry_at >= 0 && now - g_retry_at < RETRY_MS) return LOOK_MS;
	if (want == SSHLINK_NET ? sshlink_start_net(g_keydir) : sshlink_start_cable_keys(ip, g_keydir)) {
		g_mode = want;
		g_retry_at = -1;
		fprintf(stderr, "portkeys: SSH on over a port, keys only (%s)\n",
		        want == SSHLINK_NET ? "every address, over Wi-Fi" : ip);
		if (want == SSHLINK_NET) {
			snprintf(g_ip, sizeof g_ip, "%s", wip);
			snprintf(g_ifn, sizeof g_ifn, "%s", ifn);
			if (wip[0]) mdns_start(ifn, wip, true);
		} else {
			snprintf(g_ip, sizeof g_ip, "%s", ip);
			mdns_start(CABLE_IF, ip, false);
		}
	} else {
		/* A development image's dropbear has port 22 for the whole port */
		if (sshlink_dev_image()) g_gave_up = true;
		g_retry_at = now;
	}
	return LOOK_MS;
}

void pkssh_stop(void)
{
	end(g_mode != SSHLINK_OFF ? "SSH stopped, the port ended" : NULL);
	/* and what a start that failed may have left */
	sshlink_stop();
}
