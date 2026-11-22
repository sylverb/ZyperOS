/* SPDX-License-Identifier: GPL-3.0-only */
/* The Link Cable, the launcher's side. See link.h. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hotspot.h"
#include "link.h"

/* A hello again while the host has not answered, and how long it has */
#define HELLO_MS   500
#define ANSWER_MS  10000
/* Joining the host's network and taking an address: the supplicant's own
 * wait is twenty seconds (wifi_connect), and the address comes after */
#define JOIN_MS    30000
/* The list is scanned again this often while it is looked at: a host
 * started after the Join page opened shows without a key */
#define RESCAN_MS  3000

bool link_offered(const char *core, bool device)
{
	return device && core && !strcmp(core, LINK_CORE);
}

/* ---- the pure parts ------------------------------------------------------- */

void link_tail(const char *mac, char out[5])
{
	/* "aa:bb:cc:dd:ee:ff": the last two bytes are at 12 and 15 */
	if (mac && strlen(mac) >= 17 && mac[14] == ':') {
		const char *p[4] = { mac + 12, mac + 13, mac + 15, mac + 16 };
		int k;

		for (k = 0; k < 4 && isxdigit((unsigned char)*p[k]); k++)
			out[k] = (char)toupper((unsigned char)*p[k]);
		if (k == 4) { out[4] = '\0'; return; }
	}
	snprintf(out, 5, "0000");
}

void link_ssid(const char *tail, char *out, unsigned n)
{
	snprintf(out, n, "%s%s", LINK_SSID_PREFIX, tail ? tail : "");
}

void link_name(const char *tail, char *out, unsigned n)
{
	snprintf(out, n, "%s %s", LINK_MODEL, tail ? tail : "");
}

bool link_name_of_ssid(const char *ssid, char *out, unsigned n)
{
	const size_t pre = strlen(LINK_SSID_PREFIX);
	size_t k;

	if (!ssid || strncmp(ssid, LINK_SSID_PREFIX, pre) || strlen(ssid) != pre + 4) return false;
	for (k = pre; k < pre + 4; k++)
		if (!isxdigit((unsigned char)ssid[k]) || islower((unsigned char)ssid[k])) return false;
	link_name(ssid + pre, out, n);
	return true;
}

void link_password(const char *ssid, char *out, unsigned n)
{
	/* File Transfer's letters (src/captive.c): no 0 o 1 l i */
	static const char ALPHA[] = "abcdefghjkmnpqrstuvwxyz23456789";
	/* FNV-1a over the name and a word of its own, then its bits eight
	 * characters long: the same on both handhelds, and nothing to type */
	unsigned long long h = 1469598103934665603ull;
	const char *salt = "zyperos-link-cable", *p;
	char pw[10];
	int k;

	for (p = salt; *p; p++) h = (h ^ (unsigned char)*p) * 1099511628211ull;
	for (p = ssid ? ssid : ""; *p; p++) h = (h ^ (unsigned char)*p) * 1099511628211ull;
	for (k = 0; k < 9; k++) {
		if (k == 4) { pw[k] = '-'; continue; }
		pw[k] = ALPHA[h % (sizeof ALPHA - 1)];
		h /= sizeof ALPHA - 1;
		h ^= h << 13;
		h *= 1099511628211ull;
	}
	pw[9] = '\0';
	snprintf(out, n, "%s", pw);
}

bool link_ip_digits(const char *addr, char digits[12])
{
	const char *p = addr;
	int o;

	if (!p) return false;
	for (o = 0; o < 4; o++) {
		char *end;
		long v;

		if (!isdigit((unsigned char)*p)) return false;
		v = strtol(p, &end, 10);
		if (end - p > 3 || v < 0 || v > 255) return false;
		digits[o * 3]     = (char)('0' + v / 100);
		digits[o * 3 + 1] = (char)('0' + v / 10 % 10);
		digits[o * 3 + 2] = (char)('0' + v % 10);
		p = end;
		if (o < 3) {
			if (*p != '.') return false;
			p++;
		}
	}
	return *p == '\0';
}

bool link_same_net(const char *a, const char *b)
{
	char da[12], db[12];

	return link_ip_digits(a, da) && link_ip_digits(b, db) && !memcmp(da, db, 9);
}

int link_opt_lines(link_role role, const char *addr, char lines[][LINK_LINE_MAX], int max)
{
	static const char *const MODE[] = { "Not Connected", "Network Server", "Network Client" };
	char d[12];
	int n = 0, k;

	if (max < 1) return 0;
	if (role != LINK_ROLE_OFF && (max < 14 || !link_ip_digits(addr, d))) return 0;
	/* The port and the address first and the mode last: the core reads
	 * them all at once on its next frame (check_variables), so the order
	 * is only for a person reading the log */
	if (role != LINK_ROLE_OFF) {
		snprintf(lines[n++], LINK_LINE_MAX, "SETOPT\tkey=gambatte_gb_link_network_port\tvalue=%s",
		         LINK_GAME_PORT);
		for (k = 0; k < 12; k++)
			snprintf(lines[n++], LINK_LINE_MAX,
			         "SETOPT\tkey=gambatte_gb_link_network_server_ip_%d\tvalue=%c", k + 1, d[k]);
	}
	snprintf(lines[n++], LINK_LINE_MAX, "SETOPT\tkey=gambatte_gb_link_mode\tvalue=%s", MODE[role]);
	return n;
}

static const char *const KIND[] = { "", "hello", "welcome", "busy", "beat", "bye" };

/* `s` with its tabs and line ends as spaces, into `out` */
static void flat(const char *s, char *out, unsigned n)
{
	unsigned i;

	snprintf(out, n, "%s", s ? s : "");
	for (i = 0; out[i]; i++)
		if (out[i] == '\t' || out[i] == '\n' || out[i] == '\r') out[i] = ' ';
}

unsigned link_wire(link_msg kind, const char *name, const char *game, char *out, unsigned n)
{
	char fn[LINK_NAME_MAX], fg[LINK_GAME_MAX];
	int len;

	if (kind <= LINK_MSG_NONE || kind > LINK_MSG_BYE) return 0;
	flat(name, fn, sizeof fn);
	flat(game, fg, sizeof fg);
	len = snprintf(out, n, "ZLINK1\t%s\t%s\t%s", KIND[kind], fn, fg);
	return len > 0 && (unsigned)len < n ? (unsigned)len : 0;
}

link_msg link_wire_read(const char *buf, unsigned len, char *name, unsigned namen,
                        char *game, unsigned gamen)
{
	char line[256], *f[4], *p;
	int nf = 0, k;

	if (!buf || len < 7 || len >= sizeof line) return LINK_MSG_NONE;
	memcpy(line, buf, len);
	line[len] = '\0';
	if (strlen(line) != len) return LINK_MSG_NONE;    /* a NUL inside: not ours */
	for (p = line; nf < 4; ) {
		f[nf++] = p;
		p = strchr(p, '\t');
		if (!p) break;
		*p++ = '\0';
	}
	if (nf < 3 || strcmp(f[0], "ZLINK1")) return LINK_MSG_NONE;
	for (k = LINK_MSG_HELLO; k <= LINK_MSG_BYE; k++)
		if (!strcmp(f[1], KIND[k])) break;
	if (k > LINK_MSG_BYE) return LINK_MSG_NONE;
	if (name) snprintf(name, namen, "%s", f[2]);
	if (game) snprintf(game, gamen, "%s", nf > 3 ? f[3] : "");
	return (link_msg)k;
}

/* ---- on and off ----------------------------------------------------------- */

static const link_ops *g_ops;

static struct {
	link_state   st;
	link_failure why;
	link_role    role;
	char tail[5], self[LINK_NAME_MAX], ssid[LINK_SSID_MAX], pass[LINK_PASS_MAX];
	char game[LINK_GAME_MAX];
	char peer[LINK_NAME_MAX], peer_addr[LINK_ADDR_MAX];
	char hosts[LINK_HOSTS_MAX][LINK_NAME_MAX], host_ssid[LINK_HOSTS_MAX][LINK_SSID_MAX];
	int  nhosts;
	bool scanned, scanning;
	char joining[LINK_NAME_MAX];
	bool hot;        /* the hotspot was asked: hotspot_stop is owed */
	bool joined;     /* a network was joined: join_end is owed */
	bool looked;     /* the radio scanned for hosts: join_end is owed too */
	bool opts;       /* the core's link was turned on: off is owed */
	bool over;       /* the game is over (link_game_over) */
	int  sock;
	unsigned t0, sent_at, heard_at, scan_at;
} g = { .sock = -1 };

void link_use(const link_ops *ops, const char *mac)
{
	if (ops) g_ops = ops;
	link_tail(mac, g.tail);
	link_name(g.tail, g.self, sizeof g.self);
	link_ssid(g.tail, g.ssid, sizeof g.ssid);
}

static unsigned now(void)
{
	unsigned t = g_ops->now();

	return t ? t : 1;
}

static void fail(link_failure why, const char *what)
{
	fprintf(stderr, "link: %s\n", what);
	g.why = why;
	g.st = LINK_FAILED;
}

/* The core's link for `role` (the host's address either way: the server
 * listens on every address, the client connects to it) */
static bool core_link(link_role role)
{
	char lines[16][LINK_LINE_MAX];
	int n = link_opt_lines(role, LINK_HOST_ADDR, lines, 16), i;

	if (!n) return false;
	for (i = 0; i < n; i++)
		if (!g_ops->resident(lines[i])) {
			fprintf(stderr, "link: Diatom did not take %s\n", lines[i] + 7);
			return false;
		}
	fprintf(stderr, "link: the core's link is %s\n",
	        role == LINK_ROLE_SERVER ? "the server" : role == LINK_ROLE_CLIENT ? "the client" : "off");
	return true;
}

static void say(link_msg kind, const char *addr, int port)
{
	char buf[192];
	unsigned len = link_wire(kind, g.self, g.game, buf, sizeof buf);

	if (len && g.sock >= 0) g_ops->udp_send(g.sock, addr, port, buf, len);
}

/* The other one went: the core's link off at once (it would wait ten
 * seconds for an answer at every transfer), said once, and the rest for
 * link_stop */
static void lost(link_news why)
{
	fprintf(stderr, "link: %s %s\n", g.peer, why == LINK_NEWS_LEFT ? "left" : "lost");
	if (g.opts && !g.over) {
		core_link(LINK_ROLE_OFF);
		g.opts = false;
	}
	if (g.sock >= 0) {
		g_ops->udp_close(g.sock);
		g.sock = -1;
	}
	g.st = LINK_LOST;
	if (!g.over && g_ops->notice) g_ops->notice(why, g.peer);
}

static void linked(void)
{
	g.st = LINK_LINKED;
	g.heard_at = g.sent_at = now();
	fprintf(stderr, "link: linked with %s at %s as the %s\n", g.peer, g.peer_addr,
	        g.role == LINK_ROLE_SERVER ? "server" : "client");
	if (g_ops->notice) g_ops->notice(LINK_NEWS_LINKED, g.peer);
}

bool link_host(const char *game)
{
	if (!g_ops) return false;
	link_stop();
	snprintf(g.game, sizeof g.game, "%s", game ? game : "");
	link_password(g.ssid, g.pass, sizeof g.pass);
	g.role = LINK_ROLE_SERVER;
	g.t0 = now();
	g.hot = true;
	if (!g_ops->hotspot_start(g.ssid, g.pass)) {
		fail(LINK_FAIL_NETWORK, "the hotspot did not start");
		return false;
	}
	g.st = LINK_HOST_STARTING;
	fprintf(stderr, "link: hosting as %s\n", g.ssid);
	return true;
}

bool link_join_look(const char *game)
{
	if (!g_ops) return false;
	link_stop();
	snprintf(g.game, sizeof g.game, "%s", game ? game : "");
	g.role = LINK_ROLE_CLIENT;
	g.nhosts = 0;
	g.scanned = false;
	if (!g_ops->scan_start()) {
		fail(LINK_FAIL_NETWORK, "no scan would start");
		return false;
	}
	g.scanning = true;
	g.looked = true;
	g.scan_at = now();
	g.st = LINK_JOIN_LOOKING;
	return true;
}

bool link_join_pick(int i)
{
	char pass[LINK_PASS_MAX];

	if (!g_ops || g.st != LINK_JOIN_LOOKING || i < 0 || i >= g.nhosts) return false;
	snprintf(g.joining, sizeof g.joining, "%s", g.hosts[i]);
	snprintf(g.peer, sizeof g.peer, "%s", g.hosts[i]);
	link_password(g.host_ssid[i], pass, sizeof pass);
	g.scanning = false;
	g.joined = true;
	if (!g_ops->join_start(g.host_ssid[i], pass)) {
		fail(LINK_FAIL_JOIN, "the host's network would not join");
		return false;
	}
	fprintf(stderr, "link: joining %s\n", g.host_ssid[i]);
	g.t0 = now();
	g.st = LINK_JOIN_JOINING;
	return true;
}

/* The scan's names that are a link's, as the list */
static void take_hosts(void)
{
	char ssids[24][LINK_SSID_MAX];
	int n = g_ops->scan_take(ssids, 24), i, k;

	g.nhosts = 0;
	for (i = 0; i < n && g.nhosts < LINK_HOSTS_MAX; i++) {
		char name[LINK_NAME_MAX];

		/* Our own name is no host to join */
		if (!link_name_of_ssid(ssids[i], name, sizeof name) || !strcmp(ssids[i], g.ssid)) continue;
		for (k = 0; k < g.nhosts && strcmp(g.hosts[k], name); k++) { }
		if (k < g.nhosts) continue;
		snprintf(g.hosts[g.nhosts], sizeof g.hosts[0], "%s", name);
		memcpy(g.host_ssid[g.nhosts], ssids[i], LINK_SSID_MAX);
		g.host_ssid[g.nhosts][LINK_SSID_MAX - 1] = '\0';
		g.nhosts++;
	}
	g.scanned = true;
}

/* Every datagram waiting, at most a handful a tick */
static void hear(unsigned t)
{
	char buf[256], from[LINK_ADDR_MAX], name[LINK_NAME_MAX], game[LINK_GAME_MAX];
	int k, len;

	for (k = 0; k < 16 && g.sock >= 0; k++) {
		link_msg m;

		len = g_ops->udp_recv(g.sock, buf, sizeof buf, from, sizeof from);
		if (len <= 0) break;
		m = link_wire_read(buf, (unsigned)len, name, sizeof name, game, sizeof game);
		if (m == LINK_MSG_NONE) continue;
		if (g.st == LINK_HOST_WAITING && m == LINK_MSG_HELLO) {
			snprintf(g.peer, sizeof g.peer, "%s", name);
			snprintf(g.peer_addr, sizeof g.peer_addr, "%s", from);
			fprintf(stderr, "link: %s says hello from %s, playing %s\n", name, from, game);
			if (!core_link(LINK_ROLE_SERVER)) {
				fail(LINK_FAIL_GAME, "the game did not take the link");
				return;
			}
			g.opts = true;
			say(LINK_MSG_WELCOME, from, LINK_BEAT_PORT);
			linked();
			continue;
		}
		if (g.st == LINK_LINKED && g.role == LINK_ROLE_SERVER && m == LINK_MSG_HELLO) {
			/* The one linked, again (its welcome was lost): welcome again.
			 * Anyone else: this host is taken. */
			if (!strcmp(from, g.peer_addr)) {
				say(LINK_MSG_WELCOME, from, LINK_BEAT_PORT);
				g.heard_at = t;
			} else {
				say(LINK_MSG_BUSY, from, LINK_BEAT_PORT);
			}
			continue;
		}
		if (g.st == LINK_JOIN_ASKING && (m == LINK_MSG_WELCOME || m == LINK_MSG_BUSY)) {
			if (m == LINK_MSG_BUSY) {
				fail(LINK_FAIL_BUSY, "the host is linked with another");
				return;
			}
			snprintf(g.peer, sizeof g.peer, "%s", name);
			snprintf(g.peer_addr, sizeof g.peer_addr, "%s", from);
			fprintf(stderr, "link: %s welcomes us, playing %s\n", name, game);
			if (!core_link(LINK_ROLE_CLIENT)) {
				fail(LINK_FAIL_GAME, "the game did not take the link");
				return;
			}
			g.opts = true;
			linked();
			continue;
		}
		if (g.st == LINK_LINKED && !strcmp(from, g.peer_addr)) {
			if (m == LINK_MSG_BYE) { lost(LINK_NEWS_LEFT); return; }
			g.heard_at = t;
		}
	}
}

void link_tick(unsigned now_ms)
{
	unsigned t = now_ms ? now_ms : 1;
	char addr[LINK_ADDR_MAX];
	int r;

	if (!g_ops || g.over) return;
	switch (g.st) {
	case LINK_HOST_STARTING:
		r = g_ops->hotspot_poll();
		if (r == HOTSPOT_STARTING) break;
		if (r != HOTSPOT_UP) { fail(LINK_FAIL_NETWORK, "the hotspot did not come up"); break; }
		g.sock = g_ops->udp_open(LINK_HOST_ADDR, LINK_BEAT_PORT);
		if (g.sock < 0) { fail(LINK_FAIL_NETWORK, "no socket for the beat"); break; }
		fprintf(stderr, "link: %s is up, waiting\n", g.ssid);
		g.st = LINK_HOST_WAITING;
		break;
	case LINK_HOST_WAITING:
		if (g_ops->hotspot_poll() != HOTSPOT_UP) { fail(LINK_FAIL_NETWORK, "the hotspot went away"); break; }
		hear(t);
		break;
	case LINK_JOIN_LOOKING:
		if (g.scanning) {
			r = g_ops->scan_poll();
			if (r == 1) { take_hosts(); g.scanning = false; g.scan_at = t; }
			else if (r < 0) { g.scanning = false; g.scan_at = t; g.scanned = true; }
		} else if (t - g.scan_at >= RESCAN_MS) {
			g.scanning = g_ops->scan_start();
			g.scan_at = t;
		}
		break;
	case LINK_JOIN_JOINING:
		r = g_ops->join_poll(addr, sizeof addr);
		/* An address on the host's network only: the one the last network
		 * gave can still be on the adapter while the new lease comes */
		if (r > 0 && !link_same_net(addr, LINK_HOST_ADDR)) r = 0;
		if (r > 0) {
			g.sock = g_ops->udp_open(NULL, 0);
			if (g.sock < 0) { fail(LINK_FAIL_NETWORK, "no socket for the beat"); break; }
			fprintf(stderr, "link: on %s's network as %s\n", g.joining, addr);
			g.t0 = t;
			g.st = LINK_JOIN_ASKING;
			/* The first hello at once */
			say(LINK_MSG_HELLO, LINK_HOST_ADDR, LINK_BEAT_PORT);
			g.sent_at = t;
		} else if (r < 0 || t - g.t0 >= JOIN_MS) {
			fail(LINK_FAIL_JOIN, "the host's network did not join");
		}
		break;
	case LINK_JOIN_ASKING:
		if (t - g.sent_at >= HELLO_MS) {
			say(LINK_MSG_HELLO, LINK_HOST_ADDR, LINK_BEAT_PORT);
			g.sent_at = t;
		}
		hear(t);
		if (g.st == LINK_JOIN_ASKING && t - g.t0 >= ANSWER_MS)
			fail(LINK_FAIL_ANSWER, "the host did not answer");
		break;
	case LINK_LINKED:
		if (g.role == LINK_ROLE_SERVER && g_ops->hotspot_poll() != HOTSPOT_UP) {
			lost(LINK_NEWS_LOST);
			break;
		}
		hear(t);
		if (g.st != LINK_LINKED) break;
		if (t - g.sent_at >= LINK_BEAT_MS) {
			say(LINK_MSG_BEAT, g.peer_addr, LINK_BEAT_PORT);
			g.sent_at = t;
		}
		if (t - g.heard_at >= LINK_LOST_MS) lost(LINK_NEWS_LOST);
		break;
	default:
		break;
	}
}

void link_game_over(void)
{
	if (g.st != LINK_OFF) g.over = true;
}

void link_stop(void)
{
	if (!g_ops) return;
	/* Twice: a datagram can be lost, and the other one would wait
	 * LINK_LOST_MS for a beat to say what this says at once */
	if (g.st == LINK_LINKED && g.sock >= 0) {
		say(LINK_MSG_BYE, g.peer_addr, LINK_BEAT_PORT);
		say(LINK_MSG_BYE, g.peer_addr, LINK_BEAT_PORT);
	}
	if (g.opts && !g.over) core_link(LINK_ROLE_OFF);
	if (g.sock >= 0) g_ops->udp_close(g.sock);
	if (g.hot) g_ops->hotspot_stop();
	if (g.joined || g.looked) g_ops->join_end();
	if (g.st != LINK_OFF) fprintf(stderr, "link: off\n");
	g.st = LINK_OFF;
	g.why = LINK_FAIL_NONE;
	g.role = LINK_ROLE_OFF;
	g.sock = -1;
	g.hot = g.joined = g.looked = g.opts = g.over = g.scanning = g.scanned = false;
	g.nhosts = 0;
	g.peer[0] = g.peer_addr[0] = g.joining[0] = '\0';
	memset(g.pass, 0, sizeof g.pass);
}

link_state   link_state_now(void)   { return g.st; }
link_failure link_failure_now(void) { return g.st == LINK_FAILED ? g.why : LINK_FAIL_NONE; }
link_role    link_role_now(void)    { return g.role; }
const char  *link_self_name(void)   { return g.self; }
const char  *link_peer_name(void)   { return g.peer; }
int          link_host_count(void)  { return g.nhosts; }
const char  *link_host_name(int i)  { return i >= 0 && i < g.nhosts ? g.hosts[i] : ""; }
bool         link_scanned(void)     { return g.scanned; }
const char  *link_joining_name(void) { return g.joining; }
bool         link_owes_wifi(void)   { return g.hot || g.joined; }
