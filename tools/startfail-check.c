/* SPDX-License-Identifier: GPL-3.0-only */
/* What the shelf says when a core would not load (src/startfail.h).
 *
 * Diatom answers a RUN whose core dlopen refused with ERROR code=core_load and
 * why= machine, library, version, symbol or other, and the name dlerror gave
 * (diatom/src/corefail.h). A core a person added (bsnes, MAME 2010: Davistoa,
 * Discord, 2026-11-18) sent the game straight back to the shelf with nothing
 * said, and the log held the reason.
 *
 * This holds, in each of the eight languages: each why says the core's file
 * name, and the library's or the function's where there is one, in its own
 * words; no conversion left in the text, no name cut where it fits, a name
 * too long for the panel cut with an ellipsis, and one that is not an error
 * of this kind left to say nothing. And the ERROR line's reading is
 * tools/resident-check.c's.
 *
 * Links src/startfail.c and the strings, and NOT SDL. */
#include "../src/i18n.h"
#include "../src/startfail.h"

#include <stdio.h>
#include <string.h>

static int fails;

static void ck(int cond, const char *what)
{
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

static plat_start_error err(const char *why, const char *name, const char *version)
{
	plat_start_error e;

	memset(&e, 0, sizeof e);
	snprintf(e.code, sizeof e.code, "core_load");
	snprintf(e.why, sizeof e.why, "%s", why);
	snprintf(e.name, sizeof e.name, "%s", name);
	snprintf(e.version, sizeof e.version, "%s", version);
	snprintf(e.core, sizeof e.core, "/mnt/SDCARD/ZyperOS/cores/bsnes_libretro.so");
	return e;
}

int main(void)
{
	char msg[512], what[200];
	plat_start_error e;
	int l;

	/* ---- English, exactly ---- */
	e = err("machine", "", "");
	ck(startfail_message(&e, msg, sizeof msg) &&
	   !strcmp(msg, "bsnes_libretro.so could not be loaded: it is not a 64-bit ARM Linux build"),
	   "English: another machine");
	e = err("library", "libfoo.so.2", "");
	ck(startfail_message(&e, msg, sizeof msg) &&
	   !strcmp(msg, "bsnes_libretro.so could not be loaded: it needs libfoo.so.2, which ZyperOS does not have"),
	   "English: a library it needs");
	e = err("version", "libstdc++.so.6", "GLIBCXX_3.4.30");
	ck(startfail_message(&e, msg, sizeof msg) &&
	   !strcmp(msg, "bsnes_libretro.so could not be loaded: it needs libstdc++.so.6 (GLIBCXX_3.4.30), "
	                "which ZyperOS does not have"),
	   "English: a library that is too old");
	e = err("symbol", "_ZN5bsnes3fooEv", "");
	ck(startfail_message(&e, msg, sizeof msg) &&
	   !strcmp(msg, "bsnes_libretro.so could not be loaded: it calls _ZN5bsnes3fooEv, which ZyperOS does not have"),
	   "English: a function it calls");
	e = err("other", "", "");
	ck(startfail_message(&e, msg, sizeof msg) && !strcmp(msg, "bsnes_libretro.so could not be loaded"),
	   "English: nothing more known");
	e = err("something new", "", "");
	ck(startfail_message(&e, msg, sizeof msg) && !strcmp(msg, "bsnes_libretro.so could not be loaded"),
	   "a why this does not know: the plain one, so a newer Diatom's word is still said");

	/* A library or a function it is not told: the plain one, never a message with a hole in it */
	e = err("library", "", "");
	ck(startfail_message(&e, msg, sizeof msg) && !strcmp(msg, "bsnes_libretro.so could not be loaded"),
	   "a library with no name: the plain one");
	e = err("symbol", "", "");
	ck(startfail_message(&e, msg, sizeof msg) && !strcmp(msg, "bsnes_libretro.so could not be loaded"),
	   "a function with no name: the plain one");

	/* ---- not an error of this kind ---- */
	memset(&e, 0, sizeof e);
	ck(!startfail_message(&e, msg, sizeof msg) && msg[0] == '\0', "no error: nothing said");
	e = err("machine", "", "");
	snprintf(e.code, sizeof e.code, "core_missing");
	ck(!startfail_message(&e, msg, sizeof msg), "an error of another code: left to the log");
	e = err("machine", "", "");
	e.core[0] = '\0';
	ck(startfail_message(&e, msg, sizeof msg) && strstr(msg, "could not be loaded"), "no path: still said");

	/* ---- a long name: cut, with an ellipsis ---- */
	e = err("symbol", "_ZN5bsnes7SuperFamicom3SMP4readEjNSt3__16vectorIhNS1_9allocatorIhEEEE", "");
	ck(startfail_message(&e, msg, sizeof msg) && strstr(msg, "...") && !strstr(msg, "allocatorIhEEEE"),
	   "a mangled name of seventy characters: cut with an ellipsis");
	e = err("library", "libshort.so", "");
	ck(startfail_message(&e, msg, sizeof msg) && !strstr(msg, "..."), "a short one: not cut");

	/* ---- every language says each, with the names in, no conversion left ---- */
	for (l = 0; l < LANG_COUNT; l++) {
		static const char *const whys[] = { "machine", "library", "version", "symbol", "other" };
		int i;

		i18n_set((lang)l);
		for (i = 0; i < 5; i++) {
			e = err(whys[i], "libfoo.so.2", "GLIBCXX_3.4.30");
			snprintf(what, sizeof what, "%s, %s: said", lang_code((lang)l), whys[i]);
			ck(startfail_message(&e, msg, sizeof msg), what);
			snprintf(what, sizeof what, "%s, %s: the core's file name is in it", lang_code((lang)l), whys[i]);
			ck(strstr(msg, "bsnes_libretro.so") != NULL, what);
			snprintf(what, sizeof what, "%s, %s: no conversion left (%s)", lang_code((lang)l), whys[i], msg);
			ck(strchr(msg, '%') == NULL, what);
			if (!strcmp(whys[i], "library") || !strcmp(whys[i], "version")) {
				snprintf(what, sizeof what, "%s, %s: the library's name is in it", lang_code((lang)l), whys[i]);
				ck(strstr(msg, "libfoo.so.2") != NULL, what);
			}
			if (!strcmp(whys[i], "version")) {
				snprintf(what, sizeof what, "%s, version: the version is in it", lang_code((lang)l));
				ck(strstr(msg, "GLIBCXX_3.4.30") != NULL, what);
			}
			if (l != LANG_EN && strcmp(whys[i], "other")) {
				char en[512];
				plat_start_error ee = err(whys[i], "libfoo.so.2", "GLIBCXX_3.4.30");

				i18n_set(LANG_EN);
				startfail_message(&ee, en, sizeof en);
				i18n_set((lang)l);
				snprintf(what, sizeof what, "%s, %s: in its own words, not English's", lang_code((lang)l), whys[i]);
				ck(strcmp(en, msg) != 0, what);
			}
		}
	}
	i18n_set(LANG_EN);

	/* French's no-break space before its colon, as the glossary has it */
	i18n_set(LANG_FR);
	e = err("library", "libfoo.so.2", "");
	startfail_message(&e, msg, sizeof msg);
	ck(strstr(msg, "charg\xc3\xa9\xc2\xa0: il a besoin de libfoo.so.2") != NULL, "French: a no-break space before the colon");
	i18n_set(LANG_EN);

	if (fails) { printf("startfail-check: %d FAILED\n", fails); return 1; }
	printf("ok: a core that would not load is said in eight languages - another machine, a library, a library too old, "
	       "a function - with its file's name and the one dlerror gave\n");
	return 0;
}
