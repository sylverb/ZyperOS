/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_LINK_H
#define ZYPEROS_LINK_H

/* The Link Cable: a Game Boy or Game Boy Color game on two Pixel 2s, linked
 * as by the cable, over the handhelds' own Wi-Fi (DESIGN.md 11.70).
 *
 *   Gambatte    the link itself: its network Game Link, one handheld the
 *               server and the other its client, two bytes each way a
 *               transfer (mk/patches/gambatte-link.patch makes it safe to
 *               lose). Turned on by its core options, which Diatom takes
 *               while the game is paused (SETOPT) and the core reads as the
 *               game goes on.
 *   the host    makes its own network, as Player 2 does (src/hotspot.h), a
 *               name of its own (ZyperOS-Link- and the adapter's last two
 *               bytes) and a code made new from the kernel's random each
 *               time it hosts ("q7mx-3kpa", File Transfer's form), which
 *               is the network's WPA2 password and which its page shows
 *   the joiner  finds that name in a Wi-Fi scan, the player types the code
 *               the host shows, and it joins for the game's time (nothing
 *               saved); its Wi-Fi comes back as it was
 *   this file   the two launchers' own word to each other, a datagram on
 *               LINK_BEAT_PORT: hello and welcome, a beat every half second
 *               while linked, and bye. The beat is how a lost link is told:
 *               no beat for LINK_LOST_MS is the other one gone (out of
 *               range, off), said once over the game, and the core's link
 *               turned off. The launchers keep beating while their game is
 *               paused, so a menu is never a lost link.
 *
 * Started from the in-game menu's Link Cable row over the paused game, on
 * for the rest of the game; every end of the game ends it (link_stop).
 *
 * No SDL and no system calls of its own: every process, network and
 * protocol action goes through link_ops, which src/link_screen.c fills with
 * the real ones and tools/link-check.c with fakes. */

#include <stdbool.h>

/* ---- what is fixed ------------------------------------------------------- */

/* The core the link is on (Gambatte's network Game Link; mGBA has none) */
#define LINK_CORE        "gambatte"
/* The host's address on its own network: the hotspot's (src/captive.h) */
#define LINK_HOST_ADDR   "10.42.1.1"
/* Gambatte's port (its first, gambatte_gb_link_network_port), and the
 * launchers' beside it */
#define LINK_GAME_PORT   "56400"
#define LINK_BEAT_PORT   56399
/* The network's name: this, then the adapter's last two bytes */
#define LINK_SSID_PREFIX "ZyperOS-Link-"
/* How a handheld is named to the other one: "Pixel 2 7F2A" */
#define LINK_MODEL       "Pixel 2"

#define LINK_SSID_MAX    33
#define LINK_NAME_MAX    24
#define LINK_PASS_MAX    16
#define LINK_GAME_MAX    96
#define LINK_LINE_MAX    96
#define LINK_HOSTS_MAX   6
#define LINK_ADDR_MAX    16

/* A beat every LINK_BEAT_MS; none for LINK_LOST_MS is a lost link. Six
 * seconds: a launcher blocked as long as a hotspot's start or a state's save
 * is not a lost link, and the core's own wait for an answer (ten seconds,
 * the patch) is still longer, so the core is told before it gives up. */
#define LINK_BEAT_MS     500
#define LINK_LOST_MS     6000

/* Offered for a game on `core`, on a device with the hotspot it is built on
 * (`device`: plat_player2, the Pixel 2 with a Wi-Fi adapter's hostapd) */
bool link_offered(const char *core, bool device);

/* ---- the pure parts, for the check --------------------------------------- */

/* The last two bytes of an adapter's address ("aa:bb:cc:dd:7f:2a") as four
 * capitals, "7F2A"; "0000" for an address that is not one */
void link_tail(const char *mac, char out[5]);
/* The network's name for a tail, "ZyperOS-Link-7F2A" */
void link_ssid(const char *tail, char *out, unsigned n);
/* A handheld's name for a tail, "Pixel 2 7F2A" */
void link_name(const char *tail, char *out, unsigned n);
/* The name of the handheld whose network `ssid` is, "Pixel 2 7F2A"; false
 * for a network that is no link's */
bool link_name_of_ssid(const char *ssid, char *out, unsigned n);
/* A code as the player typed it, as the host made it: lower case, spaces
 * and dashes dropped, and the dash put back after the fourth ("Q7MX 3KPA"
 * and "q7mx3kpa" are "q7mx-3kpa"). False, and `out` empty, for one that is
 * not eight letters and digits: no code the host makes. A security review
 * of 1.1.0's Link Cable found the first password, made from the network's
 * name, one anybody near could make too; the code is random now, and only
 * the person holding the other handheld sees it. */
bool link_code_typed(const char *typed, char *out, unsigned n);

/* An IPv4 address as the twelve digits Gambatte's server address options
 * take, three an octet ("10.42.1.1": 0 1 0, 0 4 2, 0 0 1, 0 0 1); false for
 * anything that is not four octets of 0 to 255 */
bool link_ip_digits(const char *addr, char digits[12]);

/* Both addresses in one /24 (the host's network's) */
bool link_same_net(const char *a, const char *b);

typedef enum { LINK_ROLE_OFF, LINK_ROLE_SERVER, LINK_ROLE_CLIENT } link_role;

/* The SETOPT lines to Diatom for `role`, into `lines`: the mode, and for the
 * server and the client the port and the twelve digits of `addr` (the
 * server's own, the client's host's), fourteen in all; off is the mode
 * alone ("Not Connected"). Their count, or 0 when `addr` is no address or
 * `max` is too few. */
int link_opt_lines(link_role role, const char *addr, char lines[][LINK_LINE_MAX], int max);

/* The launchers' datagrams, one line of tab-separated words:
 * "ZLINK1\t<kind>\t<name>\t<game>". The game is the title the other one
 * plays (the host's in its welcome, the joiner's in its hello), for the
 * log; tabs and line ends in it become spaces. */
typedef enum { LINK_MSG_NONE, LINK_MSG_HELLO, LINK_MSG_WELCOME, LINK_MSG_BUSY,
               LINK_MSG_BEAT, LINK_MSG_BYE } link_msg;
/* The datagram into `out`; its length, 0 when it does not fit */
unsigned link_wire(link_msg kind, const char *name, const char *game, char *out, unsigned n);
/* `buf` (`len` bytes, not terminated) read: LINK_MSG_NONE for anything that
 * is not one. `name` and `game` may be NULL. */
link_msg link_wire_read(const char *buf, unsigned len, char *name, unsigned namen,
                        char *game, unsigned gamen);

/* ---- what it is done with ------------------------------------------------ */

typedef enum {
	LINK_OFF,
	LINK_HOST_STARTING,   /* the host's network is coming up */
	LINK_HOST_WAITING,    /* it is up; waiting for the other's hello */
	LINK_JOIN_LOOKING,    /* scanning for hosts, the list as it is found */
	LINK_JOIN_JOINING,    /* on a host's network, an address on its way */
	LINK_JOIN_ASKING,     /* saying hello to the host */
	LINK_LINKED,
	LINK_LOST,            /* the other one went; what is up stays for link_stop */
	LINK_FAILED           /* a start that did not work: link_failure says why */
} link_state;

typedef enum {
	LINK_FAIL_NONE,
	LINK_FAIL_NETWORK,    /* the hotspot or a scan would not start */
	LINK_FAIL_JOIN,       /* the host's network could not be joined: most
	                       * likely a wrong code */
	LINK_FAIL_CODE,       /* what was typed is no code */
	LINK_FAIL_ANSWER,     /* on its network, and the host did not answer */
	LINK_FAIL_BUSY,       /* the host is linked with another */
	LINK_FAIL_GAME        /* Diatom would not take the options */
} link_failure;

/* What a notice over the game says: linked with the other one, the link
 * lost (no beat), or the other one left it (its bye: it stopped, or its
 * game ended) */
typedef enum { LINK_NEWS_LINKED, LINK_NEWS_LOST, LINK_NEWS_LEFT } link_news;

typedef struct {
	/* One protocol line to Diatom, as plat_resident_line sends it */
	bool (*resident)(const char *line);
	/* A new code, from the kernel's random, in File Transfer's form
	 * (captive_password); false when there was no randomness */
	bool (*code)(char *out, unsigned n);
	/* The hotspot under a name and password of this file's, without its own
	 * DNS; its poll (a hotspot_state); its stop, which brings the Wi-Fi back
	 * (a second or two: only link_stop asks it) */
	bool (*hotspot_start)(const char *ssid, const char *pass);
	int  (*hotspot_poll)(void);
	void (*hotspot_stop)(void);
	/* A Wi-Fi scan not waited for, as wifi_scan_start, _poll and _take:
	 * the networks' names into `ssids`, their count */
	bool (*scan_start)(void);
	int  (*scan_poll)(void);
	int  (*scan_take)(char ssids[][LINK_SSID_MAX], int max);
	/* The host's network joined for the game's time, nothing saved: the
	 * start (false when it could not even ask), its poll (1 once an address
	 * is in hand, into `addr`; 0 not yet; -1 failed) and its end, the Wi-Fi
	 * as it was before the scan (blocking: only link_stop asks it, after a
	 * join or a scan alone) */
	bool (*join_start)(const char *ssid, const char *pass);
	int  (*join_poll)(char *addr, unsigned n);
	void (*join_end)(void);
	/* A datagram socket on `addr` (NULL for any) and `port` (0 for any),
	 * never blocking: its handle, or -1. A send to `addr`:`port`; a receive,
	 * its length or 0 for none waiting, the sender's address into `from`. */
	int  (*udp_open)(const char *addr, int port);
	bool (*udp_send)(int s, const char *addr, int port, const char *buf, unsigned len);
	int  (*udp_recv)(int s, char *buf, unsigned n, char *from, unsigned fromn);
	void (*udp_close)(int s);
	unsigned (*now)(void);
	/* A notice over the game; `peer` the other one's name */
	void (*notice)(link_news what, const char *peer);
} link_ops;

/* The ops (NULL keeps the ones in use), and this handheld's adapter address
 * ("aa:bb:...", NULL or "" for none known yet), which names it and its
 * network */
void link_use(const link_ops *ops, const char *mac);

/* Host: the network up (link_tick goes on), then waiting for the other's
 * hello. `game` is the title playing, for the other one's log. False, and
 * LINK_FAILED, when it could not start. Anything still up (a lost link, a
 * failure) is stopped first. Takes as long as the hotspot's start takes. */
bool link_host(const char *game);
/* Join: a scan, and its list as link_hosts says it, scanned again every
 * few seconds while it is looked at. False, and LINK_FAILED, when no scan
 * would start. */
bool link_join_look(const char *game);
/* The `i`th host of the list, with the code its page shows as the player
 * typed it (link_code_typed): its network joined, then its hello. False for
 * no such host, a code that is none, or a join that would not start; a
 * code that is none fails with LINK_FAIL_CODE. */
bool link_join_pick(int i, const char *typed);

/* From every tick while it is on, in a game and in the screens over its
 * pause: the network coming up, the hellos, the beats, a lost link. Bounded:
 * never waits. */
void link_tick(unsigned now_ms);

/* The game is over, however it ended: no notice and nothing to Diatom from
 * here (its game is gone, and an option sent now would stand for the next),
 * and link_stop says bye and takes the network down. */
void link_game_over(void);

/* Everything off: bye to the other one, the core's link off (unless the
 * game is over), the socket, then the hotspot or the joined network, which
 * brings the Wi-Fi back. At every end of a game and from the page; nothing
 * when nothing is on. */
void link_stop(void);

/* ---- what the row and the pages say ------------------------------------- */

link_state   link_state_now(void);
link_failure link_failure_now(void);
link_role    link_role_now(void);
/* This handheld's name ("Pixel 2 7F2A") and the other one's ("" until
 * known) */
const char  *link_self_name(void);
const char  *link_peer_name(void);
/* The hosts the scan found, by name, and how many; whether a scan has
 * answered yet */
int          link_host_count(void);
const char  *link_host_name(int i);
bool         link_scanned(void);
/* A network is up for the link (the hotspot, or the host's joined): its
 * stop brings the Wi-Fi back, a second or two, said on a panel first */
bool         link_owes_wifi(void);
/* The code this handheld's network takes while it hosts, "" otherwise:
 * its page shows it */
const char  *link_code(void);
/* The host being joined or asked (LINK_JOIN_JOINING, LINK_JOIN_ASKING) */
const char  *link_joining_name(void);

#endif
