/* SPDX-License-Identifier: GPL-3.0-only */
/* MP4's settings under its own name (src/mp4keys.h): a handheld's database
 * from 1.0.6, MP4's settings under "muse.", moved to "mp4." at the first
 * start of 1.1.0 - each value kept, once, nothing reset. The old names are
 * written out here as 1.0.6 wrote them (src/main.c, src/shelfbuild.c,
 * src/nowplaying_looks.c before the rename), not taken from the header. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/db.h"
#include "../src/mp4keys.h"

static int fails, checks;
static char dir[] = "/tmp/mp4keys-check-XXXXXX";
static char DEV[sizeof dir + sizeof "/device.db"];

static void ck(int cond, const char *what)
{
	checks++;
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

static int is(db *d, const char *key, const char *want)
{
	char v[64];

	db_get_str(d, key, v, sizeof v, "(none)");
	return !strcmp(v, want);
}

static bool count(const char *key, const char *value, void *ctx)
{
	(void)key, (void)value;
	++*(int *)ctx;
	return true;
}

/* How many settings are left under the old name */
static int old_left(db *d)
{
	int n = 0;

	db_each_prefix(d, "muse.", count, &n);
	return n;
}

/* The database gone, with its journal, for the next case's new handheld */
static void wipe(void)
{
	static const char *const tail[] = { "", "-wal", "-shm", "-journal" };
	char p[sizeof DEV + 8];
	size_t i;

	for (i = 0; i < sizeof tail / sizeof tail[0]; i++) {
		snprintf(p, sizeof p, "%s%s", DEV, tail[i]);
		unlink(p);
	}
}

/* A new start: the database closed and opened again, as the launcher does */
static db *restart(db *d)
{
	if (d) db_close(d);
	return db_open(DEV, DB_DEVICE);
}

/* What 1.0.6 left on a handheld whose player chose each of them */
static void old_settings(db *d)
{
	db_set_str(d, "muse.mode", "shuffle");
	db_set_str(d, "muse.show", "music");
	db_set_str(d, "muse.show_all_once", "1");
	db_set_str(d, "muse.look", "vinyl");
	/* A book's and a video's places are not under MP4's old name, and
	 * stay where they are */
	db_set_str(d, "book./mnt/SDCARD/Audiobooks/Dune", "4:1834.5");
	db_set_str(d, "video./mnt/SDCARD/Videos/Trip.mp4", "61.0");
}

static void upgrade(void)
{
	db *d;
	int n;

	printf("a 1.0.6 handheld's MP4 settings, at 1.1.0's first start:\n");
	d = restart(NULL);
	old_settings(d);
	d = restart(d);
	n = mp4keys_migrate(d);
	ck(n == 4, "the four moved");
	ck(is(d, "mp4.mode", "shuffle"), "the mode, shuffle, under mp4.mode");
	ck(is(d, "mp4.show", "music"), "Show, Music, under mp4.show");
	ck(is(d, "mp4.show_all_once", "1"), "Show's one-time All, done, under mp4.show_all_once");
	ck(is(d, "mp4.look", "vinyl"), "the Player Look, Vinyl, under mp4.look");
	ck(old_left(d) == 0, "nothing left under muse.");
	ck(is(d, "book./mnt/SDCARD/Audiobooks/Dune", "4:1834.5"), "a book's place kept as it was");
	ck(is(d, "video./mnt/SDCARD/Videos/Trip.mp4", "61.0"), "a video's place kept as it was");
	/* What the shelf does at its start (src/shelfbuild.c): Show set to All
	 * once, on a handheld with no mark. The mark came over, so Show stays
	 * on the player's Music. */
	ck(db_has(d, "mp4.show_all_once"), "the one-time All finds its mark, so Show is not reset to All");

	printf("the start after, and the one after that:\n");
	d = restart(d);
	ck(mp4keys_migrate(d) == 0, "a second start moves nothing");
	d = restart(d);
	ck(mp4keys_migrate(d) == 0, "nor a third");
	ck(is(d, "mp4.mode", "shuffle") && is(d, "mp4.show", "music") && is(d, "mp4.show_all_once", "1") &&
	   is(d, "mp4.look", "vinyl"), "each value as the player left it");
	db_close(d);
	wipe();
}

static void step_back(void)
{
	db *d;

	printf("a step back to 1.0.6 and forward again:\n");
	d = restart(NULL);
	old_settings(d);
	ck(mp4keys_migrate(d) == 4, "1.1.0's first start moved them");
	db_set_str(d, "mp4.show", "videos");
	/* 1.0.6's start: no muse.show_all_once, so it sets Show to All and
	 * writes its mark, under the old names */
	db_set_str(d, "muse.show", "all");
	db_set_str(d, "muse.show_all_once", "1");
	d = restart(d);
	ck(mp4keys_migrate(d) == 0, "forward again: nothing moved over what is there");
	ck(is(d, "mp4.show", "videos"), "the player's Show, Videos, kept over 1.0.6's All");
	ck(is(d, "mp4.show_all_once", "1"), "the mark kept");
	ck(old_left(d) == 0, "the old ones gone");
	db_close(d);
	wipe();
}

static void fresh(void)
{
	db *d;

	printf("a new handheld, and no database:\n");
	d = restart(NULL);
	ck(mp4keys_migrate(d) == 0, "nothing to move");
	ck(!db_has(d, "mp4.mode") && !db_has(d, "mp4.show") && !db_has(d, "mp4.look"),
	   "nothing made under mp4.");
	ck(mp4keys_migrate(NULL) == 0, "no database: nothing, and no crash");
	db_close(d);
	wipe();
}

static void many(void)
{
	char k[32], v[32];
	db *d;
	int i, ok = 1;

	printf("more than one pass's worth:\n");
	d = restart(NULL);
	for (i = 0; i < 70; i++) {
		snprintf(k, sizeof k, "muse.k%02d", i);
		snprintf(v, sizeof v, "%d", i);
		db_set_str(d, k, v);
	}
	ck(mp4keys_migrate(d) == 70, "all 70 moved");
	for (i = 0; i < 70; i++) {
		snprintf(k, sizeof k, "mp4.k%02d", i);
		snprintf(v, sizeof v, "%d", i);
		ok &= is(d, k, v);
	}
	ck(ok, "each with its value");
	ck(old_left(d) == 0, "none left");
	db_close(d);
	wipe();
}

/* A shipped default under either name would be seeded by the open, before
 * the move, and win over the player's own value as "the new one there" */
static void defaults(void)
{
	const db_default *t;
	size_t n, i;
	int s, clean = 1;

	printf("the shipped defaults:\n");
	for (s = DB_DEVICE; s <= DB_LIBRARY; s++) {
		t = db_defaults((db_scope)s, &n);
		for (i = 0; t && i < n; i++)
			if (!strncmp(t[i].key, "muse.", 5) || !strncmp(t[i].key, MP4KEYS_NEW, strlen(MP4KEYS_NEW)))
				clean = 0;
	}
	ck(clean, "none under muse. or mp4.");
}

int main(void)
{
	if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
	snprintf(DEV, sizeof DEV, "%s/device.db", dir);
	upgrade();
	step_back();
	fresh();
	many();
	defaults();
	rmdir(dir);
	printf("mp4keys: %d of %d checks pass\n", checks - fails, checks);
	return fails ? 1 : 0;
}
