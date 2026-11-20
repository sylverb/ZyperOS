/* SPDX-License-Identifier: GPL-3.0-only */
/* Your PICO-8's Shader and Border, the plain part (src/p8fx.h): the look
 * file, the shaders PICO-8 is offered and their steps, Next Shader on a
 * scratch database, PICO-8's picture out of its preblit, and the rect that
 * covers the screen. make check-p8fx runs it from tools/p8fx-check.py,
 * which sets the list beside the launcher's SHADERS (src/app.h).
 *
 *     build-native/p8fx-check          the checks
 *     build-native/p8fx-check --list   the list, one name a line */
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/p8fx.h"

static int fails, checks;

#define CHECK(cond, what) do { \
	checks++; \
	if (!(cond)) { fails++; printf("  FAIL %s\n", what); } \
} while (0)

static void put(const char *path, const char *text)
{
	FILE *f = fopen(path, "w");

	if (f) {
		fputs(text, f);
		fclose(f);
	}
}

static void slurp(const char *path, char *out, size_t n)
{
	FILE *f = fopen(path, "r");
	size_t got = f ? fread(out, 1, n - 1, f) : 0;

	out[got] = '\0';
	if (f) fclose(f);
}

static void look_file(const char *dir)
{
	char path[512], tmp[600], text[512];
	p8fx_look l, r;

	snprintf(path, sizeof path, "%s/look", dir);
	p8fx_look_default(&l);
	CHECK(!strcmp(l.shader, "nearest") && l.strength == 100 && l.border == P8FX_BORDER_OFF && !l.border_file[0],
	      "the default: Off, 100%, no border");
	CHECK(!p8fx_look_on(&l), "...which changes nothing");
	CHECK(!p8fx_look_read(path, &r) && !strcmp(r.shader, "nearest") && r.strength == 100,
	      "no file: false, and the default");

	snprintf(l.shader, sizeof l.shader, "crt-curved");
	l.strength = 70;
	l.border = P8FX_BORDER_PICTURE;
	snprintf(l.border_file, sizeof l.border_file, "/tmp/zyperos-pico8-border.dtov");
	CHECK(p8fx_look_write(path, &l), "a look written");
	snprintf(tmp, sizeof tmp, "%s.new", path);
	CHECK(access(tmp, F_OK) != 0, "...whole, by a rename: no .new left");
	slurp(path, text, sizeof text);
	CHECK(!strcmp(text, "shader crt-curved\nstrength 70\nborder picture\nborder_file /tmp/zyperos-pico8-border.dtov\n"),
	      "...one \"key value\" line each");
	CHECK(p8fx_look_read(path, &r) && !strcmp(r.shader, "crt-curved") && r.strength == 70 &&
	      r.border == P8FX_BORDER_PICTURE && !strcmp(r.border_file, "/tmp/zyperos-pico8-border.dtov"),
	      "...and read back as it was");
	CHECK(p8fx_look_on(&r), "...which changes the picture");

	l.strength = 0;
	l.border = P8FX_BORDER_BLUR;
	l.border_file[0] = '\0';
	snprintf(l.shader, sizeof l.shader, "lcd");
	CHECK(p8fx_look_write(path, &l) && p8fx_look_read(path, &r) && r.strength == 100 &&
	      r.border == P8FX_BORDER_BLUR && !r.border_file[0], "a strength never set is written 100; no border_file line");
	l.border = P8FX_BORDER_OFF;
	snprintf(l.shader, sizeof l.shader, "nearest");
	CHECK(p8fx_look_write(path, &l) && p8fx_look_read(path, &r) && !p8fx_look_on(&r), "Off and no border: nothing on");

	put(path, "# a hand edit\nshader scanlines\r\nstrength 5\nborder bogus\nwhat ever\nnoline\nshader\n");
	CHECK(p8fx_look_read(path, &r) && !strcmp(r.shader, "scanlines") && r.strength == 100 &&
	      r.border == P8FX_BORDER_OFF, "lines it does not know passed over, a strength out of range and a "
	      "border of none the default's, a CR ignored");
	put(path, "strength 105\nborder blur\nshader aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
	CHECK(p8fx_look_read(path, &r) && r.strength == 100 && r.border == P8FX_BORDER_BLUR &&
	      !strcmp(r.shader, "nearest"), "a shader name too long for the field: Off");
	put(path, "strength abc\n");
	CHECK(p8fx_look_read(path, &r) && r.strength == 100, "a strength that is no number: 100");
	put(path, "strength 10\n");
	CHECK(p8fx_look_read(path, &r) && r.strength == 10, "10%, the least");
	CHECK(!p8fx_look_write("/nonexistent-dir/look", &l), "a file that cannot be written: false");

	CHECK(!strcmp(p8fx_border_word(P8FX_BORDER_OFF), "off") && !strcmp(p8fx_border_word(P8FX_BORDER_BLUR), "blur") &&
	      !strcmp(p8fx_border_word(P8FX_BORDER_PICTURE), "picture") && !strcmp(p8fx_border_word((p8fx_border)9), "off"),
	      "the border words, and Off's for one that is none");
	CHECK(p8fx_border_parse("picture") == P8FX_BORDER_PICTURE && p8fx_border_parse(NULL) == P8FX_BORDER_OFF &&
	      p8fx_border_parse("Blur") == P8FX_BORDER_OFF, "...read back, exactly");
}

static void shaders(void)
{
	const char *names[16];
	char out[32];
	int n = p8fx_shaders(names, 16), i;
	static const char *const WANT[] = { "nearest", "shimmerless", "crt", "crt-curved", "scanlines", "lcd" };

	CHECK(n == 6, "six shaders on a GPU device");
	for (i = 0; i < n && i < 6; i++) CHECK(!strcmp(names[i], WANT[i]), "in SHADERS' order");
	for (i = 0; i < n; i++) CHECK(strcmp(names[i], "lcd-unlit") != 0, "never LCD No Backlight, the Game Boys'");
	CHECK(p8fx_shaders(names, 2) == 2, "no more than asked for");
	n = p8fx_shaders_on(false, names, 16);
	CHECK(n == 2 && !strcmp(names[0], "nearest") && !strcmp(names[1], "shimmerless"),
	      "with no GPU for the old screens: Off and Sharp-Shimmerless");

	p8fx_shader_step(true, "nearest", +1, out, sizeof out);
	CHECK(!strcmp(out, "shimmerless"), "Off, then Sharp-Shimmerless");
	p8fx_shader_step(true, "lcd", +1, out, sizeof out);
	CHECK(!strcmp(out, "nearest"), "LCD Grid, then Off: round");
	p8fx_shader_step(true, "nearest", -1, out, sizeof out);
	CHECK(!strcmp(out, "lcd"), "back from Off: LCD Grid");
	p8fx_shader_step(true, "vhs", +1, out, sizeof out);
	CHECK(!strcmp(out, "shimmerless"), "a name of none steps from Off");
	p8fx_shader_step(true, NULL, +1, out, sizeof out);
	CHECK(!strcmp(out, "shimmerless"), "...and none at all");
	p8fx_shader_step(false, "shimmerless", +1, out, sizeof out);
	CHECK(!strcmp(out, "nearest"), "with no GPU, round the two");

	CHECK(p8fx_shader_text("crt") == S_GAMEMENU_SHADER_CRT && p8fx_shader_text("lcd") == S_GAMEMENU_SHADER_LCD &&
	      p8fx_shader_text("crt-curved") == S_GAMEMENU_SHADER_CRT_CURVED &&
	      p8fx_shader_text("scanlines") == S_GAMEMENU_SHADER_SCANLINES &&
	      p8fx_shader_text("shimmerless") == S_GAMEMENU_SHADER_SHIMMERLESS &&
	      p8fx_shader_text("nearest") == S_MENU_OFF_TITLE && p8fx_shader_text("vhs") == S_MENU_OFF_TITLE &&
	      p8fx_shader_text(NULL) == S_MENU_OFF_TITLE, "each shader's words, Off's for one that is none");
}

static sqlite3 *scratch_db(const char *path)
{
	sqlite3 *d = NULL;

	unlink(path);
	if (sqlite3_open(path, &d) != SQLITE_OK) return NULL;
	sqlite3_exec(d, "CREATE TABLE settings(key TEXT PRIMARY KEY, value TEXT);", NULL, NULL, NULL);
	return d;
}

static void set(sqlite3 *d, const char *key, const char *value)
{
	char sql[256];

	snprintf(sql, sizeof sql, "INSERT OR REPLACE INTO settings(key,value) VALUES('%s','%s');", key, value);
	sqlite3_exec(d, sql, NULL, NULL, NULL);
}

static void get(sqlite3 *d, const char *key, char *out, size_t n)
{
	sqlite3_stmt *st;

	out[0] = '\0';
	if (sqlite3_prepare_v2(d, "SELECT value FROM settings WHERE key=?;", -1, &st, NULL) != SQLITE_OK) return;
	sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
	if (sqlite3_step(st) == SQLITE_ROW) snprintf(out, n, "%s", (const char *)sqlite3_column_text(st, 0));
	sqlite3_finalize(st);
}

static void next_shader(const char *dir)
{
	char db[512], look[512], name[32], v[32], text[512];
	sqlite3 *d;
	p8fx_look l;

	snprintf(db, sizeof db, "%s/zyperos.db", dir);
	snprintf(look, sizeof look, "%s/next-look", dir);
	unlink(look);
	if (!(d = scratch_db(db))) {
		CHECK(false, "a scratch database");
		return;
	}
	CHECK(p8fx_next_shader_at(d, look, name, sizeof name) && !strcmp(name, "shimmerless"),
	      "Next Shader with nothing kept: Off, then Sharp-Shimmerless");
	get(d, "shader.PICO8", v, sizeof v);
	CHECK(!strcmp(v, "shimmerless"), "...kept as shader.PICO8");
	CHECK(access(look, F_OK) != 0, "...and no look file made where the launcher made none");

	d = (sqlite3_close(d), scratch_db(db));
	set(d, "shader", "crt");
	CHECK(p8fx_next_shader_at(d, look, name, sizeof name) && !strcmp(name, "crt-curved"),
	      "with the old every-system key on CRT: CRT Curved, as the launcher reads it");
	set(d, "shader.PICO8", "lcd");
	CHECK(p8fx_next_shader_at(d, look, name, sizeof name) && !strcmp(name, "nearest"),
	      "with shader.PICO8 on LCD Grid: Off, round");

	p8fx_look_default(&l);
	snprintf(l.shader, sizeof l.shader, "crt");
	l.strength = 50;
	l.border = P8FX_BORDER_PICTURE;
	snprintf(l.border_file, sizeof l.border_file, "/x.dtov");
	p8fx_look_write(look, &l);
	CHECK(p8fx_next_shader_at(d, look, name, sizeof name) && !strcmp(name, "crt-curved"),
	      "with a look file: from the shader drawn now (CRT), whatever the key says");
	slurp(look, text, sizeof text);
	CHECK(!strcmp(text, "shader crt-curved\nstrength 50\nborder picture\nborder_file /x.dtov\n"),
	      "...the file's shader line alone changed");
	get(d, "shader.PICO8", v, sizeof v);
	CHECK(!strcmp(v, "crt-curved"), "...and the key kept");
	CHECK(!p8fx_next_shader_at(NULL, look, name, sizeof name), "no database: false");
	slurp(look, text, sizeof text);
	CHECK(strstr(text, "shader crt-curved\n") == text, "...and the file as it was");
	sqlite3_close(d);
	unlink(db);
	unlink(look);
}

static void shrink(void)
{
	static uint32_t src[512 * 512], small[128 * 128], out[128 * 128];
	int x, y, k;

	for (y = 0; y < 128; y++)
		for (x = 0; x < 128; x++) small[y * 128 + x] = 0xff000000u | (uint32_t)(y * 131 + x * 7);
	CHECK(p8fx_shrink(small, 128 * 4, 128, 128, (uint8_t *)out) && !memcmp(out, small, sizeof out),
	      "128x128: the picture as it is");
	/* 3x, each row padded to 400 pixels */
	for (y = 0; y < 384; y++)
		for (x = 0; x < 400; x++) src[y * 400 + x] = x < 384 ? small[(y / 3) * 128 + x / 3] : 0xdeadbeefu;
	memset(out, 0, sizeof out);
	CHECK(p8fx_shrink(src, 400 * 4, 384, 384, (uint8_t *)out) && !memcmp(out, small, sizeof out),
	      "PICO-8's 3x preblit, its rows padded: the 128x128 it was made from");
	for (k = 2; k <= 4; k++) {
		for (y = 0; y < 128 * k; y++)
			for (x = 0; x < 128 * k; x++) src[y * 128 * k + x] = small[(y / k) * 128 + x / k];
		memset(out, 0, sizeof out);
		CHECK(p8fx_shrink(src, 128 * k * 4, 128 * k, 128 * k, (uint8_t *)out) && !memcmp(out, small, sizeof out),
		      "each whole preblit, 2x to 4x");
	}
	memset(out, 7, sizeof out);
	CHECK(!p8fx_shrink(src, 400 * 4, 384, 256, (uint8_t *)out) && !p8fx_shrink(src, 130 * 4, 130, 130, (uint8_t *)out) &&
	      !p8fx_shrink(NULL, 512, 128, 128, (uint8_t *)out) && !p8fx_shrink(src, 100, 128, 128, (uint8_t *)out),
	      "any other shape, size or pitch: false");
	CHECK(((uint8_t *)out)[0] == 7, "...and the picture untouched");
}

static void covers(void)
{
	CHECK(p8fx_covers(0, 0, 640, 480, 640, 480), "Full Screen covers 640x480");
	CHECK(p8fx_covers(0, -80, 640, 640, 640, 480), "Zoomed and Edges Cut (0,-80, 640x640) cover it");
	CHECK(!p8fx_covers(80, 0, 480, 480, 640, 480), "Original Shape leaves room");
	CHECK(!p8fx_covers(128, 48, 384, 384, 640, 480), "Sharp Pixels leaves room");
	CHECK(!p8fx_covers(-1, 0, 640, 480, 640, 480), "one pixel short at the right: room");
}

int main(int argc, char **argv)
{
	char dir[] = "/tmp/zyperos-p8fx-XXXXXX";

	if (argc == 2 && !strcmp(argv[1], "--list")) {
		const char *names[16];
		int n = p8fx_shaders(names, 16), i;

		for (i = 0; i < n; i++) printf("%s\n", names[i]);
		return 0;
	}
	if (!mkdtemp(dir)) return 1;
	printf("the look file:\n");
	look_file(dir);
	printf("the shaders:\n");
	shaders();
	printf("Next Shader:\n");
	next_shader(dir);
	printf("the picture and the rect:\n");
	shrink();
	covers();
	{
		char cmd[600];

		snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
		if (system(cmd) != 0) printf("  (could not remove %s)\n", dir);
	}
	if (fails) {
		printf("p8fx: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("p8fx: %d checks passed\n", checks);
	return 0;
}
