/* SPDX-License-Identifier: GPL-3.0-only */
/* Next Shader over your PICO-8, from portkeys (src/p8fx.h): the device
 * database's shader.PICO8 stepped and the look file told. SQLite's own
 * calls, as portkeys reads its settings (tools/pkset.c), since portkeys has
 * none of the launcher's db.c. */
#include "p8fx.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

/* A text setting, "" for a key the database does not hold; false when it
 * could not be asked */
static bool setting(sqlite3 *d, const char *key, char *out, size_t n)
{
	sqlite3_stmt *st;
	int rc;

	out[0] = '\0';
	if (!d || sqlite3_prepare_v2(d, "SELECT value FROM settings WHERE key=?;", -1, &st, NULL) != SQLITE_OK)
		return false;
	sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
	rc = sqlite3_step(st);
	if (rc == SQLITE_ROW) {
		const unsigned char *v = sqlite3_column_text(st, 0);

		snprintf(out, n, "%s", v ? (const char *)v : "");
	}
	sqlite3_finalize(st);
	return rc == SQLITE_ROW || rc == SQLITE_DONE;
}

/* INSERT OR REPLACE, as src/db.c's db_set_str writes a setting */
static bool keep(sqlite3 *d, const char *key, const char *value)
{
	sqlite3_stmt *st;
	int rc;

	if (!d || sqlite3_prepare_v2(d, "INSERT OR REPLACE INTO settings(key,value) VALUES(?,?);", -1, &st,
	                             NULL) != SQLITE_OK)
		return false;
	sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
	sqlite3_bind_text(st, 2, value, -1, SQLITE_STATIC);
	rc = sqlite3_step(st);
	sqlite3_finalize(st);
	return rc == SQLITE_DONE;
}

bool p8fx_next_shader_at(sqlite3 *dev, const char *file, char *name, size_t n)
{
	char now[32] = "", key[32];
	p8fx_look l;
	bool have = p8fx_look_read(file, &l);

	/* What is drawn: the file's, which the launcher wrote from the keys
	 * below; else the keys as the launcher reads them (display_load) */
	if (have) snprintf(now, sizeof now, "%s", l.shader);
	snprintf(key, sizeof key, "shader.%s", P8FX_TAG);
	if (!have && (!setting(dev, key, now, sizeof now) || !now[0]))
		setting(dev, "shader", now, sizeof now);
	p8fx_shader_step(true, now, +1, name, n);
	if (!keep(dev, key, name)) {
		fprintf(stderr, "p8fx: Next Shader: %s not kept: %s\n", key, dev ? sqlite3_errmsg(dev) : "no database");
		return false;
	}
	/* The file only where the launcher made one: with none, no shim is
	 * drawing, and the next start reads the key */
	if (have) {
		snprintf(l.shader, sizeof l.shader, "%s", name);
		if (!p8fx_look_write(file, &l)) fprintf(stderr, "p8fx: Next Shader: %s not written\n", file);
	}
	return true;
}

bool p8fx_next_shader(sqlite3 *dev, char *name, size_t n)
{
	return p8fx_next_shader_at(dev, P8FX_FILE, name, n);
}
