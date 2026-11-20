/* SPDX-License-Identifier: GPL-3.0-only */
/* MP4's queue handed over a program of its own (src/mp4hand.h): the file
 * written and read back whole, each kind of queue (an album, a book, videos,
 * Shuffle All's with its order), a file of no tracks, what is refused on the
 * way out and on the way in; and which programs MP4 plays on over
 * (src/portmenu.h): your PICO-8 alone. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/mp4hand.h"
#include "../src/pico8.h"
#include "../src/portmenu.h"

static int fails, checks;
static char path[512];

static void ck(int cond, const char *what)
{
	checks++;
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

/* `body` as the file, after the magic */
static void put(const char *magic, const char *body)
{
	FILE *f = fopen(path, "w");

	if (!f) return;
	if (magic) fprintf(f, "%s\n", magic);
	fputs(body, f);
	fclose(f);
}

static const char *TRACKS[] = { "Music/Demo Library/First Light/01 First Light.mp3",
                                "Music/Demo Library/First Light/02 Open Road.mp3",
                                "Music/Demo Library/First Light/03 Home Again.mp3" };

/* The three tracks, in `order`, at `pos` */
static mp4hand album(const int order[3], int pos)
{
	mp4hand h;
	int i;

	memset(&h, 0, sizeof h);
	h.state = MH_PLAYING;
	h.at = 83.44;
	h.mode = 3;
	h.seed = 4000000000u;
	h.speed = 1.0;
	h.n = 3;
	h.pos = pos;
	h.order = malloc(3 * sizeof *h.order);
	h.track = malloc(3 * sizeof *h.track);
	for (i = 0; i < 3; i++) {
		h.order[i] = order[i];
		h.track[i] = strdup(TRACKS[i]);
	}
	snprintf(h.root, sizeof h.root, "/mnt/SDCARD");
	snprintf(h.artist, sizeof h.artist, "Demo Library");
	snprintf(h.album, sizeof h.album, "First Light");
	return h;
}

static void round_trip(void)
{
	static const int SHUFFLED[3] = { 2, 0, 1 }, IN_ORDER[3] = { 0, 1, 2 };
	mp4hand h = album(SHUFFLED, 1), b;

	printf("written and read back:\n");
	ck(mp4hand_write(path, &h), "an album, shuffled, playing, is written");
	{
		char first[4096] = "", second[4096] = "";
		FILE *f = fopen(path, "r");

		if (f) { if (fread(first, 1, sizeof first - 1, f)) {} fclose(f); }
		ck(mp4hand_write(path, &h) && (f = fopen(path, "r")) != NULL, "and written again, the same");
		if (f) { if (fread(second, 1, sizeof second - 1, f)) {} fclose(f); }
		ck(strncmp(first, "ZMQ1\nid\t", 8) == 0 && strcmp(first, second) != 0,
		   "each write with an id of its own, so the same queue twice is two files' bytes");
	}
	ck(mp4hand_read(path, &b), "and read back");
	ck(b.state == MH_PLAYING && b.at > 83.39 && b.at < 83.41 && b.mode == 3 && b.seed == 4000000000u &&
	   b.n == 3 && b.pos == 1 && !b.book && !b.videos && !b.names_each && !b.all && b.speed == 1.0,
	   "its state, second, mode, seed and place the same");
	ck(b.order[0] == 2 && b.order[1] == 0 && b.order[2] == 1, "its order the same");
	ck(!strcmp(b.track[0], TRACKS[0]) && !strcmp(b.track[2], TRACKS[2]) && !strcmp(b.root, "/mnt/SDCARD") &&
	   !strcmp(b.artist, "Demo Library") && !strcmp(b.album, "First Light"), "its tracks and names the same");
	mp4hand_free(&b);
	ck(b.n == 0 && !b.order && !b.track, "freed empty");

	/* A book, at 1.5x; videos; Shuffle All's, naming each track */
	mp4hand_free(&h);
	h = album(IN_ORDER, 2);
	h.mode = 0;
	h.state = MH_PAUSED;
	h.book = true;
	h.speed = 1.5;
	ck(mp4hand_write(path, &h) && mp4hand_read(path, &b) && b.book && b.speed == 1.5 &&
	   b.state == MH_PAUSED && b.pos == 2, "a book, paused, at 1.5x");
	mp4hand_free(&b);
	h.book = false;
	h.videos = true;
	h.state = MH_STOPPED;
	ck(mp4hand_write(path, &h) && mp4hand_read(path, &b) && b.videos && b.state == MH_STOPPED,
	   "videos, stopped");
	mp4hand_free(&b);
	h.videos = false;
	h.names_each = h.all = true;
	h.artist[0] = h.album[0] = '\0';
	ck(mp4hand_write(path, &h) && mp4hand_read(path, &b) && b.names_each && b.all && !b.artist[0] &&
	   !b.album[0], "Shuffle All's, no album named");
	mp4hand_free(&b);

	/* Nothing queued: MP4 kept over the program all the same */
	mp4hand_free(&h);
	memset(&h, 0, sizeof h);
	h.speed = 1.0;
	snprintf(h.root, sizeof h.root, "/mnt/SDCARD");
	ck(mp4hand_write(path, &h) && mp4hand_read(path, &b) && b.n == 0 && b.state == MH_STOPPED,
	   "a file of no tracks is one: nothing queued");
	mp4hand_free(&b);
}

static void refused_out(void)
{
	static const int IN_ORDER[3] = { 0, 1, 2 }, TWICE[3] = { 0, 1, 1 };
	mp4hand h = album(TWICE, 0), b;

	printf("refused on the way out, the file before left as it was:\n");
	ck(mp4hand_write(path, &(mp4hand){ .speed = 1.0, .root = "/mnt/SDCARD" }), "a file there");
	ck(!mp4hand_write(path, &h), "an order that plays a track twice");
	mp4hand_free(&h);
	h = album(IN_ORDER, 3);
	ck(!mp4hand_write(path, &h), "a place past the tracks");
	h.pos = 0;
	free(h.track[1]);
	h.track[1] = strdup("Music/A tab\there.mp3");
	ck(!mp4hand_write(path, &h), "a track with a tab");
	free(h.track[1]);
	h.track[1] = strdup("Music/A line\nbreak.mp3");
	ck(!mp4hand_write(path, &h), "or a line break");
	free(h.track[1]);
	h.track[1] = strdup("");
	ck(!mp4hand_write(path, &h), "an empty track");
	free(h.track[1]);
	h.track[1] = strdup(TRACKS[1]);
	snprintf(h.album, sizeof h.album, "Two\nlines");
	ck(!mp4hand_write(path, &h), "an album name of two lines");
	snprintf(h.album, sizeof h.album, "First Light");
	h.mode = 4;
	ck(!mp4hand_write(path, &h), "a mode there is none of");
	h.mode = 0;
	h.at = -1;
	ck(!mp4hand_write(path, &h), "a second less than 0");
	h.at = 0;
	h.speed = 0;
	ck(!mp4hand_write(path, &h), "no speed");
	ck(mp4hand_read(path, &b) && b.n == 0, "and the file written before is still the one there");
	mp4hand_free(&b);
	h.speed = 1.0;
	ck(!mp4hand_write("/nonexistent-folder/zyperos_mp4_queue", &h), "a folder that is not there");
	mp4hand_free(&h);
}

static void refused_in(void)
{
	static const char *const HEAD =
		"state\tplaying\nat\t1.0\nmode\t0\nseed\t1\nbook\t0\nvideos\t0\nnames\t0\nall\t0\nspeed\t1.00\n"
		"root\t/mnt/SDCARD\nartist\t\nalbum\t\n";
	static const struct { const char *magic, *tail, *what; } BAD[] = {
		{ "ZMQ0", "n\t1\npos\t0\norder\t0\ntrack\tMusic/a.mp3\n", "another magic" },
		{ NULL, "n\t1\npos\t0\norder\t0\ntrack\tMusic/a.mp3\n", "no magic" },
		{ "ZMQ1", "n\t2\npos\t0\norder\t0 1\ntrack\tMusic/a.mp3\n", "fewer tracks than it says" },
		{ "ZMQ1", "n\t1\npos\t0\norder\t0\ntrack\tMusic/a.mp3\ntrack\tMusic/b.mp3\n", "more" },
		{ "ZMQ1", "n\t2\npos\t0\norder\t1 1\ntrack\tMusic/a.mp3\ntrack\tMusic/b.mp3\n", "an order that is no permutation" },
		{ "ZMQ1", "n\t2\npos\t0\norder\t0 2\ntrack\tMusic/a.mp3\ntrack\tMusic/b.mp3\n", "an order past the tracks" },
		{ "ZMQ1", "n\t2\npos\t0\norder\t0\ntrack\tMusic/a.mp3\ntrack\tMusic/b.mp3\n", "an order too short" },
		{ "ZMQ1", "n\t2\npos\t0\norder\t0 1 \ntrack\tMusic/a.mp3\ntrack\tMusic/b.mp3\n", "an order with a space after" },
		{ "ZMQ1", "n\t2\npos\t0\ntrack\tMusic/a.mp3\ntrack\tMusic/b.mp3\n", "no order" },
		{ "ZMQ1", "n\t2\npos\t2\norder\t0 1\ntrack\tMusic/a.mp3\ntrack\tMusic/b.mp3\n", "a place past them" },
		{ "ZMQ1", "pos\t0\nn\t1\norder\t0\ntrack\tMusic/a.mp3\n", "a place before the count" },
		{ "ZMQ1", "n\t1\nn\t1\npos\t0\norder\t0\ntrack\tMusic/a.mp3\n", "two counts" },
		{ "ZMQ1", "n\t70000\npos\t0\n", "more tracks than any card" },
		{ "ZMQ1", "n\t-1\n", "fewer than none" },
		{ "ZMQ1", "n\t1\npos\t0\norder\t0\ntrack\t\n", "an empty track" },
		{ "ZMQ1", "n\t1\npos\t0\norder\t0\ntrack\tMusic/a.mp3\ncolour\tblue\n", "a field it does not know" },
		{ "ZMQ1", "n\t1\npos\t0\norder\t0\ntrack\tMusic/a.mp3\nno tab here\n", "a line with no field" },
	};
	mp4hand b;
	char body[1024];
	size_t i;

	printf("refused on the way in:\n");
	ck(!mp4hand_read("/nonexistent-folder/zyperos_mp4_queue", &b) && b.n == 0, "no file");
	for (i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
		snprintf(body, sizeof body, "%s%s", HEAD, BAD[i].tail);
		put(BAD[i].magic, body);
		ck(!mp4hand_read(path, &b) && b.n == 0 && !b.order && !b.track, BAD[i].what);
	}
	{
		static const char *const FIELD[][2] = {
			{ "state\tplaying\n", "state\tlooping\n" }, { "at\t1.0\n", "at\t-2\n" },
			{ "at\t1.0\n", "at\tsoon\n" }, { "mode\t0\n", "mode\t4\n" }, { "seed\t1\n", "seed\t-1\n" },
			{ "book\t0\n", "book\t2\n" }, { "speed\t1.00\n", "speed\t0\n" }, { "speed\t1.00\n", "speed\tnan\n" },
		};
		for (i = 0; i < sizeof FIELD / sizeof FIELD[0]; i++) {
			char h[600], *at;

			snprintf(h, sizeof h, "%s", HEAD);
			at = strstr(h, FIELD[i][0]);
			if (!at) { ck(0, "the field to break is in the head"); continue; }
			snprintf(body, sizeof body, "%.*s%s%sn\t1\npos\t0\norder\t0\ntrack\tMusic/a.mp3\n",
			         (int)(at - h), h, FIELD[i][1], at + strlen(FIELD[i][0]));
			put("ZMQ1", body);
			snprintf(h, sizeof h, "a value it cannot take: %.*s", (int)strlen(FIELD[i][1]) - 1, FIELD[i][1]);
			for (at = h; *at; at++) if (*at == '\t') *at = ' ';
			ck(!mp4hand_read(path, &b) && b.n == 0, h);
		}
	}
	snprintf(body, sizeof body, "%sn\t1\npos\t0\norder\t0\ntrack\tMusic/a.mp3\n", HEAD);
	put("ZMQ1", body);
	ck(mp4hand_read(path, &b) && b.n == 1 && !strcmp(b.track[0], "Music/a.mp3"),
	   "and the same file whole is one");
	mp4hand_free(&b);
}

static void keeps(void)
{
	printf("the programs MP4 plays on over:\n");
	ck(port_keeps_mp4(P8_CORE), "your PICO-8, by its script (pico8.h's P8_CORE)");
	ck(port_menu_for(P8_CORE), "which has the in-game menu, where SELECT opens MP4");
	ck(!port_keeps_mp4("run:ZyperOS/openbor/OpenBOR.sh") && !port_keeps_mp4("run:Emus/flycast/Flycast.sh") &&
	   !port_keeps_mp4("port") && !port_keeps_mp4("fake08") && !port_keeps_mp4(NULL),
	   "and no other: OpenBOR, the standalone Flycast, a port, a core, none");
}

int main(void)
{
	snprintf(path, sizeof path, "%s/zyperos-mp4hand-check.%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp",
	         (int)getpid());
	keeps();
	round_trip();
	refused_out();
	refused_in();
	unlink(path);
	if (fails) {
		printf("mp4hand: %d of %d failed\n", fails, checks);
		return 1;
	}
	printf("mp4hand: %d checks, the queue handed over whole or not at all\n", checks);
	return 0;
}
