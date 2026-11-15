/* SPDX-License-Identifier: GPL-3.0-only */
/* The core options a game is sent (src/coreopts.h): the player's own rows,
 * then the device's defaults, then every device's, in the library's order;
 * and the one-time cleanup of what 1.0.5 seeded into the library, which
 * keeps every row the player changed. The keys and values are written out
 * here rather than read from the tables, so a typo in either - a name the
 * core never asks for is no default at all, and fails silently on the
 * device - has to be made twice to pass. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/coreopts.h"

static int fails, checks;

static void ck(int cond, const char *what)
{
	checks++;
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

#define MAX 96

/* What `tag`'s games are sent, the Pixel 2's defaults under `rows` (pairs,
 * ending in NULL), as one line, ";" between */
static void sent(const char *const *rows, const dev_default *dev, const char *tag, char *out, size_t n)
{
	static coreopt set[MAX];
	const char *kv;
	int k = 0, i;

	for (i = 0; rows && rows[i]; i += 2) coreopt_add(set, &k, MAX, rows[i], rows[i + 1]);
	coreopt_defaults(set, &k, MAX, dev);
	out[0] = '\0';
	for (i = 0; coreopt_nth(set, k, tag, i, &kv); i++) {
		strncat(out, kv, n - strlen(out) - 1);
		strncat(out, ";", n - strlen(out) - 1);
	}
}

static int has(const char *line, const char *kv)
{
	char want[256];

	snprintf(want, sizeof want, "%s;", kv);
	return strstr(line, want) != NULL;
}

static void merge(void)
{
	static char line[8192];
	static const char *const mine[] = { "coreopt..mgba_frameskip", "disabled",
	                                    "coreopt.PSP.ppsspp_frameskip", "1", NULL };

	printf("the defaults under the player's own:\n");
	sent(NULL, dev_coreopts_pixel2, "GB", line, sizeof line);
	ck(has(line, "mgba_frameskip=auto") && has(line, "snes9x_2010_frameskip=auto") &&
	   has(line, "genesis_plus_gx_frameskip=auto") && has(line, "pcsx_rearmed_frameskip_type=auto") &&
	   has(line, "picodrive_frameskip=auto"), "fast forward's frameskip, the cores' automatic one");
	ck(has(line, "mgba_gb_model=Game Boy") && has(line, "mgba_gb_colors=DMG Green"),
	   "the Game Boy's model for the GB");
	sent(NULL, dev_coreopts_pixel2, "GBC", line, sizeof line);
	ck(!strstr(line, "mgba_gb_model"), "and not for the Game Boy Color");
	sent(NULL, dev_coreopts_pixel2, "PSP", line, sizeof line);
	ck(has(line, "ppsspp_frameskip=disabled") && has(line, "ppsspp_auto_frameskip=disabled") &&
	   has(line, "ppsspp_frame_duplication=disabled"), "the PSP draws every frame (the Pixel 2's)");
	sent(mine, dev_coreopts_pixel2, "PSP", line, sizeof line);
	ck(has(line, "ppsspp_frameskip=1") && !has(line, "ppsspp_frameskip=disabled"),
	   "a player's PSP frameskip over the device's");
	sent(mine, dev_coreopts_pixel2, "GBA", line, sizeof line);
	ck(has(line, "mgba_frameskip=disabled") && !has(line, "mgba_frameskip=auto"),
	   "a player's mGBA frameskip over every device's");
	sent(NULL, NULL, "PSP", line, sizeof line);
	ck(!strstr(line, "ppsspp_") && has(line, "mgba_frameskip=auto"),
	   "no device's own: every device's alone (the Brick's today)");
	sent(NULL, dev_coreopts_pixel2, "N64", line, sizeof line);
	ck(has(line, "parallel-n64-pak1=rumble") && has(line, "mupen64plus-EnableFBEmulation=False"),
	   "the N64's on the Pixel 2");
	{
		static const dev_default dev[] = { { "coreopt..picodrive_aspect", "PAR" }, { NULL, NULL } };

		sent(NULL, dev, "32X", line, sizeof line);
		ck(has(line, "picodrive_aspect=PAR") && !has(line, "picodrive_aspect=CRT"),
		   "a device's default over every device's of the same key");
	}
}

static void order(void)
{
	static coreopt set[MAX];
	const char *kv;
	int k = 0, i, ok = 1;

	printf("in the library's order:\n");
	coreopt_add(set, &k, MAX, "coreopt.SFC.zzz", "1");
	coreopt_add(set, &k, MAX, "coreopt..aaa", "1");
	coreopt_defaults(set, &k, MAX, dev_coreopts_pixel2);
	for (i = 1; i < k; i++) {
		char a[256], b[256];

		snprintf(a, sizeof a, "%s.%s", set[i - 1].tag, set[i - 1].kv);
		snprintf(b, sizeof b, "%s.%s", set[i].tag, set[i].kv);
		*strchr(a, '=') = '\0';
		*strchr(b, '=') = '\0';
		if (strcmp(a, b) >= 0) ok = 0;
	}
	ck(ok, "sorted by key, as the library's scan is");
	ck(coreopt_nth(set, k, "SFC", 0, &kv) && !strcmp(kv, "aaa=1"), "every system's first");
	for (i = 0; coreopt_nth(set, k, "SFC", i + 1, NULL); i++) {}
	ck(coreopt_nth(set, k, "SFC", i, &kv) && !strcmp(kv, "zzz=1"), "then the system's own, last");
	ck(!coreopt_nth(set, k, "SFC", i + 1, &kv), "and nothing past it");
}

static void refused(void)
{
	static coreopt set[MAX];
	char big[300];
	int k = 0, i;

	printf("refused whole:\n");
	memset(big, 'x', sizeof big - 1);
	big[sizeof big - 1] = '\0';
	ck(!coreopt_add(set, &k, MAX, "coreopt..long", big) && k == 0, "a pair too long for its room");
	ck(!coreopt_add(set, &k, MAX, "coreopt.LONGTAGS.opt", "1") && k == 0, "a tag of 8 characters");
	ck(!coreopt_add(set, &k, MAX, "coreopt.notag", "1") && k == 0, "a key with no tag");
	ck(!coreopt_add(set, &k, MAX, "turbo.GB", "1") && k == 0, "a key that is no core option");
	ck(coreopt_add(set, &k, MAX, "coreopt.GB.a", "1") && !coreopt_add(set, &k, MAX, "coreopt.GB.a", "2") && k == 1,
	   "and the same option twice is the first");
	/* A full set keeps what it has, the player's rows */
	k = 0;
	for (i = 0; i < 8; i++) {
		char key[32];

		snprintf(key, sizeof key, "coreopt..mine%d", i);
		coreopt_add(set, &k, 8, key, "1");
	}
	coreopt_defaults(set, &k, 8, dev_coreopts_pixel2);
	ck(k == 8 && !strncmp(set[0].kv, "mine", 4) && !strncmp(set[7].kv, "mine", 4),
	   "a full set keeps the player's rows and drops the defaults");
}

/* A library as 1.0.5 seeded it, every row of both tables, and two the
 * player changed */
static void migrate(void)
{
	char dir[] = "/tmp/zyperos-coreopts-XXXXXX", path[96], v[64];
	const dev_default *t;
	db *lib;
	int left_seeded = 0;

	printf("the one-time cleanup of 1.0.5's rows:\n");
	if (!mkdtemp(dir)) { ck(0, "a scratch folder"); return; }
	snprintf(path, sizeof path, "%s/library.db", dir);
	lib = db_open(path, DB_LIBRARY);
	if (!lib) { ck(0, "the library opens"); return; }
	for (t = coreopt_shared(); t->key; t++) db_set_str(lib, t->key, t->value);
	for (t = dev_coreopts_pixel2; t->key; t++) db_set_str(lib, t->key, t->value);
	db_set_str(lib, "coreopt..mgba_frameskip", "disabled");
	db_set_str(lib, "coreopt.N64.parallel-n64-pak1", "memory");
	db_set_str(lib, "coreopt.NES.fceumm_palette", "nintendo-vc");
	ck(coreopts_migrate(lib), "it runs on a card from 1.0.5");
	for (t = coreopt_shared(); t->key; t++)
		if (strcmp(t->key, "coreopt..mgba_frameskip") && db_has(lib, t->key)) left_seeded++;
	for (t = dev_coreopts_pixel2; t->key; t++)
		if (strcmp(t->key, "coreopt.N64.parallel-n64-pak1") && db_has(lib, t->key)) left_seeded++;
	ck(left_seeded == 0, "every row still at its seeded value is out");
	db_get_str(lib, "coreopt..mgba_frameskip", v, sizeof v, "");
	ck(!strcmp(v, "disabled"), "a default the player changed stays");
	db_get_str(lib, "coreopt.N64.parallel-n64-pak1", v, sizeof v, "");
	ck(!strcmp(v, "memory"), "and a Pixel 2 one the player changed");
	db_get_str(lib, "coreopt.NES.fceumm_palette", v, sizeof v, "");
	ck(!strcmp(v, "nintendo-vc"), "and one that was never a default");
	ck(db_get_int(lib, COREOPTS_MIGRATED_KEY, 0) == 1, "and the card says it was done");
	db_set_str(lib, "coreopt..picodrive_aspect", "CRT");
	ck(!coreopts_migrate(lib), "a second start does nothing");
	ck(db_has(lib, "coreopt..picodrive_aspect"), "so a value set after it stays, even a default's");
	ck(!coreopts_migrate(NULL), "and no library is nothing to do");
	db_close(lib);
	{
		const char *suffix[] = { "", "-wal", "-shm" };
		char p[128];
		unsigned i;

		for (i = 0; i < 3; i++) {
			snprintf(p, sizeof p, "%s%s", path, suffix[i]);
			unlink(p);
		}
		rmdir(dir);
	}
}

int main(void)
{
	merge();
	order();
	refused();
	migrate();
	if (fails) {
		printf("coreopts: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("coreopts: %d checks passed\n", checks);
	return 0;
}
