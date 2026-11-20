/* SPDX-License-Identifier: GPL-3.0-only */
/* See hkstore.h. */
#include "hkstore.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "db.h"
#include "library.h"

/* As long as a game's whole key, as remapstore's */
#define KEY_MAX (CFG_STR + LIB_PATH + 16)

static void key_of(hks_scope scope, const char *tag, const char *file, char *out, size_t n)
{
	if (scope == HKS_GAME)        snprintf(out, n, "hotkeys.%s.%s", tag ? tag : "", file ? file : "");
	else if (scope == HKS_SYSTEM) snprintf(out, n, "hotkeys.%s", tag ? tag : "");
	else                          snprintf(out, n, "hotkeys");
}

void hks_game_key(const char *tag, const char *file, char *out, int n)
{
	key_of(HKS_GAME, tag, file, out, n > 0 ? (size_t)n : 0);
}

/* The scope's own table; false, out untouched, with no key or one this
 * release cannot read (said once, and read as no key) */
static bool own(hks_scope scope, const char *tag, const char *file, hk_table *out)
{
	char key[KEY_MAX], v[160];
	hk_table t;

	key_of(scope, tag, file, key, sizeof key);
	if (!db_get_str(db_lib(), key, v, sizeof v, NULL)) return false;
	if (!hk_load(v, &t)) {
		static char said[KEY_MAX];

		if (strcmp(said, key)) fprintf(stderr, "hotkeys: %s: not read, as none\n", key);
		snprintf(said, sizeof said, "%s", key);
		return false;
	}
	*out = t;
	return true;
}

void hks_fallback(hks_scope scope, const char *tag, const char *file, hk_table *out)
{
	hk_default(out);
	if (scope == HKS_EVERY) return;
	if (own(HKS_EVERY, NULL, NULL, out) && scope == HKS_SYSTEM) return;
	if (scope == HKS_GAME) own(HKS_SYSTEM, tag, NULL, out);
	(void)file;
}

bool hks_get(hks_scope scope, const char *tag, const char *file, hk_table *out)
{
	hks_fallback(scope, tag, file, out);
	return own(scope, tag, file, out);
}

void hks_put(hks_scope scope, const char *tag, const char *file, const hk_table *t)
{
	char key[KEY_MAX], v[160];
	hk_table back;

	key_of(scope, tag, file, key, sizeof key);
	hks_fallback(scope, tag, file, &back);
	if (hk_equal(&back, t)) {
		db_del(db_lib(), key);
		return;
	}
	hk_store(t, v, sizeof v);
	/* ZyperOS's own under a scope that falls back to another table holds
	 * no pairs, and still holds its own */
	db_set_str(db_lib(), key, v[0] ? v : "1;");
}

void hks_reset(hks_scope scope, const char *tag, const char *file)
{
	char key[KEY_MAX];

	key_of(scope, tag, file, key, sizeof key);
	db_del(db_lib(), key);
}
