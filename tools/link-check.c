/* SPDX-License-Identifier: GPL-3.0-only */
/* src/link.c: the Link Cable's names, its core options and its word between
 * the two launchers, and its start and stop on each side, on fakes. Every
 * process, network and protocol action link.c takes is one of link_ops, so
 * the fakes write each down and the checks read them back: what goes to
 * Diatom, the hotspot, the scan and the join, and the datagrams. Then
 * src/linknet.c's real socket, on the loopback. No SDL, no Diatom. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../src/hotspot.h"
#include "../src/link.h"
#include "../src/linknet.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

/* ---- the fakes ------------------------------------------------------------ */

#define LOG_MAX 96
static char     g_log[LOG_MAX][320];
static int      g_nlog;
static unsigned g_clock = 1000;
static int      g_hs = HOTSPOT_STARTING;
static bool     g_resident_ok = true, g_hotspot_ok = true, g_scan_ok = true;
static int      g_scan_state;                 /* what scan_poll says */
static const char *g_scan[8];                 /* what scan_take gives */
static int      g_nscan;
static int      g_join;                       /* what join_poll says */
static int      g_sock_next = 3;
/* The datagrams waiting for link.c, oldest first */
static struct { char buf[256], from[16]; } g_in[8];
static int      g_nin;
/* The last one link.c sent, and to whom */
static char     g_out[256], g_out_to[32];
static int      g_nout;

static void say(const char *fmt, ...)
{
	va_list ap;

	if (g_nlog >= LOG_MAX) return;
	va_start(ap, fmt);
	vsnprintf(g_log[g_nlog++], sizeof g_log[0], fmt, ap);
	va_end(ap);
}

static void reset_log(void) { g_nlog = 0; g_nout = 0; g_out[0] = '\0'; }

/* How many lines of the log start with `prefix` */
static int logged(const char *prefix)
{
	int i, n = 0;

	for (i = 0; i < g_nlog; i++)
		if (!strncmp(g_log[i], prefix, strlen(prefix))) n++;
	return n;
}

/* The index of the first line that is `line`, -1 for none */
static int at(const char *line)
{
	int i;

	for (i = 0; i < g_nlog; i++)
		if (!strcmp(g_log[i], line)) return i;
	return -1;
}

static bool f_resident(const char *line) { say("resident %s", line); return g_resident_ok; }
static bool f_hs_start(const char *ssid, const char *pass) { say("hotspot %s %s", ssid, pass); return g_hotspot_ok; }
static int  f_hs_poll(void) { return g_hs; }
static void f_hs_stop(void) { say("hotspot stop"); }
static bool f_scan_start(void) { say("scan"); return g_scan_ok; }
static int  f_scan_poll(void) { return g_scan_state; }
static int  f_scan_take(char ssids[][LINK_SSID_MAX], int max)
{
	int i;

	for (i = 0; i < g_nscan && i < max; i++) snprintf(ssids[i], LINK_SSID_MAX, "%s", g_scan[i]);
	return i;
}
static bool f_join_start(const char *ssid, const char *pass) { say("join %s %s", ssid, pass); return true; }
static const char *g_join_addr = "10.42.1.23";
static int  f_join_poll(char *addr, unsigned n) { if (g_join > 0) snprintf(addr, n, "%s", g_join_addr); return g_join; }
static void f_join_end(void) { say("join end"); }
static int  f_udp_open(const char *addr, int port) { say("udp %s %d", addr ? addr : "*", port); return g_sock_next; }
static bool f_udp_send(int s, const char *addr, int port, const char *buf, unsigned len)
{
	(void)s;
	snprintf(g_out, sizeof g_out, "%.*s", (int)len, buf);
	snprintf(g_out_to, sizeof g_out_to, "%s:%d", addr, port);
	g_nout++;
	say("send %s %s", g_out_to, g_out);
	return true;
}
static int f_udp_recv(int s, char *buf, unsigned n, char *from, unsigned fromn)
{
	int len;

	(void)s;
	if (!g_nin) return 0;
	len = snprintf(buf, n, "%s", g_in[0].buf);
	snprintf(from, fromn, "%s", g_in[0].from);
	memmove(&g_in[0], &g_in[1], sizeof g_in[0] * (size_t)(--g_nin));
	return len;
}
static void f_udp_close(int s) { say("udp close %d", s); }
static unsigned f_now(void) { return g_clock; }
static void f_notice(link_news what, const char *peer)
{
	say("notice %s %s", what == LINK_NEWS_LINKED ? "linked" : what == LINK_NEWS_LOST ? "lost" : "left", peer);
}

static const link_ops FAKE = {
	f_resident, f_hs_start, f_hs_poll, f_hs_stop,
	f_scan_start, f_scan_poll, f_scan_take,
	f_join_start, f_join_poll, f_join_end,
	f_udp_open, f_udp_send, f_udp_recv, f_udp_close,
	f_now, f_notice,
};

/* A datagram from `from` for link.c's next tick */
static void arrive(link_msg kind, const char *name, const char *game, const char *from)
{
	link_wire(kind, name, game, g_in[g_nin].buf, sizeof g_in[0].buf);
	snprintf(g_in[g_nin].from, sizeof g_in[0].from, "%s", from);
	g_nin++;
}

/* Time on, `ms` in ticks of 100 */
static void run(unsigned ms)
{
	unsigned t;

	for (t = 0; t < ms; t += 100) {
		g_clock += 100;
		link_tick(g_clock);
	}
}

/* ---- the pure parts -------------------------------------------------------- */

static void offered(void)
{
	CHECK(link_offered("gambatte", true));
	CHECK(!link_offered("gambatte", false));
	CHECK(!link_offered("mgba", true));
	CHECK(!link_offered(NULL, true));
}

static void names(void)
{
	char tail[5], s[40], name[32], p1[16], p2[16];

	link_tail("00:c0:ca:b1:7f:2a", tail);
	CHECK(!strcmp(tail, "7F2A"));
	link_tail("00:c0:ca:b1:7f", tail);
	CHECK(!strcmp(tail, "0000"));
	link_tail(NULL, tail);
	CHECK(!strcmp(tail, "0000"));
	link_tail("00:c0:ca:b1:zz:2a", tail);
	CHECK(!strcmp(tail, "0000"));
	link_ssid("7F2A", s, sizeof s);
	CHECK(!strcmp(s, "ZyperOS-Link-7F2A"));
	CHECK(strlen(s) < 32);
	link_name("7F2A", name, sizeof name);
	CHECK(!strcmp(name, "Pixel 2 7F2A"));
	CHECK(link_name_of_ssid("ZyperOS-Link-7F2A", name, sizeof name) && !strcmp(name, "Pixel 2 7F2A"));
	/* File Transfer's network, a home's, a longer or a lower-case one: no host */
	CHECK(!link_name_of_ssid("ZyperOS-7F2A", name, sizeof name));
	CHECK(!link_name_of_ssid("ZyperOS-Link-7F2A9", name, sizeof name));
	CHECK(!link_name_of_ssid("ZyperOS-Link-7f2a", name, sizeof name));
	CHECK(!link_name_of_ssid("Home", name, sizeof name));
	CHECK(!link_name_of_ssid(NULL, name, sizeof name));
	/* The same password on both handhelds, a new one for another name,
	 * WPA2's eight at least, and no look-alikes */
	link_password("ZyperOS-Link-7F2A", p1, sizeof p1);
	link_password("ZyperOS-Link-7F2A", p2, sizeof p2);
	CHECK(!strcmp(p1, p2));
	CHECK(strlen(p1) == 9 && p1[4] == '-');
	CHECK(strcspn(p1, "0o1li") == strlen(p1));
	link_password("ZyperOS-Link-7F2B", p2, sizeof p2);
	CHECK(strcmp(p1, p2));
}

static void digits(void)
{
	char d[13] = { 0 };

	CHECK(link_ip_digits("10.42.1.1", d) && !memcmp(d, "010042001001", 12));
	CHECK(link_ip_digits("192.168.0.255", d) && !memcmp(d, "192168000255", 12));
	CHECK(link_ip_digits("0.0.0.0", d) && !memcmp(d, "000000000000", 12));
	CHECK(!link_ip_digits("256.1.1.1", d));
	CHECK(!link_ip_digits("10.42.1", d));
	CHECK(!link_ip_digits("10.42.1.1.5", d));
	CHECK(!link_ip_digits("10.42.1.1 ", d));
	CHECK(!link_ip_digits("10.42.-1.1", d));
	CHECK(!link_ip_digits("1000.1.1.1", d));
	CHECK(!link_ip_digits("", d));
	CHECK(!link_ip_digits(NULL, d));
	CHECK(link_same_net("10.42.1.23", "10.42.1.1"));
	CHECK(!link_same_net("192.168.1.23", "10.42.1.1"));
	CHECK(!link_same_net("10.42.2.23", "10.42.1.1"));
	CHECK(!link_same_net("", "10.42.1.1"));
}

static void options(void)
{
	char l[16][LINK_LINE_MAX];
	int n;

	n = link_opt_lines(LINK_ROLE_CLIENT, "10.42.1.1", l, 16);
	CHECK(n == 14);
	CHECK(!strcmp(l[0], "SETOPT\tkey=gambatte_gb_link_network_port\tvalue=56400"));
	CHECK(!strcmp(l[1], "SETOPT\tkey=gambatte_gb_link_network_server_ip_1\tvalue=0"));
	CHECK(!strcmp(l[2], "SETOPT\tkey=gambatte_gb_link_network_server_ip_2\tvalue=1"));
	CHECK(!strcmp(l[5], "SETOPT\tkey=gambatte_gb_link_network_server_ip_5\tvalue=4"));
	CHECK(!strcmp(l[6], "SETOPT\tkey=gambatte_gb_link_network_server_ip_6\tvalue=2"));
	CHECK(!strcmp(l[12], "SETOPT\tkey=gambatte_gb_link_network_server_ip_12\tvalue=1"));
	CHECK(!strcmp(l[13], "SETOPT\tkey=gambatte_gb_link_mode\tvalue=Network Client"));
	n = link_opt_lines(LINK_ROLE_SERVER, "10.42.1.1", l, 16);
	CHECK(n == 14 && !strcmp(l[13], "SETOPT\tkey=gambatte_gb_link_mode\tvalue=Network Server"));
	n = link_opt_lines(LINK_ROLE_OFF, NULL, l, 16);
	CHECK(n == 1 && !strcmp(l[0], "SETOPT\tkey=gambatte_gb_link_mode\tvalue=Not Connected"));
	CHECK(link_opt_lines(LINK_ROLE_CLIENT, "10.42.1", l, 16) == 0);
	CHECK(link_opt_lines(LINK_ROLE_CLIENT, "10.42.1.1", l, 13) == 0);
}

static void wire(void)
{
	char buf[256], name[24], game[96];
	unsigned len;

	len = link_wire(LINK_MSG_HELLO, "Pixel 2 7F2A", "Pokemon\tRed\n", buf, sizeof buf);
	CHECK(len && !strcmp(buf, "ZLINK1\thello\tPixel 2 7F2A\tPokemon Red "));
	CHECK(link_wire_read(buf, len, name, sizeof name, game, sizeof game) == LINK_MSG_HELLO);
	CHECK(!strcmp(name, "Pixel 2 7F2A") && !strcmp(game, "Pokemon Red "));
	len = link_wire(LINK_MSG_BEAT, "Pixel 2 7F2A", NULL, buf, sizeof buf);
	CHECK(link_wire_read(buf, len, name, sizeof name, NULL, 0) == LINK_MSG_BEAT);
	CHECK(link_wire_read("ZLINK1\tbye\tX", 12, NULL, 0, NULL, 0) == LINK_MSG_BYE);
	CHECK(link_wire_read("ZLINK2\tbye\tX\t", 13, NULL, 0, NULL, 0) == LINK_MSG_NONE);
	CHECK(link_wire_read("ZLINK1\tjump\tX\t", 14, NULL, 0, NULL, 0) == LINK_MSG_NONE);
	CHECK(link_wire_read("ZLINK1\tbeat", 11, NULL, 0, NULL, 0) == LINK_MSG_NONE);
	CHECK(link_wire_read("ZLINK1\tbe\0t\tX", 13, NULL, 0, NULL, 0) == LINK_MSG_NONE);
	CHECK(link_wire_read("hello", 5, NULL, 0, NULL, 0) == LINK_MSG_NONE);
	CHECK(link_wire(LINK_MSG_NONE, "a", "b", buf, sizeof buf) == 0);
	CHECK(link_wire(LINK_MSG_BEAT, "a", "b", buf, 8) == 0);
}

/* ---- each side ------------------------------------------------------------- */

static void host_side(void)
{
	char pass[16];

	link_stop();
	reset_log();
	g_hs = HOTSPOT_STARTING;
	CHECK(link_host("Tetris"));
	link_password("ZyperOS-Link-7F2A", pass, sizeof pass);
	{
		char want[96];

		snprintf(want, sizeof want, "hotspot ZyperOS-Link-7F2A %s", pass);
		CHECK(at(want) == 0);
	}
	CHECK(link_state_now() == LINK_HOST_STARTING);
	/* Nothing listens before the network is up */
	run(500);
	CHECK(logged("udp ") == 0);
	g_hs = HOTSPOT_UP;
	run(100);
	CHECK(at("udp 10.42.1.1 56399") >= 0);
	CHECK(link_state_now() == LINK_HOST_WAITING);
	/* Nothing to Diatom until the other one says hello */
	run(2000);
	CHECK(logged("resident") == 0);
	arrive(LINK_MSG_HELLO, "Pixel 2 0C11", "Tetris", "10.42.1.23");
	run(100);
	CHECK(link_state_now() == LINK_LINKED);
	CHECK(logged("resident SETOPT") == 14);
	CHECK(at("resident SETOPT\tkey=gambatte_gb_link_mode\tvalue=Network Server") >= 0);
	CHECK(at("send 10.42.1.23:56399 ZLINK1\twelcome\tPixel 2 7F2A\tTetris") >= 0);
	CHECK(at("notice linked Pixel 2 0C11") >= 0);
	CHECK(!strcmp(link_peer_name(), "Pixel 2 0C11"));
	/* A beat every half second to the joiner, and beats keep it linked */
	reset_log();
	run(5000);
	arrive(LINK_MSG_BEAT, "Pixel 2 0C11", "", "10.42.1.23");
	run(5000);
	CHECK(logged("send 10.42.1.23:56399 ZLINK1\tbeat") >= 18);
	CHECK(link_state_now() == LINK_LINKED);
	/* A third handheld's hello: taken. The joiner's again: welcome again */
	reset_log();
	arrive(LINK_MSG_HELLO, "Pixel 2 9999", "", "10.42.1.40");
	arrive(LINK_MSG_HELLO, "Pixel 2 0C11", "", "10.42.1.23");
	run(100);
	CHECK(at("send 10.42.1.40:56399 ZLINK1\tbusy\tPixel 2 7F2A\tTetris") >= 0);
	CHECK(at("send 10.42.1.23:56399 ZLINK1\twelcome\tPixel 2 7F2A\tTetris") >= 0);
	CHECK(logged("resident") == 0);
	/* A beat from a stranger keeps nothing alive: six seconds with no
	 * beat from the joiner is a lost link, said once, the core's link off */
	reset_log();
	run(3000);
	arrive(LINK_MSG_BEAT, "Pixel 2 9999", "", "10.42.1.40");
	run(3100);
	CHECK(link_state_now() == LINK_LOST);
	CHECK(at("resident SETOPT\tkey=gambatte_gb_link_mode\tvalue=Not Connected") >= 0);
	CHECK(logged("notice lost Pixel 2 0C11") == 1);
	run(3000);
	CHECK(logged("notice") == 1);
	CHECK(logged("resident") == 1);
	/* The stop: the hotspot down, nothing more to the core */
	reset_log();
	link_stop();
	CHECK(at("hotspot stop") >= 0);
	CHECK(logged("resident") == 0);
	CHECK(link_state_now() == LINK_OFF);
	/* A second stop does nothing */
	reset_log();
	link_stop();
	CHECK(g_nlog == 0);
}

static void host_failures(void)
{
	link_stop();
	reset_log();
	g_hotspot_ok = false;
	CHECK(!link_host("Tetris"));
	CHECK(link_state_now() == LINK_FAILED && link_failure_now() == LINK_FAIL_NETWORK);
	link_stop();
	CHECK(at("hotspot stop") >= 0);
	g_hotspot_ok = true;

	g_hs = HOTSPOT_STARTING;
	link_host("Tetris");
	g_hs = HOTSPOT_FAILED;
	run(100);
	CHECK(link_failure_now() == LINK_FAIL_NETWORK);
	link_stop();

	/* Diatom refusing the options: failed, and nothing linked said */
	g_hs = HOTSPOT_UP;
	link_host("Tetris");
	run(100);
	g_resident_ok = false;
	reset_log();
	arrive(LINK_MSG_HELLO, "Pixel 2 0C11", "", "10.42.1.23");
	run(100);
	CHECK(link_failure_now() == LINK_FAIL_GAME);
	CHECK(logged("notice") == 0 && logged("send") == 0);
	g_resident_ok = true;
	link_stop();

	/* The hotspot going away while linked is a lost link */
	link_host("Tetris");
	run(100);
	arrive(LINK_MSG_HELLO, "Pixel 2 0C11", "", "10.42.1.23");
	run(100);
	CHECK(link_state_now() == LINK_LINKED);
	g_hs = HOTSPOT_FAILED;
	run(100);
	CHECK(link_state_now() == LINK_LOST);
	link_stop();
	g_hs = HOTSPOT_UP;
}

static void join_side(void)
{
	static const char *seen[] = { "Home", "ZyperOS-Link-0C11", "ZyperOS-7F2A", "ZyperOS-Link-0C11",
	                              "ZyperOS-Link-7F2A", "ZyperOS-Link-BEEF" };
	char want[96], pass[16];
	int i;

	link_stop();
	reset_log();
	for (i = 0; i < 6; i++) g_scan[i] = seen[i];
	g_nscan = 6;
	g_scan_state = 0;
	CHECK(link_join_look("Tetris DX"));
	CHECK(link_state_now() == LINK_JOIN_LOOKING && !link_scanned());
	run(300);
	CHECK(link_host_count() == 0);
	g_scan_state = 1;
	run(100);
	/* The links' networks once each, not File Transfer's, not a home's,
	 * not this handheld's own */
	CHECK(link_scanned());
	CHECK(link_host_count() == 2);
	CHECK(!strcmp(link_host_name(0), "Pixel 2 0C11"));
	CHECK(!strcmp(link_host_name(1), "Pixel 2 BEEF"));
	/* Scanned again while it is looked at */
	reset_log();
	run(3100);
	CHECK(logged("scan") >= 1);
	CHECK(!link_join_pick(5));
	CHECK(link_join_pick(0));
	link_password("ZyperOS-Link-0C11", pass, sizeof pass);
	snprintf(want, sizeof want, "join ZyperOS-Link-0C11 %s", pass);
	CHECK(at(want) >= 0);
	CHECK(link_state_now() == LINK_JOIN_JOINING && !strcmp(link_joining_name(), "Pixel 2 0C11"));
	/* No hello before an address, nor with the last network's still on */
	g_join = 0;
	reset_log();
	run(2000);
	g_join = 1;
	g_join_addr = "192.168.1.40";
	run(500);
	CHECK(logged("send") == 0);
	CHECK(link_state_now() == LINK_JOIN_JOINING);
	g_join_addr = "10.42.1.23";
	run(100);
	CHECK(link_state_now() == LINK_JOIN_ASKING);
	CHECK(at("udp * 0") >= 0);
	CHECK(at("send 10.42.1.1:56399 ZLINK1\thello\tPixel 2 7F2A\tTetris DX") >= 0);
	/* Hello again every half second until the host answers */
	run(1000);
	CHECK(logged("send 10.42.1.1:56399 ZLINK1\thello") >= 3);
	CHECK(logged("resident") == 0);
	arrive(LINK_MSG_WELCOME, "Pixel 2 0C11", "Tetris DX", "10.42.1.1");
	run(100);
	CHECK(link_state_now() == LINK_LINKED);
	CHECK(logged("resident SETOPT") == 14);
	CHECK(at("resident SETOPT\tkey=gambatte_gb_link_network_server_ip_2\tvalue=1") >= 0);
	CHECK(at("resident SETOPT\tkey=gambatte_gb_link_mode\tvalue=Network Client") >= 0);
	CHECK(logged("notice linked Pixel 2 0C11") == 1);
	/* The host's bye: it left, said once, the core's link off */
	reset_log();
	run(1000);
	CHECK(logged("send 10.42.1.1:56399 ZLINK1\tbeat") >= 2);
	arrive(LINK_MSG_BYE, "Pixel 2 0C11", "", "10.42.1.1");
	run(100);
	CHECK(link_state_now() == LINK_LOST);
	CHECK(logged("notice left Pixel 2 0C11") == 1);
	CHECK(at("resident SETOPT\tkey=gambatte_gb_link_mode\tvalue=Not Connected") >= 0);
	reset_log();
	link_stop();
	CHECK(at("join end") >= 0 && logged("hotspot") == 0);
}

static void join_failures(void)
{
	link_stop();
	g_scan_ok = false;
	CHECK(!link_join_look("Tetris"));
	CHECK(link_failure_now() == LINK_FAIL_NETWORK);
	link_stop();
	g_scan_ok = true;

	/* The network never joins */
	g_scan_state = 1;
	link_join_look("Tetris");
	run(100);
	link_join_pick(0);
	g_join = 0;
	run(30000);
	CHECK(link_failure_now() == LINK_FAIL_JOIN);
	reset_log();
	link_stop();
	CHECK(at("join end") >= 0);

	/* On it, and nobody answers in ten seconds */
	link_join_look("Tetris");
	run(100);
	link_join_pick(0);
	g_join = 1;
	run(9500);
	CHECK(link_state_now() == LINK_JOIN_ASKING);
	run(700);
	CHECK(link_failure_now() == LINK_FAIL_ANSWER);
	link_stop();

	/* The host is linked with another */
	link_join_look("Tetris");
	run(100);
	link_join_pick(0);
	run(100);
	reset_log();
	arrive(LINK_MSG_BUSY, "Pixel 2 0C11", "", "10.42.1.1");
	run(100);
	CHECK(link_failure_now() == LINK_FAIL_BUSY);
	CHECK(logged("resident") == 0);
	link_stop();
}

/* The game's end: quiet, and nothing to Diatom, whose game is gone; the
 * stop still says bye and takes the network down */
static void game_end_is_quiet(void)
{
	link_stop();
	g_hs = HOTSPOT_UP;
	link_host("Tetris");
	run(100);
	arrive(LINK_MSG_HELLO, "Pixel 2 0C11", "", "10.42.1.23");
	run(100);
	CHECK(link_state_now() == LINK_LINKED);
	reset_log();
	link_game_over();
	run(10000);
	CHECK(g_nlog == 0);
	link_stop();
	CHECK(logged("send 10.42.1.23:56399 ZLINK1\tbye") == 2);
	CHECK(logged("resident") == 0);
	CHECK(at("hotspot stop") >= 0);
}

/* src/linknet.c's socket on the loopback: one sends, the other hears it and
 * whom from, and nothing waiting is 0, at once */
static void real_socket(void)
{
	int a = linknet_open("127.0.0.1", 0), b = linknet_open("127.0.0.1", 56398), i, got = 0;
	char buf[64], from[16];

	CHECK(a >= 0 && b >= 0);
	CHECK(linknet_recv(b, buf, sizeof buf, from, sizeof from) == 0);
	CHECK(linknet_send(a, "127.0.0.1", 56398, "ZLINK1\tbeat\tX\t", 14));
	CHECK(!linknet_send(a, "not an address", 56398, "x", 1));
	for (i = 0; i < 100 && !got; i++) {
		got = linknet_recv(b, buf, sizeof buf, from, sizeof from);
		if (!got) { struct timespec ts = { 0, 1000000 }; nanosleep(&ts, NULL); }
	}
	CHECK(got == 14 && !memcmp(buf, "ZLINK1\tbeat\tX\t", 14) && !strcmp(from, "127.0.0.1"));
	CHECK(link_wire_read(buf, (unsigned)got, NULL, 0, NULL, 0) == LINK_MSG_BEAT);
	CHECK(linknet_open("10.42.1.999", 0) < 0);
	linknet_close(a);
	linknet_close(b);
}

int main(void)
{
	/* link.c's lines are the launcher's log; the check says only what failed */
	if (!freopen("/dev/null", "w", stderr)) return 1;
	link_use(&FAKE, "00:c0:ca:b1:7f:2a");
	CHECK(!strcmp(link_self_name(), "Pixel 2 7F2A"));
	offered();
	names();
	digits();
	options();
	wire();
	host_side();
	host_failures();
	join_side();
	join_failures();
	game_end_is_quiet();
	real_socket();
	if (fails) { printf("link: %d failed\n", fails); return 1; }
	printf("link: ok\n");
	return 0;
}
