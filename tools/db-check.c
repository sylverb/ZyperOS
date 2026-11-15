/* Does the settings store keep its promises?
 *
 * Fourteen config files became two databases, and the properties that used to
 * be obvious from looking at a text file now have to be asserted. Chiefly:
 *
 *   - a shipped default never overwrites a choice the player made. That is not
 *     hypothetical - zyperos.cfg was applied over the top of levels.cfg at every
 *     boot until 2026-08, which put the config ahead of the player.
 *   - a missing or unreadable database comes back as a working one, because
 *     the defaults are compiled in and there is no .db in the payload.
 *   - a value that is not a number is not a zero.
 *   - the two scopes stay apart. Flattening them would move a session token
 *     between handhelds, which is the one thing the old layout was careful of.
 *   - the games table exists on the card and nowhere else, and a row of scraped
 *     text comes back exactly as it went in.
 *
 * Links src/db.c and NOT SDL. If it ever needs SDL, the split has failed.
 */
#include "../src/db.h"
#include "../src/strength.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;

static void ck(int cond, const char *what)
{
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

static char test_dir[] = "/tmp/zyperos-db-check-XXXXXX";
static char DEV[sizeof test_dir + sizeof "/device.db"];
static char LIB[sizeof test_dir + sizeof "/library.db"];
static char BOOT[sizeof test_dir + sizeof "/boot.env"];

static void scrub(void)
{
	const char *suffix[] = { "", "-wal", "-shm" };
	char p[256];
	for (unsigned i = 0; i < 3; i++) {
		snprintf(p, sizeof p, "%s%s", DEV, suffix[i]); unlink(p);
		snprintf(p, sizeof p, "%s%s", LIB, suffix[i]); unlink(p);
	}
}

static void cleanup(void)
{
	char path[256];

	db_shutdown();
	scrub();
	snprintf(path, sizeof path, "%s.dump", DEV); unlink(path);
	unlink(BOOT);
	snprintf(path, sizeof path, "%s.new", BOOT); unlink(path);
	rmdir(test_dir);
}

static void opens_and_seeds(void)
{
	db *d;
	char buf[64];

	printf("a fresh database arrives with the shipped defaults:\n");
	d = db_open(DEV, DB_DEVICE);
	ck(d != NULL, "it opens where nothing existed");
	if (!d) return;

	/* A rung, not the percentage config/zyperos.cfg shipped: one key, one unit. */
	ck(db_get_int(d, "volume", -1) == 8, "volume seeded as a rung");
	ck(db_get_int(d, "brightness", -1) == 7, "brightness seeded");
	db_get_str(d, "audioout", buf, sizeof buf, "");
	ck(!strcmp(buf, "auto"), "audioout seeded to auto");
	/* Two minutes, which is what auto_off_load() returned when no file
	 * existed. Seeding 0 here would quietly change the shipped default to
	 * "never", which is exactly what it did the first time. */
	ck(db_get_int(d, "autooff", -1) == 120, "auto off seeded to two minutes");
	db_close(d);
}

static void a_default_never_beats_a_choice(void)
{
	db *d;

	printf("a default never overwrites a choice:\n");
	d = db_open(DEV, DB_DEVICE);
	ck(db_set_int(d, "volume", 12), "the player turns it down");
	db_close(d);

	d = db_open(DEV, DB_DEVICE);              /* seeds again on every open */
	ck(db_get_int(d, "volume", -1) == 12, "and it is still 12 after a reopen");
	ck(db_get_int(d, "brightness", -1) == 7, "while an untouched key keeps its default");
	db_close(d);
}

static void round_trips(void)
{
	db *d = db_open(DEV, DB_DEVICE);
	char buf[64];

	printf("values survive a close and reopen:\n");
	ck(db_set_str(d, "probe.text", "hello world"), "a string with a space writes");
	ck(db_set_int(d, "probe.int", -17), "a negative integer writes");
	db_close(d);

	d = db_open(DEV, DB_DEVICE);
	db_get_str(d, "probe.text", buf, sizeof buf, "");
	ck(!strcmp(buf, "hello world"), "the string comes back whole");
	ck(db_get_int(d, "probe.int", 0) == -17, "so does the integer");

	printf("a value that is not a number is not a zero:\n");
	db_set_str(d, "probe.junk", "seven");
	ck(db_get_int(d, "probe.junk", -99) == -99, "it falls back instead of reading 0");
	db_set_str(d, "probe.trailing", "12x");
	ck(db_get_int(d, "probe.trailing", -99) == -99, "and trailing rubbish is not 12");

	printf("absent keys and deletion:\n");
	ck(!db_has(d, "probe.never"), "a key nobody wrote is absent");
	ck(db_get_int(d, "probe.never", 5) == 5, "and reads as the fallback");
	db_get_str(d, "probe.never", buf, sizeof buf, "spare");
	ck(!strcmp(buf, "spare"), "including for strings");
	ck(db_has(d, "probe.text"), "a key that was written is present");
	ck(db_del(d, "probe.text"), "it deletes");
	ck(!db_has(d, "probe.text"), "and is gone afterwards");
	db_close(d);
}

static void a_lost_database_self_heals(void)
{
	db *d;

	printf("a deleted database comes back working:\n");
	scrub();
	d = db_open(DEV, DB_DEVICE);
	ck(d != NULL, "it reopens from nothing");
	ck(db_get_int(d, "volume", -1) == 8, "with the shipped default, not an empty value");
	ck(!db_has(d, "probe.int"), "and nothing from the old file");
	db_close(d);
}

/* The scraped text: a card's, not a handheld's, and intact on the way back. */
static void the_games_table(void)
{
	db *dev, *lib;
	game_meta m, got;
	size_t i;

	printf("the games table:\n");
	dev = db_open(DEV, DB_DEVICE);
	lib = db_open(LIB, DB_LIBRARY);
	if (!dev || !lib) { ck(0, "both open"); return; }

	memset(&m, 0, sizeof m);
	snprintf(m.year, sizeof m.year, "1993");
	snprintf(m.publisher, sizeof m.publisher, "Sega");
	snprintf(m.developer, sizeof m.developer, "Treasure");
	snprintf(m.players, sizeof m.players, "2");
	snprintf(m.genres, sizeof m.genres, "Action,Shooter");
	snprintf(m.esrb, sizeof m.esrb, "E");
	snprintf(m.note, sizeof m.note, "18");
	/* Right up to the cap, because that is the length nobody tries by hand. */
	for (i = 0; i < sizeof m.synopsis - 1; i++)
		m.synopsis[i] = (char)('a' + (i % 26));

	ck(db_game_set(lib, "Genesis", "Gunstar Heroes (USA).zip", &m),
	   "a row goes into the card's database");
	ck(!db_game_set(dev, "Genesis", "Gunstar Heroes (USA).zip", &m),
	   "and cannot go into the handheld's, which has no such table");

	memset(&got, 0xAB, sizeof got);
	ck(db_game_get(lib, "Genesis", "Gunstar Heroes (USA).zip", &got), "and comes back");
	ck(!strcmp(got.year, "1993") && !strcmp(got.publisher, "Sega") &&
	   !strcmp(got.developer, "Treasure") && !strcmp(got.players, "2") &&
	   !strcmp(got.genres, "Action,Shooter") && !strcmp(got.esrb, "E") &&
	   !strcmp(got.note, "18"), "with every field as written");
	ck(!strcmp(got.synopsis, m.synopsis), "including a synopsis at the cap");

	memset(&got, 0xAB, sizeof got);
	ck(!db_game_get(lib, "Genesis", "a game nobody has.zip", &got),
	   "a game with no row says so");
	ck(got.year[0] == '\0' && got.synopsis[0] == '\0',
	   "and zeroes what it was handed, so a screen can draw it either way");

	/* Scraped twice is one row, not two: the second write replaces. */
	snprintf(m.year, sizeof m.year, "1994");
	ck(db_game_set(lib, "Genesis", "Gunstar Heroes (USA).zip", &m), "a re-scrape writes");
	ck(db_game_get(lib, "Genesis", "Gunstar Heroes (USA).zip", &got) &&
	   !strcmp(got.year, "1994"), "and replaces rather than duplicating");

	db_close(dev);
	db_close(lib);
}

static void the_scopes_stay_apart(void)
{
	db *dev, *lib;
	char buf[64];
	size_t nd, nl, i, j;
	const db_default *dd, *ld;

	printf("the two scopes do not bleed into each other:\n");
	dev = db_open(DEV, DB_DEVICE);
	lib = db_open(LIB, DB_LIBRARY);
	ck(dev && lib, "both open");
	if (!dev || !lib) return;

	db_get_str(lib, "timezone", buf, sizeof buf, "");
	ck(!strcmp(buf, "America/New_York"), "the library scope seeds its own defaults");
	ck(!db_has(dev, "timezone"), "which the device scope does not get");
	ck(!db_has(lib, "volume"), "and the device's do not leak the other way");

	/* A key in both would be ambiguous the first time a card moved between
	 * handhelds, which is exactly the failure the split exists to prevent. */
	dd = db_defaults(DB_DEVICE, &nd);
	ld = db_defaults(DB_LIBRARY, &nl);
	for (i = 0; i < nd; i++)
		for (j = 0; j < nl; j++)
			ck(strcmp(dd[i].key, ld[j].key) != 0,
			   "no key is declared in both scopes");

	/* Writing the same name either side must stay two separate values. */
	db_set_str(dev, "shared.name", "device");
	db_set_str(lib, "shared.name", "library");
	db_get_str(dev, "shared.name", buf, sizeof buf, "");
	ck(!strcmp(buf, "device"), "the same key in both files holds two values");
	db_get_str(lib, "shared.name", buf, sizeof buf, "");
	ck(!strcmp(buf, "library"), "and neither one wins");

	db_close(dev);
	db_close(lib);
}

/* boot.env is the one thing outside this process that depends on the schema,
 * and it is sourced by the script that boots the device. A key it reads from
 * the wrong scope silently exports the fallback forever - which is exactly the
 * bug this caught while it was being written. */
static void boot_env_says_what_the_shell_needs(void)
{
	const char *path = BOOT;
	char line[256];
	int seen_bright = 0, seen_tz = 0, seen_wifi = 0, seen_bt = 0;
	/* seen_bright and seen_tz: neither must be in it any more. Only TrimUI's
	 * launch.sh read the brightness, to set the panel before its boot
	 * animation, and nothing read the timezone: the launcher sets TZ itself,
	 * and Muse inherits it. */
	FILE *f;

	printf("boot.env carries the values launch.sh reads:\n");
	scrub();
	unlink(path);
	ck(db_init(DEV, LIB, path), "both databases open");

	/* Values a shell must see, not the defaults, so a wrong scope shows up. */
	db_set_int(db_dev(), "brightness", 3);
	db_set_int(db_dev(), "wifi", 1);
	db_set_int(db_dev(), "bluetooth", 1);
	db_set_str(db_lib(), "timezone", "Europe/Berlin");

	ck(db_write_boot_env(), "it writes");
	f = fopen(path, "r");
	ck(f != NULL, "and the file is there");
	if (!f) { db_shutdown(); return; }
	while (fgets(line, sizeof line, f)) {
		if (!strncmp(line, "BRIGHTNESS=", 11))          seen_bright = 1;
		if (!strcmp(line, "WIFI='1'\n"))                seen_wifi = 1;
		if (!strcmp(line, "BLUETOOTH='1'\n"))           seen_bt = 1;
		if (!strncmp(line, "TIMEZONE=", 9))            seen_tz = 1;
	}
	fclose(f);
	ck(!seen_bright, "brightness is not in it");
	ck(!seen_tz,     "nor the timezone");
	ck(seen_wifi,   "wifi is the stored value");
	ck(seen_bt,     "bluetooth is the stored value, from the device scope");

	/* The format is a contract with a reader written in another language:
	 * launch.sh's boot_env reads it (each device's, sd/DEVICE/launch.sh). A
	 * quoting change here would break the boot, so it is exercised rather
	 * than eyeballed: the function as launch.sh has it, run by sh. And what a
	 * power cut can leave in the file instead (2026-11-15) - half a line,
	 * another file's old bytes, zeros - reads as off, and the script goes
	 * on: sourced, a syntax error in it ended launch.sh before ZyperOS. */
	{
		static const char *const scripts[] = { "sd/pixel2/launch.sh", "sd/brick/launch.sh" };
		static const char *const junk[] = {
			"WIFI='1\nBLUET", "\x01\x02)(\xff'\"`\n\x7f", "\0\0\0\0\0\0\0\0",
		};
		char cmd[1024], bad[sizeof test_dir + sizeof "/bad.env"];
		FILE *r;
		char got[128];
		size_t i, j;

		snprintf(bad, sizeof bad, "%s/bad.env", test_dir);
		for (i = 0; i < sizeof scripts / sizeof *scripts; i++) {
			snprintf(cmd, sizeof cmd,
			         "sh -c 'eval \"$(sed -n \"/^boot_env()/,/^}$/p\" %s)\"; BOOT_ENV=%s; "
			         "boot_env; printf \"%%s %%s\" \"$WIFI\" \"$BLUETOOTH\"'",
			         scripts[i], path);
			got[0] = '\0';
			r = popen(cmd, "r");
			if (r) { if (!fgets(got, sizeof got, r)) got[0] = '\0'; pclose(r); }
			printf("  %s: \"%s\"\n", scripts[i], got);
			ck(!strcmp(got, "1 1"), "launch.sh's boot_env reads it");

			for (j = 0; j < sizeof junk / sizeof *junk; j++) {
				FILE *b = fopen(bad, "wb");
				size_t len = j == 2 ? 8 : strlen(junk[j]);

				if (b) { fwrite(junk[j], 1, len, b); fclose(b); }
				snprintf(cmd, sizeof cmd,
				         "sh -c 'eval \"$(sed -n \"/^boot_env()/,/^}$/p\" %s)\"; BOOT_ENV=%s; "
				         "boot_env; printf \"%%s %%s after\" \"$WIFI\" \"$BLUETOOTH\"' 2>/dev/null",
				         scripts[i], bad);
				got[0] = '\0';
				r = popen(cmd, "r");
				if (r) { if (!fgets(got, sizeof got, r)) got[0] = '\0'; pclose(r); }
				ck(!strcmp(got, "0 0 after"), "a broken boot.env reads as off, and the script goes on");
			}
		}
		unlink(bad);
	}

	/* Derived, not authoritative: losing it must cost nothing. This is also
	 * the case the unchanged-content memo below could have broken - a deleted
	 * file whose content has not changed still has to come back. */
	unlink(path);
	ck(db_write_boot_env(), "it rewrites after being deleted");
	ck(!access(path, F_OK), "and the file is really there again");

	/* Unchanged content must not rewrite: a rewrite costs a file replacement
	 * (3 ms median, 9 ms at the tail) on whatever path called it. */
	{
		struct stat a, b;
		stat(path, &a);
		sleep(1);                      /* mtime has one-second resolution */
		ck(db_write_boot_env(), "an unchanged write reports success");
		stat(path, &b);
		ck(a.st_mtime == b.st_mtime, "and does not touch the file");

		db_set_int(db_dev(), "wifi", 0);
		ck(db_write_boot_env(), "a changed value writes");
		stat(path, &b);
		ck(a.st_mtime != b.st_mtime, "and does touch it");
	}
	unlink(path);
	db_shutdown();
	ck(db_dev() == NULL && db_lib() == NULL, "shutdown clears both handles");
}

/* Six config files became a namespace rather than a table each, so the prefix
 * scan is what makes them readable at all. The bound matters: "turbo." must
 * not pick up "turbos.x", and an empty middle segment - "coreopt..global" -
 * has to come back like any other. */
static bool collect(const char *key, const char *value, void *ctx)
{
	char *out = ctx;
	strncat(out, key, 200);
	strncat(out, "=", 2);
	strncat(out, value, 200);
	strncat(out, ";", 2);
	return true;
}

static void prefix_scan(void)
{
	db *d;
	char got[512] = { 0 };

	printf("the prefix scan brings back a namespace and nothing else:\n");
	scrub();
	d = db_open(LIB, DB_LIBRARY);
	if (!d) { ck(0, "library opens"); return; }

	/* Its own namespace, not turbo. or coreopt. - those are seeded, and an
	 * assertion written against an empty database would be asserting that the
	 * defaults are absent. That is how this check failed when it was written. */
	db_set_str(d, "zz.NES", "a");
	db_set_str(d, "zz.GB", "b");
	db_set_str(d, "zzs.NOT", "c");        /* one byte past the prefix */
	db_set_str(d, "zz", "d");             /* the prefix minus its dot */
	db_each_prefix(d, "zz.", collect, got);
	ck(!strcmp(got, "zz.GB=b;zz.NES=a;"), "only the namespace, in key order");

	got[0] = '\0';
	db_set_str(d, "yy..global", "g");
	db_set_str(d, "yy.GB.local", "l");
	db_each_prefix(d, "yy.", collect, got);
	ck(!strcmp(got, "yy..global=g;yy.GB.local=l;"),
	   "an empty tag segment scans like any other");

	/* And the real namespaces carry what the defaults declared. */
	got[0] = '\0';
	db_each_prefix(d, "turbo.", collect, got);
	ck(strstr(got, "turbo.NGPC=x:a~3,y:b~3;") != NULL,
	   "the seeded turbo maps are readable through the same scan");

	got[0] = '\0';
	db_each_prefix(d, "nothing.", collect, got);
	ck(!got[0], "an empty namespace yields nothing rather than everything");
	db_close(d);
}

/* The old screens' strength (src/strength.h): shader_strength.<TAG> beside
 * shader.<TAG>, and shader_strength for every system. A system's own beats
 * MENU's, MENU's beats 100%, a value off the 10% steps is put on one, junk
 * reads 100%, and MENU's row takes every system's own - but not a shader. */
static void shader_strength(void)
{
	db *d;
	char got[512] = { 0 };

	printf("the shaders' strength, per system and for all of them:\n");
	scrub();
	d = db_open(DEV, DB_DEVICE);
	if (!d) { ck(0, "device opens"); return; }
	ck(strength_read(d, "GB") == 100 && strength_read_all(d) == 100,
	   "100% when nothing is kept, today's picture");
	strength_write(d, "GB", 30);
	ck(db_get_int(d, "shader_strength.GB", 0) == 30, "a system's own is shader_strength.<TAG>");
	ck(strength_read(d, "GB") == 30 && strength_read(d, "GBA") == 100,
	   "and is that system's alone");
	db_set_str(d, "shader.GB", "lcd");
	db_set_str(d, "shader.GBA", "scanlines");
	strength_write_all(d, 50);
	ck(db_get_int(d, "shader_strength", 0) == 50, "MENU's is shader_strength");
	ck(!db_has(d, "shader_strength.GB") && strength_read(d, "GB") == 50 && strength_read(d, "GBA") == 50,
	   "MENU's row takes every system's own, and they read MENU's");
	db_each_prefix(d, "shader.", collect, got);
	ck(!strcmp(got, "shader.GB=lcd;shader.GBA=scanlines;"),
	   "and leaves the shaders as they were");
	got[0] = '\0';
	strength_write(d, "GBA", 20);
	db_each_prefix(d, "shader_strength.", collect, got);
	ck(!strcmp(got, "shader_strength.GBA=20;"), "a key MENU's Shader row (shader.) never walks");
	db_set_str(d, "shader_strength.GB", "57");
	ck(strength_read(d, "GB") == 60, "a value off the steps is put on the nearest");
	db_set_str(d, "shader_strength.GB", "strong");
	ck(strength_read(d, "GB") == 50, "junk is not a zero: MENU's is read");
	db_set_str(d, "shader_strength.GB", "0");
	ck(strength_read(d, "GB") == 100, "nor is 0, which reads 100%");
	ck(strength_step(10, -1) == 10 && strength_step(100, 1) == 100 &&
	   strength_step(50, 1) == 60 && strength_step(50, -1) == 40 && strength_step(0, -1) == 90,
	   "a step is 10%, stopping at 10% and 100%");
	got[0] = '\0';
	strength_label(20, got, sizeof got);
	ck(!strcmp(got, "20%"), "and says itself as 20%");
	db_close(d);
}

/* A settings failure must not take the launcher down: launch.sh restarts it,
 * and five exits inside five seconds each makes it call poweroff. So every
 * getter has to survive there being no database at all. */
/* What a power cut can leave of a database: a file SQLite cannot read. It is
 * set aside as NAME.broken and a new one comes in its place, so what is set
 * from then on is kept (2026-11-15: a broken one stayed, and every start was
 * the shipped defaults and the tutorial again). */
static void a_broken_database_is_set_aside(void)
{
	static const char junk[] = "this is not a database, it is what a power cut left";
	char broken[sizeof DEV + sizeof ".broken-wal"], wal[sizeof DEV + sizeof "-wal"];
	char page[4096];
	db *d;
	FILE *f;
	int pass;

	printf("a database a power cut broke is set aside, and a new one made:\n");
	snprintf(broken, sizeof broken, "%s.broken", DEV);
	snprintf(wal, sizeof wal, "%s-wal", DEV);
	for (pass = 0; pass < 2; pass++) {
		scrub();
		unlink(broken);
		f = fopen(DEV, "wb");
		if (!f) { ck(0, "the test writes a broken file"); return; }
		if (pass == 0) {
			fwrite(junk, 1, sizeof junk, f);
		} else {
			/* a whole page of zeros: the size of a database, not its header */
			memset(page, 0, sizeof page);
			fwrite(page, 1, sizeof page, f);
		}
		fclose(f);
		f = fopen(wal, "wb");
		if (f) { fputs("junk", f); fclose(f); }

		/* The language the launcher keeps beside it (src/i18n.h, LANG_FILE) */
		{
			char lang[sizeof test_dir + sizeof "/boot-theme"];
			FILE *l;

			snprintf(lang, sizeof lang, "%s/language", test_dir);
			if ((l = fopen(lang, "w"))) { fputs(pass ? "x\n" : "pt-BR\n", l); fclose(l); }
			/* and the Appearance it notes for the boot screen (boottheme.h) */
			snprintf(lang, sizeof lang, "%s/boot-theme", test_dir);
			if ((l = fopen(lang, "w"))) {
				fputs(pass ? "zytheme=darkish zyzone=-\n" : "zytheme=dark zyzone=Europe/Berlin zylang=pt-BR\n", l);
				fclose(l);
			}
		}

		d = db_open(DEV, DB_DEVICE);
		ck(d != NULL, pass ? "a page of zeros opens as a new database" : "junk opens as a new database");
		ck(!access(broken, F_OK), "the broken file is kept beside it, as .broken");
		ck(db_get_int(d, "volume", -1) == 8, "the new one has the shipped defaults");
		ck(db_get_int(d, "tour.done", 0) == 1, "but not the tutorial: this handheld was in use");
		db_get_str(d, "language", page, sizeof page, "");
		ck(!strcmp(page, pass ? "en" : "pt-BR"),
		   pass ? "a language file that says no code leaves English" : "and the language kept on the card beside it");
		db_get_str(d, "appearance", page, sizeof page, "");
		ck(pass ? !db_has(d, "appearance") : !strcmp(page, "dark"),
		   pass ? "a boot-theme note that says no Appearance leaves the default"
		        : "and the Appearance the boot-theme note keeps");
		ck(db_set_int(d, "volume", 11), "and takes a setting");
		db_close(d);
		d = db_open(DEV, DB_DEVICE);
		ck(db_get_int(d, "volume", -1) == 11, "which is still there at the next open");
		db_close(d);
	}
	unlink(broken);
	snprintf(broken, sizeof broken, "%s.broken-wal", DEV);
	unlink(broken);
	snprintf(broken, sizeof broken, "%s/language", test_dir);
	unlink(broken);
	snprintf(broken, sizeof broken, "%s/boot-theme", test_dir);
	unlink(broken);

	/* A library database too: its own scope, its own schema */
	scrub();
	f = fopen(LIB, "wb");
	if (f) { fwrite(junk, 1, sizeof junk, f); fclose(f); }
	d = db_open(LIB, DB_LIBRARY);
	ck(d != NULL, "a broken library database opens as a new one");
	ck(d && db_get_str(d, "timezone", page, sizeof page, NULL), "with its own defaults");
	db_close(d);
	snprintf(broken, sizeof broken, "%s.broken", LIB);
	ck(!access(broken, F_OK), "and the broken one is kept as .broken");
	unlink(broken);
	scrub();
}

static void no_database_is_survivable(void)
{
	char buf[32];

	printf("with no database open at all:\n");
	db_shutdown();
	ck(db_dev() == NULL && db_lib() == NULL, "the handles are NULL");
	ck(db_get_int(NULL, "volume", 8) == 8, "an integer reads its fallback");
	db_get_str(NULL, "audioout", buf, sizeof buf, "auto");
	ck(!strcmp(buf, "auto"), "a string reads its fallback");
	ck(!db_has(NULL, "volume"), "nothing is present");
	ck(!db_set_int(NULL, "volume", 3), "a write fails rather than crashing");
	ck(!db_write_boot_env(), "and the export declines rather than crashing");
	db_each_prefix(NULL, "turbo.", collect, buf);   /* must simply not run */
}

static void every_default_is_readable(void)
{
	db *d;
	const db_default *def;
	size_t n, i;
	char buf[128];

	printf("every declared default is present and reads back:\n");
	scrub();
	for (int s = 0; s < 2; s++) {
		db_scope sc = s ? DB_LIBRARY : DB_DEVICE;
		d = db_open(s ? LIB : DEV, sc);
		if (!d) { ck(0, "scope opens"); continue; }
		def = db_defaults(sc, &n);
		ck(n > 0, "the scope declares at least one default");
		for (i = 0; i < n; i++) {
			ck(db_has(d, def[i].key), def[i].key);
			db_get_str(d, def[i].key, buf, sizeof buf, "");
			ck(!strcmp(buf, def[i].value), "seeded value matches the table");
		}
		db_close(d);
	}
}

/* --dump must never put a credential on a terminal.
 *
 * It is the repair tool for a device with no sqlite3, so it is run over adb
 * and its output lands in a log, a paste or a screen share. It printed
 * ra.token from the day the account arrived; ss.password would have joined it,
 * and that one is the credential itself rather than something traded for it.
 *
 * Asserted by dumping to a file and searching it for the values, which is the
 * only form of this check that cannot be fooled by the redaction being applied
 * somewhere other than where the printing happens. */
static void a_dump_hides_credentials(void)
{
	static const char *secret = "hunter2-not-a-real-password";
	static const char *plain  = "America/New_York";
	char path[256], buf[8192];
	FILE *f;
	size_t got = 0;

	printf("what --dump puts on a terminal:\n");
	/* Its own database: the case before this one closes them deliberately, and
	 * a dump of nothing would pass every assertion below by being empty. */
	scrub();
	if (!db_init(DEV, LIB, NULL)) { ck(0, "could not open a database to dump"); return; }
	db_set_str(db_dev(), "ra.token", secret);
	db_set_str(db_dev(), "ss.password", secret);
	/* SteamGridDB's API key, typed or pasted: the credential itself too */
	db_set_str(db_dev(), "sgdb.key", "0123456789abcdef0123456789abcdef");
	db_set_str(db_dev(), "ss.user", "someone");
	db_set_str(db_dev(), "timezone", plain);

	snprintf(path, sizeof path, "%s.dump", DEV);
	f = fopen(path, "w+");
	if (!f) { ck(0, "could not write a dump to read back"); return; }
	db_dump(db_dev(), f);
	fflush(f);
	rewind(f);
	got = fread(buf, 1, sizeof buf - 1, f);
	buf[got] = '\0';
	fclose(f);
	unlink(path);

	ck(strstr(buf, secret) == NULL, "no secret value appears anywhere in it");
	ck(strstr(buf, "ra.token") != NULL, "the token's KEY is still listed");
	ck(strstr(buf, "ss.password") != NULL, "and so is the password's");
	ck(strstr(buf, "0123456789abcdef") == NULL, "nor any part of the SteamGridDB key");
	ck(strstr(buf, "sgdb.key") != NULL, "whose KEY is listed too");
	ck(strstr(buf, "[hidden,") != NULL, "with a length in place of the value");
	/* A user name is not a secret, and hiding it would make the dump useless
	 * for the question it is usually opened to answer: which account is this. */
	ck(strstr(buf, "someone") != NULL, "an account NAME is still shown");
	ck(strstr(buf, plain) != NULL, "and an ordinary setting is untouched");
}

/* One connection, two threads at once, as at every start: the scan thread
 * (main.c, scan_thread) reads the settings while the main thread does. On a
 * SQLite that is multi-thread rather than serialized - macOS's, so this
 * Mac's and CI's - that corrupted the connection's own allocator, and a
 * shot crashed in db_get_str (2026-11-07). db_open asks for a serialized
 * connection; with it this passes, without it this crashes or reads wrong
 * within a few thousand reads. */
#define READS 20000

static void *reader(void *arg)
{
	db *d = arg;
	long wrong = 0;
	char buf[64];

	for (int i = 0; i < READS; i++) {
		db_get_str(d, "audioout", buf, sizeof buf, "");
		if (strcmp(buf, "auto") != 0) wrong++;
	}
	return (void *)wrong;
}

static void one_connection_two_threads(void)
{
	pthread_t t;
	void *theirs = NULL;
	long mine;
	db *d;

	printf("one connection read from two threads at once:\n");
	d = db_open(DEV, DB_DEVICE);
	ck(d != NULL, "it opens");
	if (!d) return;
	ck(pthread_create(&t, NULL, reader, d) == 0, "a second thread starts");
	mine = (long)reader(d);
	pthread_join(t, &theirs);
	ck(mine == 0 && (long)theirs == 0, "every read on both threads comes back right");
	db_close(d);
}

int main(void)
{
	if (!mkdtemp(test_dir)) { perror("mkdtemp"); return 1; }
	snprintf(DEV, sizeof DEV, "%s/device.db", test_dir);
	snprintf(LIB, sizeof LIB, "%s/library.db", test_dir);
	snprintf(BOOT, sizeof BOOT, "%s/boot.env", test_dir);

	scrub();
	opens_and_seeds();
	a_default_never_beats_a_choice();
	round_trips();
	a_lost_database_self_heals();
	a_broken_database_is_set_aside();
	the_games_table();
	the_scopes_stay_apart();
	boot_env_says_what_the_shell_needs();
	prefix_scan();
	shader_strength();
	no_database_is_survivable();
	every_default_is_readable();
	a_dump_hides_credentials();
	one_connection_two_threads();
	cleanup();

	if (fails) { printf("\n%d FAILED\n", fails); return 1; }
	printf("\nok: the settings store keeps its promises\n");
	return 0;
}
