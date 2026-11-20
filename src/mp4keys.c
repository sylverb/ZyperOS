/* SPDX-License-Identifier: GPL-3.0-only */
/* See mp4keys.h. */
#include "mp4keys.h"

#include <stdio.h>
#include <string.h>

/* A pass's keys: far more than were ever written (four), and a pass that
 * fills it is followed by another */
#define PASS 32

typedef struct {
	char key[PASS][96];
	int  n;
} found;

static bool note(const char *key, const char *value, void *ctx)
{
	found *f = ctx;

	(void)value;
	if (strlen(key) >= sizeof f->key[0]) return true;
	snprintf(f->key[f->n++], sizeof f->key[0], "%s", key);
	return f->n < PASS;
}

int mp4keys_migrate(db *dev)
{
	static found f;
	char to[sizeof f.key[0] + sizeof MP4KEYS_NEW];
	int i, moved = 0, gone;

	if (!dev) return 0;
	/* The keys first, then the moves: no write while the walk is open */
	do {
		f.n = 0;
		gone = 0;
		db_each_prefix(dev, MP4KEYS_OLD, note, &f);
		for (i = 0; i < f.n; i++) {
			snprintf(to, sizeof to, MP4KEYS_NEW "%s", f.key[i] + strlen(MP4KEYS_OLD));
			if (db_has(dev, to)) gone += db_del(dev, f.key[i]);
			else if (db_key_move(dev, f.key[i], to)) gone++, moved++;
		}
	} while (f.n == PASS && gone > 0);
	return moved;
}
