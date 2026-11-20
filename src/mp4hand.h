/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_MP4HAND_H
#define ZYPEROS_MP4HAND_H

/* MP4's queue handed from one process to the next while a program of its
 * own plays over it: your PICO-8 (DESIGN.md 11.55), which keeps MP4 playing
 * as a Diatom game does (src/portmenu.h, port_keeps_mp4). Mine, 2026-11-20:
 * "mp4 should keep playing while pico 8 is running like we do on other
 * systems. and menu and select should open the mp4 like other systems".
 *
 * The daemon (src/mp4d/) plays one file and knows nothing of what comes
 * next; the queue is the launcher's (src/mp4c.h), and the launcher exits for
 * a program of its own. So the queue goes into this file as it hands over
 * (launch_port), and three processes take it in turn, one at a time:
 *
 *   - `zyperos.elf --mp4-follow`, which launch.sh starts beside the program:
 *     the next track at each END, the resume row and a book's place kept,
 *     as the launcher's tick does over a game. It writes the file as it
 *     ends.
 *   - the in-game menu over the program (`zyperos.elf --port-menu`), for
 *     SELECT: MP4 itself, its shelf and Now Playing. portkeys stops the
 *     follower while the menu is up, so the menu is the one that talks to
 *     the daemon then; it writes the file as it closes, and the follower
 *     takes it back from there.
 *   - the launcher after the program, which takes the queue on and removes
 *     the file.
 *
 * Each takes it as the daemon finds it: on the track the daemon says it
 * plays, wherever the file left the queue, and on the next track if the one
 * the file names ran out while no process was listening (mp4c_hand_in).
 *
 * In /tmp, which is memory: a power-off takes it, and MP4's resume row
 * (src/mp4resume.h) is what comes back after one. Text, one field a line,
 * "key<TAB>value", written whole under another name and renamed into place.
 * This file is the format and nothing else - no socket, no SDL, no database -
 * so tools/mp4hand-check.c can hold it to its word. */

#include <stdbool.h>
#include <stddef.h>

#define MP4HAND_PATH  "/tmp/zyperos_mp4_queue"
#define MP4HAND_MAGIC "ZMQ1"
/* The most tracks a file carries: Shuffle All's every song on a large card */
#define MP4HAND_MAX   65536

typedef enum { MH_STOPPED, MH_PLAYING, MH_PAUSED } mh_state;

typedef struct {
	mh_state state;            /* as the writer last heard it */
	double   at;               /* seconds into the file playing */
	int      mode;             /* the queue's own play mode (muq_mode) */
	unsigned seed;             /* its shuffle's generator */
	bool     book;             /* a book: in order, always */
	bool     videos;           /* videos: in order too */
	bool     names_each;       /* each track's own artist and album (Shuffle All) */
	bool     all;              /* Shuffle All's queue, for the resume row (MR_ALL) */
	double   speed;            /* a book's, 1.0 otherwise */
	char     root[512];        /* the card, the tracks' folder */
	char     artist[128], album[128];
	int      n;                /* tracks */
	int      pos;              /* where in `order` it plays */
	int     *order;            /* order[pos] is the track playing: a permutation of 0..n-1 */
	char   **track;            /* n paths, relative to root */
} mp4hand;

/* `h` as the file holds it, to `path`, whole or not at all. False when it
 * cannot be (a track with a tab or a newline, an order that is no
 * permutation, more than MP4HAND_MAX) or the write failed. */
bool mp4hand_write(const char *path, const mp4hand *h);

/* The file at `path` into `h` (mp4hand_free it after). False for no file or
 * anything that is not one: another magic, a field it does not know the
 * value of, an order that is no permutation of the tracks, a place past
 * them; `h` is then empty. A file with no tracks is one: nothing queued. */
bool mp4hand_read(const char *path, mp4hand *h);

void mp4hand_free(mp4hand *h);

#endif
